#include "PCH.h"
#include "VulkanRenderer.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include <Core/Paths.h>

#include <vulkan/vk_enum_string_helper.h>

#include <span>

namespace DA {
    namespace {
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
    } // namespace

    std::unique_ptr<VulkanRenderer> VulkanRenderer::Create(const VulkanDevice& device, VulkanSwapchain& swapchain) {
        std::unique_ptr<VulkanRenderer> renderer(new VulkanRenderer(device, swapchain));
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

        VulkanGraphicsPipelineDesc pipelineDesc;
        pipelineDesc.VertexShader = Paths::ResolveEngine("Shaders/Triangle.vert.spv");
        pipelineDesc.FragmentShader = Paths::ResolveEngine("Shaders/Triangle.frag.spv");
        pipelineDesc.ColorFormat = m_swapchain.GetFormat();
        m_trianglePipeline = VulkanGraphicsPipeline::Create(m_device, pipelineDesc);
        if (!m_trianglePipeline) return false;

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
        // Draw calls go here (the triangle is next).
        m_trianglePipeline->Bind(cmd);

        const VkViewport viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height),
                                  0.0f, 1.0f};
        const VkRect2D scissor{{0, 0}, extent};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdDraw(cmd, 3, 1, 0, 0);
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
