#include "PCH.h"
#include "VulkanRenderer.h"
#include <Renderer/RHI/Vulkan/Core/VulkanDevice.h>
#include <Renderer/RHI/Vulkan/Memory/VulkanAllocator.h>
#include <Renderer/RHI/Vulkan/Presentation/VulkanSwapchain.h>
#include <Renderer/Resources/Bitmap.h>
#include <Core/Log.h>
#include <Core/Paths.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vk_enum_string_helper.h>

#include <algorithm>
#include <array>
#include <optional>
#include <span>

namespace DA {
    namespace {
        // Must match FrameData in Cube.vert/.frag (std140, 144 bytes).
        struct FrameUniforms {
            glm::mat4 View;
            glm::mat4 Proj;
            glm::vec4 CameraPos;
            float Time;
            float Pad[3];
        };

        // Must match Push in Cube.vert (64 bytes).
        struct PushConstants {
            glm::mat4 Model;
            glm::vec4 BaseColor;
            glm::vec4 Emissive;
            float Matallic;
            float Roughness;
            float AlphaCutoff;
            uint32_t Flags;
        };
        static_assert(sizeof(PushConstants) == 112);

        void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                             VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                             VkAccessFlags2 dstAccess, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT) {
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
            barrier.subresourceRange = {aspect, 0, 1, 0, 1};

            VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            dependency.imageMemoryBarrierCount = 1;
            dependency.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(cmd, &dependency);
        }

        // 64x64 RGBA checkerboard, used when the texture file is missing.
        Bitmap MakeCheckerboard() {
            Bitmap bmp(64, 64);
            for (uint32_t y = 0; y < 64; ++y)
                for (uint32_t x = 0; x < 64; ++x) {
                    const uint8_t v = (((x / 8) + (y / 8)) % 2 == 0) ? 230 : 60;
                    uint8_t* p = bmp.Pixel8(x, y);
                    p[0] = p[1] = p[2] = v;
                    p[3] = 255;
                }
            return bmp;
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
            // The pool is reset as a whole every frame, so TRANSIENT is enough.
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
        std::optional<Bitmap> image = Bitmap::FromFile(Paths::ResolveEngine("Textures/Vulkan_Logo.png"));
        if (!image) {
            DA_WARN("Using the checkerboard fallback texture.");
            image = MakeCheckerboard();
        }
        m_texture = VulkanImage::CreateTexture2D(m_device, m_allocator, VK_FORMAT_R8G8B8A8_SRGB, image->GetWidth(),
                                                 image->GetHeight(), image->Data());
        m_sampler = VulkanSampler::Create(m_device);
        if (!m_texture || !m_sampler) return false;

        const uint8_t white[4] = {255, 255, 255, 255};
        m_whiteTexture = VulkanImage::CreateTexture2D(m_device, m_allocator, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, white);
        if (!m_whiteTexture) return false;

        // Depth image (created before the pipeline, which needs its format).
        m_depth = VulkanImage::CreateDepth(m_device, m_allocator, m_swapchain.GetExtent().width,
                                           m_swapchain.GetExtent().height);
        if (!m_depth) return false;

        // Descriptors: binding 0 = per-frame uniforms, binding 1 = texture.
        const VkDescriptorSetLayoutBinding frameBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                                        nullptr};
        m_setLayout = VulkanDescriptorSetLayout::Create(m_device, std::span(&frameBinding, 1));

        std::array<VkDescriptorSetLayoutBinding, VulkanMaterial::SlotCount> materialBindings{};
        for (uint32_t i = 0; i < VulkanMaterial::SlotCount; ++i)
            materialBindings[i] = {i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
                                   nullptr};
        m_materialLayout = VulkanDescriptorSetLayout::Create(m_device, materialBindings);
        if (!m_setLayout || !m_materialLayout) return false;

