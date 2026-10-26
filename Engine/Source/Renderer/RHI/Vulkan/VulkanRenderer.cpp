#include "PCH.h"
#include "VulkanRenderer.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanAllocator.h"
#include <Core/Paths.h>
#include <Core/Log.h>
#include <Renderer/ImageLoader.h>

#include <vulkan/vk_enum_string_helper.h>

#include <span>
#include <cmath>

namespace DA {
    namespace {
        struct Vertex {
            float Position[2];
            float Color[3];
            float UV[2];
        };

        struct FrameUniforms {
            float Time;
            float Aspect;
            float Pad[2];
        };

        struct PushConstants {
            float Offset[2];
            float Angle;
            float Pad;
        };

        void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                             VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                             VkAccessFlags2 dstAccess) {
            VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
            barrier.srcStageMask = srcStage;
            barrier.srcAccessMask = srcAccess;
            barrier.dstStageMask = dstStage;
            barrier.dstAccessMask = dstAccess;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

            VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            dependency.imageMemoryBarrierCount = 1;
            dependency.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(cmd, &dependency);
        }

        // 64x64 BGRA checkerboard, used when the texture file is missing.
        ImageData MakeCheckerboard() {
            ImageData data;
            data.Width = 64;
            data.Height = 64;
            data.Pixels.resize(64 * 64 * 4);
            for (uint32_t y = 0; y < 64; ++y) {
                for (uint32_t x = 0; x < 64; ++x) {
                    const uint8_t v = (((x / 8) + (y / 8)) % 2 == 0) ? 230 : 60;
                    uint8_t* p = &data.Pixels[(y * 64 + x) * 4];
                    p[0] = v;
                    p[1] = v;
                    p[2] = v;
                    p[3] = 255;
                }
            }
            return data;
        }
    } // namespace

    std::unique_ptr<VulkanRenderer> VulkanRenderer::Create(const VulkanDevice& device, const VulkanAllocator& allocator,
                                                           VulkanSwapchain& swapchain) {
        std::unique_ptr<VulkanRenderer> renderer(new VulkanRenderer(device, allocator, swapchain));
        if (!renderer->Init()) return nullptr;
        return renderer;
    }

    bool VulkanRenderer::Init() {
        for (Frame& frame : m_frames) {
            // The pool is reset as a whole every frame, so TRANSIENT is enough (no per-buffer reset flag).
            frame.Pool =
                VulkanCommandPool::Create(m_device, m_device.GetGraphicsFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
            if (!frame.Pool) return false;

            frame.Cmd = VulkanCommandBuffer::Create(m_device, *frame.Pool);
            frame.ImageAvailable = VulkanSemaphore::Create(m_device);
            frame.InFlight = VulkanFence::Create(m_device, /*signaled*/ true);
            if (!frame.Cmd || !frame.ImageAvailable || !frame.InFlight) return false;
        }
        m_requestedWidth = m_swapchain.GetExtent().width;
        m_requestedHeight = m_swapchain.GetExtent().height;

        // Texture + sampler (stb gives RGBA, so the format is R8G8B8A8_SRGB).
        std::optional<ImageData> image = LoadImageFile(Paths::ResolveEngine("Textures/Vulkan_Logo.png"));
        if (!image) {
            DA_WARN("Using the checkboard fallback texture.");
            image = MakeCheckerboard();
        }
        m_texture = VulkanImage::CreateTexture2D(m_device, m_allocator, VK_FORMAT_R8G8B8A8_SRGB, image->Width,
                                                 image->Height, image->Pixels.data());
        m_sampler = VulkanSampler::Create(m_device);
        if (!m_texture || !m_sampler) return false;

        // Descriptors: binding 0 = per-frame uniforms, binding 1 = texture.
        const std::array<VkDescriptorSetLayoutBinding, 2> bindings = {{
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
              nullptr},
             {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        }};
        m_setLayout = VulkanDescriptorSetLayout::Create(m_device, bindings);
        if (!m_setLayout) return false;

        const std::array<VkDescriptorPoolSize, 2> poolSizes = {{
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, FramesInFlight},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, FramesInFlight},
        }};
        m_descriptorPool = VulkanDescriptorPool::Create(m_device, FramesInFlight, poolSizes);
        if (!m_descriptorPool) return false;

        for (Frame& frame : m_frames) {
            frame.Uniform =
                VulkanBuffer::CreateHostVisible(m_allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(FrameUniforms));
            frame.Set = m_descriptorPool->Allocate(m_setLayout->GetHandle());
            if (!frame.Uniform || frame.Set == VK_NULL_HANDLE) return false;
            WriteUniformBufferDescriptor(m_device, frame.Set, 0, frame.Uniform->GetHandle(), sizeof(FrameUniforms));
            WriteCombinedImageSamplerDescriptor(m_device, frame.Set, 1, m_texture->GetView(), m_sampler->GetHandle());
        }

        const Vertex vertices[] = {
            {{-0.5f, -0.5f}, {1.0f, 0.2f, 0.2f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f}, {0.2f, 1.0f, 0.2f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f}, {0.3f, 0.4f, 1.0f}, {1.0f, 1.0f}},
            {{-0.5f, 0.5f}, {1.0f, 1.0f, 0.2f}, {0.0f, 1.0f}},
        };
        const uint16_t indices[] = {0, 1, 2, 2, 3, 0};

        m_vertexBuffer = VulkanBuffer::CreateDeviceLocal(m_device, m_allocator, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                         vertices, sizeof(vertices));
        m_indexBuffer = VulkanBuffer::CreateDeviceLocal(m_device, m_allocator, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                        indices, sizeof(indices));
        if (!m_vertexBuffer || !m_indexBuffer) return false;

        VulkanGraphicsPipelineDesc pipelineDesc;
        pipelineDesc.VertexShader = Paths::ResolveEngine("Shaders/Quad.vert.spv");
        pipelineDesc.FragmentShader = Paths::ResolveEngine("Shaders/Quad.frag.spv");
        pipelineDesc.ColorFormat = m_swapchain.GetFormat();
        pipelineDesc.VertexBindings = {{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}};
        pipelineDesc.VertexAttributes = {
            {0, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, Position))},
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, Color))},
            {2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, UV))},
        };
        pipelineDesc.PushConstants = {{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants)}};
        pipelineDesc.SetLayouts = {m_setLayout->GetHandle()};
        m_quadPipeline = VulkanGraphicsPipeline::Create(m_device, pipelineDesc);
        if (!m_quadPipeline) return false;

        m_clock.Reset();
        return CreateRenderFinishedSemaphores();
    }

    bool VulkanRenderer::CreateRenderFinishedSemaphores() {
        m_renderFinished.clear(); // callers guarantee the GPU is idle
        for (uint32_t i = 0; i < m_swapchain.GetImageCount(); ++i) {
            auto semaphore = VulkanSemaphore::Create(m_device);
            if (!semaphore) return false;
            m_renderFinished.push_back(std::move(semaphore));
        }
        return true;
    }

    bool VulkanRenderer::Recreate(uint32_t width, uint32_t height) {
        if (!m_swapchain.Recreate(width, height)) { // waits for the GPU internally
            m_needsRecreate = true;                 // try again next frame
            return false;
        }
        m_requestedWidth = width;
        m_requestedHeight = height;
        m_needsRecreate = false;

        // Always rebuild: a failed present can leave a semaphore signaled, and the image count may change.
        return CreateRenderFinishedSemaphores();
    }

    bool VulkanRenderer::RenderFrame(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) return true; // minimized: nothing to draw

        if (m_needsRecreate || width != m_requestedWidth || height != m_requestedHeight) {
            if (!Recreate(width, height)) return true; // skip this frame, retry next one
        }

        Frame& frame = m_frames[m_frameIndex];

        if (!frame.InFlight->Wait()) {
            DA_ERROR("Waiting for the in-flight fence failed.");
            return false;
        }

        uint32_t imageIndex = 0;
        VkResult result = m_swapchain.AcquireNextImage(frame.ImageAvailable->GetHandle(), imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            m_needsRecreate = true;
            return true;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            DA_ERROR("vkAcquireNextImageKHR failed: {}", string_VkResult(result));
            return false;
        }
        if (result == VK_SUBOPTIMAL_KHR) m_needsRecreate = true; // usable now, rebuild next frame

        if (!frame.InFlight->Reset()) {
            DA_ERROR("Resetting the in-flight fence failed.");
            return false;
        }
        if (!frame.Pool->Reset()) return false;
        if (!frame.Cmd->Begin()) return false;
        Record(frame.Cmd->GetHandle(), imageIndex);

        const VkSemaphoreSubmitInfo wait =
            MakeSemaphoreSubmitInfo(frame.ImageAvailable->GetHandle(), VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
        const VkSemaphoreSubmitInfo signal =
            MakeSemaphoreSubmitInfo(m_renderFinished[imageIndex]->GetHandle(), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
        result = frame.Cmd->Submit(m_device.GetGraphicsQueue(), std::span(&wait, 1), std::span(&signal, 1),
                                   frame.InFlight->GetHandle());
        if (result != VK_SUCCESS) {
            DA_ERROR("vkQueueSubmit2 failed: {}", string_VkResult(result));
            return false;
        }

        result = m_swapchain.Present(m_device.GetPresentQueue(), imageIndex, m_renderFinished[imageIndex]->GetHandle());
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            m_needsRecreate = true;
        } else if (result != VK_SUCCESS) {
            DA_ERROR("vkQueuePresentKHR failed: {}", string_VkResult(result));
            return false;
        }
        m_frameIndex = (m_frameIndex + 1) % FramesInFlight;
        return true;
    }

    void VulkanRenderer::Record(VkCommandBuffer cmd, uint32_t imageIndex) {
        const VkImage image = m_swapchain.GetImage(imageIndex);
        const VkExtent2D extent = m_swapchain.GetExtent();

        // UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL. The source stage matches the semaphore wait stage,
        // so the transition cannot run before the image is actually available.
        TransitionImage(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

        VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        color.imageView = m_swapchain.GetImageView(imageIndex);
        color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.clearValue.color = {{m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]}};

        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
        rendering.renderArea = {{0, 0}, extent};
        rendering.layerCount = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments = &color;

        vkCmdBeginRendering(cmd, &rendering);

        m_quadPipeline->Bind(cmd);

        const VkViewport viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height),
                                  0.0f, 1.0f};
        const VkRect2D scissor{{0, 0}, extent};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        Frame& frame = m_frames[m_frameIndex];
        const float t = static_cast<float>(m_clock.GetElapsedSeconds());
        const FrameUniforms uniforms{
            t, static_cast<float>(extent.width) / static_cast<float>(extent.height), {0.0f, 0.0f}};
        frame.Uniform->Write(&uniforms, sizeof(uniforms));
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_quadPipeline->GetLayout(), 0, 1, &frame.Set, 0,
                                nullptr);
        PushConstants push{};
        push.Offset[0] = 0.3f * std::sin(t);
        push.Angle = t;
        vkCmdPushConstants(cmd, m_quadPipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);

        const VkBuffer vertexBuffer = m_vertexBuffer->GetHandle();
        const VkDeviceSize vertexOffset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &vertexOffset);
        vkCmdBindIndexBuffer(cmd, m_indexBuffer->GetHandle(), 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
        vkCmdEndRendering(cmd);

        // COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC. No destination stage: the semaphore signal covers visibility.
        TransitionImage(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    }

    VulkanRenderer::~VulkanRenderer() {
        // Nothing may still be using the semaphores, fences or command buffers when they are destroyed.
        m_device.WaitIdle();
    }
} // namespace DA
