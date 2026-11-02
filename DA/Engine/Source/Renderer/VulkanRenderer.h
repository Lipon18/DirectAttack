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
#include <Renderer/Resources/VulkanModel.h>
#include <Renderer/Resources/TextureCache.h>
#include <Renderer/Resources/VulkanMaterial.h>

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
            const VulkanMaterial* Material = nullptr;
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

        // Queues every instance of a model. `root` is applied on top of each node's own transform.
        void Submit(const VulkanModel& model, const glm::mat4& root = glm::mat4(1.0f)) {
            m_drawList.reserve(m_drawList.size() + model.GetInstances().size());
            for (const VulkanModel::Instance& inst : model.GetInstances()) {
                m_drawList.push_back({inst.Mesh, root * inst.Transform, inst.Material});
            }
        }

        std::unique_ptr<VulkanModel> CreateModel(ModelData&& data, const std::string& sourcePath);
        std::unique_ptr<VulkanMaterial> CreateMaterial(
            const MaterialParams& params,
            const std::array<std::shared_ptr<VulkanImage>, VulkanMaterial::SlotCount>& textures, bool doubleSided);

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

        VkDescriptorSet AllocateMaterialSet();

        const VulkanDevice& m_device;
        const VulkanAllocator& m_allocator;
        VulkanSwapchain& m_swapchain;

        std::array<Frame, FramesInFlight> m_frames;
        std::vector<std::unique_ptr<VulkanSemaphore>> m_renderFinished; // one per swapchain image
        uint32_t m_frameIndex = 0;

        bool m_needsRecreate = false;
        uint32_t m_requestedWidth = 0;
        uint32_t m_requestedHeight = 0;

        std::array<float, 4> m_clearColor = {0.16f, 0.22f, 0.30f, 1.0f};

        std::vector<DrawItem> m_drawList;

        std::shared_ptr<VulkanImage> m_texture;
        std::shared_ptr<VulkanImage> m_whiteTexture;
        std::unique_ptr<VulkanSampler> m_sampler;
        std::unique_ptr<VulkanImage> m_depth;
        std::unique_ptr<VulkanDescriptorSetLayout> m_setLayout;
        std::unique_ptr<VulkanDescriptorSetLayout> m_materialLayout;
        std::unique_ptr<VulkanDescriptorPool> m_descriptorPool;
        std::vector<std::unique_ptr<VulkanDescriptorPool>> m_materialPools;
        uint32_t m_materialPoolUsed = 0;
        std::unique_ptr<VulkanGraphicsPipeline> m_cubePipeline;
        std::unique_ptr<VulkanGraphicsPipeline> m_doubleSidedPipeline;
        TextureCache m_textureCache;
        std::unique_ptr<VulkanMaterial> m_defaultMaterial;

        Camera m_camera;
        Clock m_clock;
    };
} // namespace DA
