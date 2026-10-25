#pragma once

#include <Core/DA_API.h>
#include "VulkanCommandBuffer.h"
#include "VulkanCommandPool.h"
#include "VulkanSync.h"
#include "VulkanGraphicsPipeline.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace DA {
    class VulkanDevice;
    class VulkanSwapchain;

    // Owns the per-frame objects and runs the acquire -> record -> submit -> present loop.
    class DA_API VulkanRenderer {
    public:
        static constexpr uint32_t FramesInFlight = 2;

        // The device and swapchain must outlive the renderer (declare the renderer after them).
        static std::unique_ptr<VulkanRenderer> Create(const VulkanDevice& device, VulkanSwapchain& swapchain);
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;

        // Renders one frame. Handles resize and minimize internally.
        // Returns false only on an unrecoverable error.
        bool RenderFrame(uint32_t windowWidth, uint32_t windowHeight);

        void SetClearColor(float r, float g, float b, float a = 1.0f) { m_clearColor = {r, g, b, a}; }

    private:
        struct Frame {
            // Declaration order = construction order; destruction runs in reverse (buffer before pool).
            std::unique_ptr<VulkanCommandPool> Pool;
            std::unique_ptr<VulkanCommandBuffer> Cmd;
            std::unique_ptr<VulkanSemaphore> ImageAvailable;
            std::unique_ptr<VulkanFence> InFlight;
        };

        VulkanRenderer(const VulkanDevice& device, VulkanSwapchain& swapchain)
            : m_device(device), m_swapchain(swapchain) {}

        bool Init();
        bool CreateRenderFinishedSemaphores();
        bool Recreate(uint32_t width, uint32_t height);
        void Record(VkCommandBuffer cmd, uint32_t imageIndex);

        const VulkanDevice& m_device;
        VulkanSwapchain& m_swapchain;

        std::array<Frame, FramesInFlight> m_frames;
        std::vector<std::unique_ptr<VulkanSemaphore>> m_renderFinished; // one per swapchain image
        uint32_t m_frameIndex = 0;

        bool m_needsRecreate = false;
        uint32_t m_requestedWidth = 0;
        uint32_t m_requestedHeight = 0;

        std::array<float, 4> m_clearColor = {1.0f, 0.0f, 0.0f, 1.0f};

        std::unique_ptr<VulkanGraphicsPipeline> m_trianglePipeline;
    };
} // namespace DA