        const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, FramesInFlight};
        m_descriptorPool = VulkanDescriptorPool::Create(m_device, FramesInFlight, std::span(&poolSize, 1));
        if (!m_descriptorPool) return false;

        for (Frame& frame : m_frames) {
            frame.Uniform =
                VulkanBuffer::CreateHostVisible(m_allocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(FrameUniforms));
            frame.Set = m_descriptorPool->Allocate(m_setLayout->GetHandle());
            if (!frame.Uniform || frame.Set == VK_NULL_HANDLE) return false;
            WriteUniformBufferDescriptor(m_device, frame.Set, 0, frame.Uniform->GetHandle(), sizeof(FrameUniforms));
        }

        VulkanGraphicsPipelineDesc pipelineDesc;
        pipelineDesc.VertexShader = Paths::ResolveShader("Lit.vert");
        pipelineDesc.FragmentShader = Paths::ResolveShader("Lit.frag");
        pipelineDesc.ColorFormat = m_swapchain.GetFormat();
        pipelineDesc.DepthFormat = m_depth->GetFormat();
        pipelineDesc.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pipelineDesc.VertexBindings = {VertexBinding()};
        pipelineDesc.VertexAttributes = VertexAttributes(); // plain assignment: it already returns a vector
        pipelineDesc.PushConstants = {
            {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants)}};
        pipelineDesc.SetLayouts = {m_setLayout->GetHandle(), m_materialLayout->GetHandle()};
        m_cubePipeline = VulkanGraphicsPipeline::Create(m_device, pipelineDesc);
        pipelineDesc.CullMode = VK_CULL_MODE_NONE;
        m_doubleSidedPipeline = VulkanGraphicsPipeline::Create(m_device, pipelineDesc);
        if (!m_cubePipeline || !m_doubleSidedPipeline) return false;

        // Default material: the logo texture, so cubes/planes and untextured models still draw.
        MaterialParams defaultParams;
        std::array<std::shared_ptr<VulkanImage>, VulkanMaterial::SlotCount> defaultTextures{};
        defaultTextures[VulkanMaterial::BaseColor] = m_texture;
        m_defaultMaterial = CreateMaterial(defaultParams, defaultTextures, false);
        if (!m_defaultMaterial) return false;

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

        // The GPU is idle (the swapchain waited), so the old depth image can be replaced safely.
        m_depth = VulkanImage::CreateDepth(m_device, m_allocator, m_swapchain.GetExtent().width,
                                           m_swapchain.GetExtent().height);
        if (!m_depth) return false;

        // Always rebuild: a failed present can leave a semaphore signaled, and the image count may change.
        return CreateRenderFinishedSemaphores();
    }

    VkDescriptorSet VulkanRenderer::AllocateMaterialSet() {
        constexpr uint32_t SetsPerPool = 256;
        if (m_materialPools.empty() || m_materialPoolUsed >= SetsPerPool) {
            const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                            SetsPerPool * VulkanMaterial::SlotCount};
            auto pool = VulkanDescriptorPool::Create(m_device, SetsPerPool, std::span(&size, 1));
            if (!pool) return VK_NULL_HANDLE;
            m_materialPools.push_back(std::move(pool));
            m_materialPoolUsed = 0;
        }
        ++m_materialPoolUsed;
        return m_materialPools.back()->Allocate(m_materialLayout->GetHandle());
    }

    std::unique_ptr<VulkanMesh> VulkanRenderer::CreateMesh(const MeshData& data) const {
        return VulkanMesh::Create(m_device, m_allocator, data.Vertices, data.Indices);
    }

    std::unique_ptr<VulkanMaterial> VulkanRenderer::CreateMaterial(
        const MaterialParams& params,
        const std::array<std::shared_ptr<VulkanImage>, VulkanMaterial::SlotCount>& textures, bool doubleSided) {
        auto material = std::make_unique<VulkanMaterial>();
        material->Params = params;
        material->DoubleSided = doubleSided;
        material->Set = AllocateMaterialSet();
        if (material->Set == VK_NULL_HANDLE) return nullptr;
        for (uint32_t i = 0; i < VulkanMaterial::SlotCount; ++i) {
            material->Textures[i] = textures[i] ? textures[i] : m_whiteTexture;
            WriteCombinedImageSamplerDescriptor(m_device, material->Set, i, material->Textures[i]->GetView(),
                                                m_sampler->GetHandle());
        }
        return material;
    }

    std::unique_ptr<VulkanModel> VulkanRenderer::CreateModel(ModelData&& data, const std::string& sourcePath) {
        auto loadTexture = [&](int32_t image, bool srgb) -> std::shared_ptr<VulkanImage> {
            if (image < 0 || static_cast<size_t>(image) >= data.Images.size()) return nullptr;
            const std::string key = sourcePath + "#" + std::to_string(image) + (srgb ? "|srgb" : "|linear");
            return m_textureCache.GetOrCreate(key, [&]() -> std::shared_ptr<VulkanImage> {
                const ImageSourceData& src = data.Images[image];
                auto pixels =
                    src.Encoded.empty() ? Bitmap::FromFile(src.Path) : Bitmap::FromMemory(src.Encoded, src.Name);
                if (!pixels) return nullptr;
                return VulkanImage::CreateTexture2D(m_device, m_allocator,
                                                    srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
                                                    pixels->GetWidth(), pixels->GetHeight(), pixels->Data());
            });
        };

        std::vector<std::unique_ptr<VulkanMaterial>> materials;
        materials.reserve(data.Materials.size());
        for (const MaterialData& md : data.Materials) {
            std::array<std::shared_ptr<VulkanImage>, VulkanMaterial::SlotCount> tex{};
            tex[VulkanMaterial::BaseColor] = loadTexture(md.BaseColorImage, true);
            tex[VulkanMaterial::Normal] = loadTexture(md.NormalImage, false);
            tex[VulkanMaterial::MetallicRoughness] = loadTexture(md.MetallicRoughnessImage, false);
            tex[VulkanMaterial::Occlusion] = loadTexture(md.OcclusionImage, false);
            tex[VulkanMaterial::Emissive] = loadTexture(md.EmissiveImage, true);

            MaterialParams p;
            p.BaseColor = md.BaseColorFactor;
            p.Emissive = glm::vec4(md.EmissiveFactor, 0.0f);
            p.Metallic = md.Metallic;
            p.Roughness = md.Roughness;
            p.AlphaCutoff = md.AlphaCutoff;
            if (tex[VulkanMaterial::Normal]) p.Flags |= MaterialParams::FlagNormalMap;
            // Blend is approximated by a cutout until there is a transparent pass.
            if (md.Alpha != EAlphaMode::EAM_OPAQUE) p.Flags |= MaterialParams::FlagAlphaMask;

            auto material = CreateMaterial(p, tex, md.DoubleSided);
            if (!material) return nullptr;
            materials.push_back(std::move(material));
        }
        return VulkanModel::Create(m_device, m_allocator, std::move(data), std::move(materials));
    }

    bool VulkanRenderer::RenderFrame(uint32_t width, uint32_t height) {
        // Whatever happens below (early return, minimize, error), this frame's submissions are discarded.
        struct DrawListReset {
            std::vector<DrawItem>& List;
            ~DrawListReset() { List.clear(); }
        } drawListReset{m_drawList};

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

        // Reset the fence only now that we know we will submit.
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

        // Swapchain image: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL. The source stage matches the semaphore wait
        // stage, so the transition cannot run before the image is actually available.
        TransitionImage(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

        // Depth: contents are cleared every frame, so UNDEFINED is fine. The fragment-test stages on both sides
        // order this frame's depth use after the previous frame's (the image is shared by the frames in flight).
        TransitionImage(cmd, m_depth->GetHandle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        m_depth->GetAspect());

        VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        color.imageView = m_swapchain.GetImageView(imageIndex);
        color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.clearValue.color = {{m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]}};

        VkRenderingAttachmentInfo depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        depth.imageView = m_depth->GetView();
        depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.clearValue.depthStencil = {1.0f, 0};

        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
        rendering.renderArea = {{0, 0}, extent};
        rendering.layerCount = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments = &color;
        rendering.pDepthAttachment = &depth;

        vkCmdBeginRendering(cmd, &rendering);

        const VkViewport viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height),
                                  0.0f, 1.0f};
        const VkRect2D scissor{{0, 0}, extent};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // m_frameIndex has not advanced yet; the fence wait guarantees the GPU is done with this slot.
        Frame& frame = m_frames[m_frameIndex];
        const float t = static_cast<float>(m_clock.GetElapsedSeconds());
        const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);

        FrameUniforms uniforms{};
        uniforms.View = m_camera.GetViewMatrix();
        uniforms.Proj = m_camera.GetProjectionMatrix(aspect);
        uniforms.Time = t;
        uniforms.CameraPos = glm::vec4(m_camera.GetPosition(), 1.0f);
        frame.Uniform->Write(&uniforms, sizeof(uniforms));
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_cubePipeline->GetLayout(), 0, 1, &frame.Set, 0,
                                nullptr);

        const VulkanGraphicsPipeline* boundPipeline = nullptr;
        const VulkanMaterial* boundMaterial = nullptr;
        const VulkanMesh* boundMesh = nullptr;
        for (const DrawItem& item : m_drawList) {
            if (!item.Mesh) continue;
            const auto* mat = item.Material ? item.Material : m_defaultMaterial.get();
            const auto* pipeline = mat->DoubleSided ? m_doubleSidedPipeline.get() : m_cubePipeline.get();
            if (pipeline != boundPipeline) {
                pipeline->Bind(cmd);
                boundPipeline = pipeline;
            }
            if (mat != boundMaterial) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout(), 1, 1, &mat->Set, 0,
                                        nullptr);
                boundMaterial = mat;
            }
            const auto& mp = mat->Params;
            const PushConstants push{item.Model,   mp.BaseColor,   mp.Emissive, mp.Metallic,
                                      mp.Roughness, mp.AlphaCutoff, mp.Flags};
            vkCmdPushConstants(cmd, pipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(push), &push);
            if (item.Mesh != boundMesh) {
                item.Mesh->Bind(cmd);
                boundMesh = item.Mesh;
            }
            item.Mesh->Draw(cmd);
        }

        vkCmdEndRendering(cmd);

        // COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC. No destination stage: the semaphore signal covers visibility.
        TransitionImage(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    }

    VulkanRenderer::~VulkanRenderer() {
        // Nothing may still be using the semaphores, fences, command buffers or descriptors when they are
        // destroyed.
        m_device.WaitIdle();
    }
} // namespace DA
