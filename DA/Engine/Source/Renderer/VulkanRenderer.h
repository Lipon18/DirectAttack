#pragma once

#include <Core/DA_API.h>
#include <Core/Time/Clock.h>
#include <Core/Jobs/MainThreadQueue.h>
#include <Scene/Camera.h>
#include <Scene/Frustum.h>
#include <Renderer/Lighting.h>
#include <Renderer/RHI/Vulkan/Memory/VulkanBuffer.h>
#include <Renderer/RHI/Vulkan/Commands/VulkanCommandBuffer.h>
#include <Renderer/RHI/Vulkan/Commands/VulkanCommandPool.h>
#include <Renderer/RHI/Vulkan/Commands/VulkanSync.h>
#include <Renderer/RHI/Vulkan/Descriptors/VulkanDescriptors.h>
#include <Renderer/RHI/Vulkan/Pipelines/VulkanGraphicsPipeline.h>
#include <Renderer/RHI/Vulkan/Resources/VulkanImage.h>
#include <Renderer/Resources/VulkanMesh.h>
#include <Renderer/Resources/VulkanModel.h>
#include <Renderer/Resources/TextureCache.h>
#include <Renderer/Resources/VulkanMaterial.h>
#include <Renderer/Resources/VulkanCubeMap.h>
#include <Renderer/Resources/SkyboxMaterial.h>
#include <Renderer/Resources/ModelAsset.h>

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace DA {
    class VulkanAllocator;
    class VulkanDevice;
    class VulkanSwapchain;
    class ThreadPool;

    // Owns the per-frame objects and runs the acquire -> record -> submit -> present loop.
    class DA_API VulkanRenderer {
    public:
        static constexpr uint32_t FramesInFlight = 2;

        struct DrawItem {
            const VulkanMesh* Mesh = nullptr; // not owned
            glm::mat4 Model{1.0f};
            const VulkanMaterial* Material = nullptr; // null = default material
        };

        // The device, allocator and swapchain must outlive the renderer.
        static std::unique_ptr<VulkanRenderer> Create(const VulkanDevice& device, const VulkanAllocator& allocator,
                                                      VulkanSwapchain& swapchain, ThreadPool& jobs);
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;

        // Queue a draw for the next RenderFrame. The list is cleared after every RenderFrame call.
        void Submit(const DrawItem& item) { m_drawList.push_back(item); }

        // Queues every instance of a model. `root` is applied on top of each node's own transform.
        void Submit(const VulkanModel& model, const glm::mat4& root = glm::mat4(1.0f)) {
            m_drawList.reserve(m_drawList.size() + model.GetInstances().size());
            for (const VulkanModel::Instance& inst : model.GetInstances()) {
                m_drawList.push_back({inst.Mesh, root * inst.Transform, inst.Material});
            }
        }

        // Dynamic lights, per frame like Submit. If more than MaxDynamicLights are submitted,
        // the ones nearest the camera are used.
        void SubmitLight(const PointLight& light) { m_lights.push_back(ToGpu(light)); }
        void SubmitLight(const SpotLight& light) { m_lights.push_back(ToGpu(light)); }

        void SetEnvironment(const Environment& env) { m_environment = env; }
        const Environment& GetEnvironment() const { return m_environment; }

        // Convenience so game code does not need the device or allocator.
        std::unique_ptr<VulkanMesh> CreateMesh(const MeshData& data) const;
        std::shared_ptr<VulkanCubeMap> CreateCubeMap(const CubeMapFaces& faces) const;
        std::unique_ptr<VulkanMaterial> CreateMaterial(
            const MaterialParams& params,
            const std::array<std::shared_ptr<VulkanImage>, VulkanMaterial::SlotCount>& textures, bool doubleSided,
            bool blend = false);

        // Synchronous (blocks the caller). Prefer LoadModelAsync.
        std::unique_ptr<VulkanModel> CreateModel(ModelData&& data, const std::string& sourcePath);

        // Returns immediately. The model appears (IsReady) after workers decode it and the budgeted uploads finish.
        std::shared_ptr<ModelAsset> LoadModelAsync(const std::string& path);

        // Frees an object only after every frame that might still use it has finished.
        // Pass the LAST reference by move. A plain reset() of a model that was just drawn is unsafe.
        // clang-format off
        template <class T>
        void Retire(std::shared_ptr<T> object) {
            if (object) m_retired.push_back({m_frameCounter, std::move(object)});
        }
        size_t GetPendingUploads() const { return m_uploads.size(); }
        // clang-format on

        // Renders one frame. Handles resize and minimize internally.
        // Returns false only on an unrecoverable error.
        bool RenderFrame(uint32_t windowWidth, uint32_t windowHeight);

        void SetClearColor(float r, float g, float b, float a = 1.0f) { m_clearColor = {r, g, b, a}; }
        void SetCamera(const Camera& camera) { m_camera = camera; }

        uint32_t GetLastDrawn() const { return m_lastDrawn; }
        uint32_t GetLastCulled() const { return m_lastCulled; }

        void SetSkybox(std::unique_ptr<SkyboxMaterial> material);
        void RemoveSkybox();
        const SkyboxMaterial* GetSkybox() const { return m_skybox.get(); }

    private:
        struct Frame {
            // Destruction runs in reverse: buffer before pool.
            std::unique_ptr<VulkanCommandPool> Pool;
            std::unique_ptr<VulkanCommandBuffer> Cmd;
            std::unique_ptr<VulkanSemaphore> ImageAvailable;
            std::unique_ptr<VulkanFence> InFlight;
            std::unique_ptr<VulkanBuffer> Uniform;
            VkDescriptorSet Set = VK_NULL_HANDLE; // owned by m_descriptorPool
        };

        struct Retired {
            uint64_t Frame;
            std::shared_ptr<void> Object;
        };

        struct ModelUpload; // defined in the .cpp

        VulkanRenderer(const VulkanDevice& device, const VulkanAllocator& allocator, VulkanSwapchain& swapchain,
                       ThreadPool& jobs)
            : m_device(device), m_allocator(allocator), m_swapchain(swapchain), m_jobs(jobs) {}

        bool Init();
        bool CreateRenderFinishedSemaphores();
        bool Recreate(uint32_t width, uint32_t height);
        void Record(VkCommandBuffer cmd, uint32_t imageIndex);

        std::shared_ptr<VulkanImage> LoadTexture(const ImageSourceData& source, bool srgb);
        VkDescriptorSet AllocateMaterialSet();

        bool StepUpload(ModelUpload& job); // returns true when the job is finished (success or failure)
        void ProcessUploads();
        void CollectRetired();

        const VulkanDevice& m_device;
        const VulkanAllocator& m_allocator;
        VulkanSwapchain& m_swapchain;
        ThreadPool& m_jobs;

        // Async loading and deferred deletion. The queue is shared_ptr-owned so a late worker can still post to it.
        std::shared_ptr<MainThreadQueue> m_mainQueue = std::make_shared<MainThreadQueue>();
        std::deque<std::shared_ptr<ModelUpload>> m_uploads;
        std::vector<Retired> m_retired;
        uint64_t m_frameCounter = 0;

        std::array<Frame, FramesInFlight> m_frames;
        std::vector<std::unique_ptr<VulkanSemaphore>> m_renderFinished; // one per swapchain image
        uint32_t m_frameIndex = 0;

        bool m_needsRecreate = false;
        uint32_t m_requestedWidth = 0;
        uint32_t m_requestedHeight = 0;

        std::array<float, 4> m_clearColor = {0.05f, 0.07f, 0.14f, 1.0f};

        // Per-frame submissions (cleared after every RenderFrame).
        std::vector<DrawItem> m_drawList;
        std::vector<GpuLight> m_lights;
        Environment m_environment;

        // Declared in dependency order; destroyed in reverse.
        std::shared_ptr<VulkanImage> m_texture;
        std::shared_ptr<VulkanImage> m_whiteTexture;
        std::unique_ptr<VulkanSampler> m_sampler;
        std::unique_ptr<VulkanImage> m_depth;                        // recreated with the swapchain
        std::unique_ptr<VulkanDescriptorSetLayout> m_setLayout;      // set 0: per frame
        std::unique_ptr<VulkanDescriptorSetLayout> m_materialLayout; // set 1: per material
        std::unique_ptr<VulkanDescriptorSetLayout> m_skyboxLayout;   // set 1 of the skybox pipeline
        std::unique_ptr<VulkanDescriptorPool> m_descriptorPool;
        std::vector<std::unique_ptr<VulkanDescriptorPool>> m_materialPools;
        std::unique_ptr<VulkanDescriptorPool> m_skyboxDescriptorPool;
        uint32_t m_materialPoolUsed = 0;
        std::array<std::unique_ptr<VulkanGraphicsPipeline>, 4> m_litPipelines; // [blend * 2 + doubleSided]
        std::unique_ptr<VulkanGraphicsPipeline> m_skyboxPipeline;
        TextureCache m_textureCache;
        std::unique_ptr<VulkanMaterial> m_defaultMaterial;

        VkDescriptorSet m_skyboxSet = VK_NULL_HANDLE; // owned by m_skyboxDescriptorPool
        std::unique_ptr<VulkanSampler> m_skyboxSampler;
        std::unique_ptr<SkyboxMaterial> m_skybox;

        Camera m_camera;
        Clock m_clock;

        uint32_t m_lastDrawn = 0;
        uint32_t m_lastCulled = 0;
    };
} // namespace DA
