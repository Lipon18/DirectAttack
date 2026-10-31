#pragma once

#include <Core/DA_API.h>
#include <Core/Time/Clock.h>
#include <Scene/Camera.h>
#include <Renderer/RHI/Vulkan/Memory/VulkanBuffer.h>
#include <Renderer/RHI/Vulkan/Commands/VulkanCommandBuffer.h>
#include <Renderer/RHI/Vulkan/Commands/VulkanCommandPool.h>
#include <Renderer/RHI/Vulkan/Descriptors/VulkanDescriptors.h>
#include <Renderer/RHI/Vulkan/Pipelines/VulkanGraphicsPipeline.h>
#include <Renderer/RHI/Vulkan/Commands/VulkanSync.h>
#include <Renderer/RHI/Vulkan/Resources/VulkanImage.h>
#include <Renderer/Resources/VulkanMesh.h>

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace DA {
    class VulkanAllocator;
    class VulkanDevice;
    class VulkanSwapchain;

    // Owns the per-frame objects and runs the acquire -> record -> submit -> present loop.
    class DA_API VulkanRenderer {
    public:
        static constexpr uint32_t FramesInFlight = 2;

        struct DrawItem {
            const VulkanMesh* Mesh = nullptr;
            glm::mat4 Model{1.0f};
        };

        // The device and swapchain must outlive the renderer (declare the renderer after them).
        static std::unique_ptr<VulkanRenderer> Create(const VulkanDevice& device, const VulkanAllocator& allocator,
                                                      VulkanSwapchain& swapchain);
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;

        void Submit(const DrawItem& item) { m_drawList.push_back(item); }

        // Convenience so game code does not need the device or allocator.
        std::unique_ptr<VulkanMesh> CreateMesh(const MeshData& data) const;

        // Renders one frame. Handles resize and minimize internally.
        // Returns false only on an unrecoverable error.
        bool RenderFrame(uint32_t windowWidth, uint32_t windowHeight);

        void SetClearColor(float r, float g, float b, float a = 1.0f) { m_clearColor = {r, g, b, a}; }

        void SetCamera(const Camera& camera) { m_camera = camera; }

    private:
        struct Frame {
            // Declaration order = construction order; destruction runs in reverse (buffer before pool).
            std::unique_ptr<VulkanCommandPool> Pool;
            std::unique_ptr<VulkanCommandBuffer> Cmd;
            std::unique_ptr<VulkanSemaphore> ImageAvailable;
            std::unique_ptr<VulkanFence> InFlight;
            std::unique_ptr<VulkanBuffer> Uniform;
            VkDescriptorSet Set = VK_NULL_HANDLE;
        };

        VulkanRenderer(const VulkanDevice& device, const VulkanAllocator& allocator, VulkanSwapchain& swapchain)
            : m_device(device), m_allocator(allocator), m_swapchain(swapchain) {}

        bool Init();
        bool CreateRenderFinishedSemaphores();
        bool Recreate(uint32_t width, uint32_t height);
        void Record(VkCommandBuffer cmd, uint32_t imageIndex);

        const VulkanDevice& m_device;
        const VulkanAllocator& m_allocator;
        VulkanSwapchain& m_swapchain;

        std::array<Frame, FramesInFlight> m_frames;
        std::vector<std::unique_ptr<VulkanSemaphore>> m_renderFinished; // one per swapchain image
        uint32_t m_frameIndex = 0;

        bool m_needsRecreate = false;
        uint32_t m_requestedWidth = 0;
        uint32_t m_requestedHeight = 0;

        std::array<float, 4> m_clearColor = {1.0f, 0.0f, 0.0f, 1.0f};

        std::vector<DrawItem> m_drawList;

        std::unique_ptr<VulkanImage> m_texture;
        std::unique_ptr<VulkanSampler> m_sampler;
        std::unique_ptr<VulkanImage> m_depth;
        std::unique_ptr<VulkanDescriptorSetLayout> m_setLayout;
        std::unique_ptr<VulkanDescriptorPool> m_descriptorPool;
        std::unique_ptr<VulkanGraphicsPipeline> m_cubePipeline;
        Clock m_clock;

        Camera m_camera;
    };
} // namespace DA
