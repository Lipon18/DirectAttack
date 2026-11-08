#include "PCH.h"
#include <Core/Log.h>
#include <Core/Paths.h>
#include <Core/Jobs/ThreadPool.h>
#include "VulkanRenderer.h"
#include <Renderer/RHI/Vulkan/Core/VulkanDevice.h>
#include <Renderer/RHI/Vulkan/Memory/VulkanAllocator.h>
#include <Renderer/RHI/Vulkan/Presentation/VulkanSwapchain.h>
#include <Renderer/Resources/Bitmap.h>
#include <Renderer/Resources/Ktx2Loader.h>
#include <Renderer/Resources/gltfLoader.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vk_enum_string_helper.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>
#include <optional>
#include <span>

namespace DA {
    // CPU-decoded texture: a premade KTX2 or RGBA8 pixels. No Vulkan involved, so workers can make these.
    struct PreparedImage {
        std::optional<TextureData> Ktx;
        std::optional<Bitmap> Pixels;
    };

    namespace {
        // Must match FrameData in Lit.frag (std140, 624 bytes).
        struct FrameUniforms {
            glm::mat4 View;
            glm::mat4 Proj;
            glm::vec4 CameraPos;
            glm::vec4 Misc;   // x = time, y = dynamic light count
            glm::vec4 SunDir; // direction TOWARD the sun
            glm::vec4 SunColor;
            glm::vec4 Ambient;
            glm::vec4 FogColor;
            glm::vec4 FogParams; // x = start, y = end
            GpuLight Lights[MaxDynamicLights];
        };
        static_assert(sizeof(FrameUniforms) == 624);

        // Must match Push in Lit.vert/.frag (112 bytes).
        struct PushConstants {
            glm::mat4 Model;
            glm::vec4 BaseColor;
            glm::vec4 Emissive;
            float Metallic;
            float Roughness;
            float AlphaCutoff;
            uint32_t Flags;
        };
        static_assert(sizeof(PushConstants) == 112);

        // Must match Push in skybox.frag (scalars in the shader, because a vec2 would be 8-byte aligned there).
        struct SkyboxPushConstants {
            glm::vec4 BaseColor;
            glm::vec4 FogColor;
            float Blend;
            glm::vec2 FogLimits;
            float Padding = 0.0f;
        };
        static_assert(sizeof(SkyboxPushConstants) == 48);

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

        // Runs on worker threads: no Vulkan, no renderer state.
        PreparedImage PrepareImage(const ImageSourceData& src) {
            PreparedImage out;
            if (!src.Encoded.empty()) {
                if (IsKtx2(src.Encoded)) out.Ktx = ParseKtx2(src.Encoded, src.Name);
                if (!out.Ktx) out.Pixels = Bitmap::FromMemory(src.Encoded, src.Name);
            } else if (!src.Path.empty()) {
                std::filesystem::path p(src.Path);
                if (p.extension() == ".ktx2") {
                    out.Ktx = LoadKtx2File(src.Path);
                } else {
                    p.replace_extension(".ktx2");
                    if (std::filesystem::exists(p)) out.Ktx = LoadKtx2File(p.generic_string());
                    if (!out.Ktx) out.Pixels = Bitmap::FromFile(src.Path);
                }
            }
            return out;
        }
    } // namespace

    struct VulkanRenderer::ModelUpload {
        enum class Phase {
            Meshes,
            Textures,
            Materials,
            Finish
        };
        struct TexRequest {
            int32_t Image;
            bool Srgb;
        };

        std::shared_ptr<ModelAsset> Asset;
        std::string SourcePath;
        ModelData Data;
        std::vector<PreparedImage> Images; // index = Data.Images
        double WorkerMs = 0.0;
        double UploadMs = 0.0;

        Phase Current = Phase::Meshes;
        size_t Next = 0;
        std::vector<std::unique_ptr<VulkanMesh>> Meshes;
        std::vector<int32_t> PrimitiveMaterial;
        std::vector<TexRequest> Requests;
        std::vector<std::shared_ptr<VulkanImage>> Textures; // strong refs; the cache only holds weak ones
        std::vector<std::unique_ptr<VulkanMaterial>> Materials;
    };

    std::unique_ptr<VulkanRenderer> VulkanRenderer::Create(const VulkanDevice& device, const VulkanAllocator& allocator,
                                                           VulkanSwapchain& swapchain, ThreadPool& jobs) {
        std::unique_ptr<VulkanRenderer> renderer(new VulkanRenderer(device, allocator, swapchain, jobs));
        if (!renderer->Init()) return nullptr;
        return renderer;
    }

    // Synchronous texture path, used by CreateModel.
    std::shared_ptr<VulkanImage> VulkanRenderer::LoadTexture(const ImageSourceData& source, bool srgb) {
        // 1. KTX2: embedded in the glTF, given directly, or a sidecar next to the source image.
        std::optional<TextureData> tex;
        if (!source.Encoded.empty()) {
            if (IsKtx2(source.Encoded)) tex = ParseKtx2(source.Encoded, source.Name);
        } else if (!source.Path.empty()) {
            std::filesystem::path p(source.Path);
            if (p.extension() == ".ktx2") {
                tex = LoadKtx2File(source.Path);
            } else {
                p.replace_extension(".ktx2");
                if (std::filesystem::exists(p)) tex = LoadKtx2File(p.generic_string());
            }
        }
        if (tex) {
            tex->Format = WithColorSpace(tex->Format, srgb);
            if (auto image = VulkanImage::CreateFromTexture(m_device, m_allocator, *tex)) return image;
            DA_WARN("KTX2 texture unusable, falling back to the source image.");
        }
        // 2. stb: decode to RGBA8 and generate mips on the GPU.
        auto bitmap =
            source.Encoded.empty() ? Bitmap::FromFile(source.Path) : Bitmap::FromMemory(source.Encoded, source.Name);
        if (!bitmap) return nullptr;
        return VulkanImage::CreateTexture2D(m_device, m_allocator,
                                            srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
                                            bitmap->GetWidth(), bitmap->GetHeight(), bitmap->Data());
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

        // Default texture + sampler (stb gives RGBA, so the format is R8G8B8A8_SRGB).
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

        // Depth image (created before the pipelines, which need its format).
        m_depth = VulkanImage::CreateDepth(m_device, m_allocator, m_swapchain.GetExtent().width,
                                           m_swapchain.GetExtent().height);
        if (!m_depth) return false;

        // Descriptors: set 0 = per-frame uniforms, set 1 = per-material textures.
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

        // Lit pipelines: [blend * 2 + doubleSided].
        VulkanGraphicsPipelineDesc pipelineDesc;
        pipelineDesc.Language = EShaderLanguage::ESL_GLSL;
        pipelineDesc.VertexShader = "lit.vert";
        pipelineDesc.FragmentShader = "lit.frag";
        pipelineDesc.ColorFormat = m_swapchain.GetFormat();
        pipelineDesc.DepthFormat = m_depth->GetFormat();
        pipelineDesc.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        pipelineDesc.VertexBindings = {VertexBinding()};
        pipelineDesc.VertexAttributes = VertexAttributes();
        pipelineDesc.PushConstants = {
            {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants)}};
        pipelineDesc.SetLayouts = {m_setLayout->GetHandle(), m_materialLayout->GetHandle()};
        for (uint32_t i = 0; i < m_litPipelines.size(); ++i) {
            const bool blend = (i & 2) != 0;
            pipelineDesc.CullMode = (i & 1) ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
            pipelineDesc.BlendEnable = blend;
            pipelineDesc.DepthWrite = !blend; // transparent surfaces test depth but never write it
            m_litPipelines[i] = VulkanGraphicsPipeline::Create(m_device, pipelineDesc);
            if (!m_litPipelines[i]) return false;
        }

        // Default material: the logo texture, so cubes/planes and untextured models still draw.
        MaterialParams defaultParams;
        std::array<std::shared_ptr<VulkanImage>, VulkanMaterial::SlotCount> defaultTextures{};
        defaultTextures[VulkanMaterial::BaseColor] = m_texture;
        m_defaultMaterial = CreateMaterial(defaultParams, defaultTextures, false);
        if (!m_defaultMaterial) return false;

        // Skybox: set 1 = one cubemap sampler, drawn with a fullscreen triangle and no depth test.
        const VkDescriptorSetLayoutBinding skyBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                                      VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        m_skyboxLayout = VulkanDescriptorSetLayout::Create(m_device, std::span(&skyBinding, 1));
        const VkDescriptorPoolSize skyPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
        m_skyboxDescriptorPool = VulkanDescriptorPool::Create(m_device, 1, std::span(&skyPoolSize, 1));
        if (!m_skyboxLayout || !m_skyboxDescriptorPool) return false;
        m_skyboxSet = m_skyboxDescriptorPool->Allocate(m_skyboxLayout->GetHandle());
        if (m_skyboxSet == VK_NULL_HANDLE) return false;

        VulkanSamplerDesc skySampler;
        skySampler.AddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // no seams at face edges
        skySampler.Anisotropy = false;
        m_skyboxSampler = VulkanSampler::Create(m_device, skySampler);
        if (!m_skyboxSampler) return false;

        VulkanGraphicsPipelineDesc skyDesc;
        skyDesc.Language = EShaderLanguage::ESL_GLSL;
        skyDesc.VertexShader = "skybox.vert";
        skyDesc.FragmentShader = "skybox.frag";
        skyDesc.ColorFormat = m_swapchain.GetFormat();
        skyDesc.DepthFormat = m_depth->GetFormat(); // must match the attachment even though we don't test
        skyDesc.DepthTest = false;
        skyDesc.DepthWrite = false;
        skyDesc.CullMode = VK_CULL_MODE_NONE; // fullscreen triangle, winding doesn't matter
        skyDesc.PushConstants = {{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SkyboxPushConstants)}};
        skyDesc.SetLayouts = {m_setLayout->GetHandle(), m_skyboxLayout->GetHandle()};
        m_skyboxPipeline = VulkanGraphicsPipeline::Create(m_device, skyDesc);
        if (!m_skyboxPipeline) return false;

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

    std::shared_ptr<VulkanCubeMap> VulkanRenderer::CreateCubeMap(const CubeMapFaces& faces) const {
        return VulkanCubeMap::Create(m_device, m_allocator, faces);
    }

    std::unique_ptr<VulkanMaterial> VulkanRenderer::CreateMaterial(
        const MaterialParams& params,
        const std::array<std::shared_ptr<VulkanImage>, VulkanMaterial::SlotCount>& textures, bool doubleSided,
        bool blend) {
        auto material = std::make_unique<VulkanMaterial>();
        material->Params = params;
        material->DoubleSided = doubleSided;
        material->Blend = blend;
        material->Set = AllocateMaterialSet();
        if (material->Set == VK_NULL_HANDLE) return nullptr;
        for (uint32_t i = 0; i < VulkanMaterial::SlotCount; ++i) {
            material->Textures[i] = textures[i] ? textures[i] : m_whiteTexture;
            WriteCombinedImageSamplerDescriptor(m_device, material->Set, i, material->Textures[i]->GetView(),
                                                m_sampler->GetHandle());
        }
        return material;
    }

    // Synchronous path (blocks the caller). Prefer LoadModelAsync.
    std::unique_ptr<VulkanModel> VulkanRenderer::CreateModel(ModelData&& data, const std::string& sourcePath) {
        auto loadTexture = [&](int32_t image, bool srgb) -> std::shared_ptr<VulkanImage> {
            if (image < 0 || static_cast<size_t>(image) >= data.Images.size()) return nullptr;
            const std::string key = sourcePath + "#" + std::to_string(image) + (srgb ? "|srgb" : "|linear");
            return m_textureCache.GetOrCreate(key, [&]() { return LoadTexture(data.Images[image], srgb); });
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
            if (md.Alpha == EAlphaMode::EAM_MASK) p.Flags |= MaterialParams::FlagAlphaMask;
            const bool blend = (md.Alpha == EAlphaMode::EAM_BLEND);

            auto material = CreateMaterial(p, tex, md.DoubleSided, blend);
            if (!material) return nullptr;
            materials.push_back(std::move(material));
        }
        return VulkanModel::Create(m_device, m_allocator, std::move(data), std::move(materials));
    }

    bool VulkanRenderer::RenderFrame(uint32_t width, uint32_t height) {
        // Whatever happens below (early return, minimize, error), this frame's submissions are discarded.
        struct FrameSubmissionReset {
            std::vector<DrawItem>& List;
            std::vector<GpuLight>& Lights;
            ~FrameSubmissionReset() {
                List.clear();
                Lights.clear();
            }
        } submissionReset{m_drawList, m_lights};

        ProcessUploads(); // drain worker results and run the budgeted GPU uploads

        if (width == 0 || height == 0) return true; // minimized: nothing to draw

        if (m_needsRecreate || width != m_requestedWidth || height != m_requestedHeight) {
            if (!Recreate(width, height)) return true; // skip this frame, retry next one
        }

        Frame& frame = m_frames[m_frameIndex];

        if (!frame.InFlight->Wait()) {
            DA_ERROR("Waiting for the in-flight fence failed.");
            return false;
        }
        CollectRetired(); // frames up to this slot's previous use have finished

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

        ++m_frameCounter;
        m_frameIndex = (m_frameIndex + 1) % FramesInFlight;
        return true;
    }

    void VulkanRenderer::SetSkybox(std::unique_ptr<SkyboxMaterial> material) {
        // Descriptor updates and resource replacement must not race with frames using the old cubemap.
        m_device.WaitIdle();
        m_skybox = std::move(material);
        if (!m_skybox || !m_skybox->GetImage()) return;
        WriteCombinedImageSamplerDescriptor(m_device, m_skyboxSet, 0, m_skybox->GetImage()->GetView(),
                                            m_skyboxSampler->GetHandle());
    }

    void VulkanRenderer::RemoveSkybox() {
        m_device.WaitIdle();
        m_skybox.reset();
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
        const glm::vec3 camPos = m_camera.GetPosition();

        // Keep the nearest lights (by distance to the camera).
        auto dist2 = [&](const GpuLight& l) {
            const glm::vec3 d = glm::vec3(l.PositionRadius) - camPos;
            return glm::dot(d, d);
        };
        const size_t lightCount = std::min<size_t>(m_lights.size(), MaxDynamicLights);
        std::partial_sort(m_lights.begin(), m_lights.begin() + lightCount, m_lights.end(),
                          [&](const GpuLight& a, const GpuLight& b) { return dist2(a) < dist2(b); });

        const Environment& env = m_environment;
        FrameUniforms uniforms{};
        uniforms.View = m_camera.GetViewMatrix();
        uniforms.Proj = m_camera.GetProjectionMatrix(aspect);
        uniforms.CameraPos = glm::vec4(camPos, 1.0f);
        uniforms.Misc = glm::vec4(t, static_cast<float>(lightCount), 0.0f, 0.0f);
        uniforms.SunDir = glm::vec4(-glm::normalize(env.Sun.Direction), 0.0f);
        uniforms.SunColor = glm::vec4(env.Sun.Color * env.Sun.Intensity, 0.0f);
        uniforms.Ambient = glm::vec4(env.Ambient, 0.0f);
        uniforms.FogColor = glm::vec4(env.Fog.Color, 1.0f);
        uniforms.FogParams = env.Fog.Enabled ? glm::vec4(env.Fog.Start, env.Fog.End, 0.0f, 0.0f) : glm::vec4(0.0f);
        for (size_t i = 0; i < lightCount; ++i)
            uniforms.Lights[i] = m_lights[i];
        frame.Uniform->Write(&uniforms, sizeof(uniforms));

        // Skybox first (no vertex buffer: a fullscreen triangle, no depth test or write).
        if (m_skybox && m_skybox->GetImage() && m_skyboxPipeline) {
            m_skyboxPipeline->Bind(cmd);
            // Set 0: frame uniforms.
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline->GetLayout(), 0, 1,
                                    &frame.Set, 0, nullptr);
            // Set 1: cubemap.
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline->GetLayout(), 1, 1,
                                    &m_skyboxSet, 0, nullptr);
            const SkyboxPushConstants push{m_skybox->GetBaseColor(), m_skybox->GetFogColor(), m_skybox->GetBlend(),
                                           m_skybox->GetFogLimits(), 0.0f};
            vkCmdPushConstants(cmd, m_skyboxPipeline->GetLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(SkyboxPushConstants), &push);
            vkCmdDraw(cmd, 3, 1, 0, 0);
        }

        // Geometry. The skybox layout has different push constants, so set 0 must be bound again here.
        // Set 0 has the same layout in all lit pipelines, so one bind covers them.
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_litPipelines[0]->GetLayout(), 0, 1, &frame.Set,
                                0, nullptr);

        struct Visible {
            const DrawItem* Item;
            const VulkanMaterial* Mat;
            uint32_t Pipeline;
            float Dist2; // camera to object center, used for blended sorting
        };
        std::vector<Visible> opaque, blended;
        opaque.reserve(m_drawList.size());

        const Frustum frustum = Frustum::FromViewProj(uniforms.Proj * uniforms.View);
        m_lastDrawn = m_lastCulled = 0;
        for (const DrawItem& item : m_drawList) {
            if (!item.Mesh) continue;
            if (!frustum.Intersects(item.Mesh->GetBounds(), item.Model)) {
                ++m_lastCulled;
                continue;
            }
            ++m_lastDrawn;
            const VulkanMaterial* mat = item.Material ? item.Material : m_defaultMaterial.get();
            const uint32_t pipeline = (mat->Blend ? 2u : 0u) + (mat->DoubleSided ? 1u : 0u);
            const glm::vec3 center = glm::vec3(item.Model * glm::vec4(item.Mesh->GetBounds().Center(), 1.0f));
            const glm::vec3 d = center - camPos;
            const Visible v{&item, mat, pipeline, glm::dot(d, d)};
            (mat->Blend ? blended : opaque).push_back(v);
        }

        // Opaque and alpha-tested: group by pipeline, material, mesh to minimize state changes.
        std::sort(opaque.begin(), opaque.end(), [](const Visible& a, const Visible& b) {
            if (a.Pipeline != b.Pipeline) return a.Pipeline < b.Pipeline;
            if (a.Mat != b.Mat) return a.Mat < b.Mat;
            return a.Item->Mesh < b.Item->Mesh;
        });
        // Blended: back to front.
        std::sort(blended.begin(), blended.end(), [](const Visible& a, const Visible& b) { return a.Dist2 > b.Dist2; });

        uint32_t boundPipeline = UINT32_MAX;
        const VulkanMaterial* boundMaterial = nullptr;
        const VulkanMesh* boundMesh = nullptr;
        auto draw = [&](const Visible& v) {
            const VulkanGraphicsPipeline* pipeline = m_litPipelines[v.Pipeline].get();
            if (v.Pipeline != boundPipeline) {
                pipeline->Bind(cmd);
                boundPipeline = v.Pipeline;
            }
            if (v.Mat != boundMaterial) {
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetLayout(), 1, 1, &v.Mat->Set,
                                        0, nullptr);
                boundMaterial = v.Mat;
            }
            const MaterialParams& mp = v.Mat->Params;
            const PushConstants push{v.Item->Model, mp.BaseColor,   mp.Emissive, mp.Metallic,
                                     mp.Roughness,  mp.AlphaCutoff, mp.Flags};
            vkCmdPushConstants(cmd, pipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(push), &push);
            if (v.Item->Mesh != boundMesh) {
                v.Item->Mesh->Bind(cmd);
                boundMesh = v.Item->Mesh;
            }
            v.Item->Mesh->Draw(cmd);
        };
        for (const Visible& v : opaque)
            draw(v);
        for (const Visible& v : blended)
            draw(v);

        vkCmdEndRendering(cmd);

        // COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC. No destination stage: the semaphore signal covers visibility.
        TransitionImage(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE);
    }

    std::shared_ptr<ModelAsset> VulkanRenderer::LoadModelAsync(const std::string& path) {
        auto asset = std::shared_ptr<ModelAsset>(new ModelAsset());
        asset->m_name = std::filesystem::path(path).stem().string();
        const auto start = std::chrono::steady_clock::now();

        // Worker lambdas only touch `this` through the main-thread queue: the renderer may be gone by the time they
        // finish, and the queue is shared_ptr-owned for exactly that reason.
        ThreadPool* jobs = &m_jobs;
        m_jobs.Enqueue([this, jobs, asset, path, start, queue = m_mainQueue]() {
            auto parsed = Loadgltf(path);
            if (!parsed) {
                asset->m_state.store(ModelAsset::State::Failed, std::memory_order_release);
                return;
            }

            auto upload = std::make_shared<ModelUpload>();
            upload->Asset = asset;
            upload->SourcePath = path;
            upload->Data = std::move(*parsed);
            const size_t imageCount = upload->Data.Images.size();
            upload->Images.resize(imageCount);

            // Which (image, color space) textures do the materials need?
            auto request = [&](int32_t image, bool srgb) {
                if (image < 0 || static_cast<size_t>(image) >= imageCount) return;
                for (const auto& r : upload->Requests)
                    if (r.Image == image && r.Srgb == srgb) return;
                upload->Requests.push_back({image, srgb});
            };
            for (const MaterialData& m : upload->Data.Materials) {
                request(m.BaseColorImage, true);
                request(m.NormalImage, false);
                request(m.MetallicRoughnessImage, false);
                request(m.OcclusionImage, false);
                request(m.EmissiveImage, true);
            }

            // Runs on whichever worker finishes last.
            auto finish = [this, upload, queue, start]() {
                upload->WorkerMs =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                queue->Post([this, upload]() { m_uploads.push_back(upload); });
            };

            if (imageCount == 0) {
                finish();
                return;
            }

            // Fan out: one job per image, so textures decode in parallel.
            auto remaining = std::make_shared<std::atomic<size_t>>(imageCount);
            for (size_t i = 0; i < imageCount; ++i) {
                jobs->Enqueue([upload, remaining, i, finish]() {
                    try {
                        upload->Images[i] = PrepareImage(upload->Data.Images[i]); // each job writes only its own slot
                    } catch (...) {
                    }
                    if (remaining->fetch_sub(1, std::memory_order_acq_rel) == 1) finish();
                });
            }
        });
        return asset;
    }

    void VulkanRenderer::ProcessUploads() {
        m_mainQueue->Drain();

        constexpr double BudgetMs = 3.0;
        const auto begin = std::chrono::steady_clock::now();
        while (!m_uploads.empty()) {
            ModelUpload& job = *m_uploads.front();
            const auto stepStart = std::chrono::steady_clock::now();
            const bool done = StepUpload(job);
            const auto now = std::chrono::steady_clock::now();
            job.UploadMs += std::chrono::duration<double, std::milli>(now - stepStart).count();
            if (done) m_uploads.pop_front();
            if (std::chrono::duration<double, std::milli>(now - begin).count() >= BudgetMs) break;
        }
    }

    void VulkanRenderer::CollectRetired() {
        std::erase_if(m_retired, [&](const Retired& r) { return r.Frame + FramesInFlight <= m_frameCounter; });
    }

    bool VulkanRenderer::StepUpload(ModelUpload& job) {
        ModelAsset& asset = *job.Asset;
        auto fail = [&](const char* why) {
            DA_ERROR("Model '{}': {}", asset.m_name, why);
            asset.m_state.store(ModelAsset::State::Failed, std::memory_order_release);
            return true;
        };
        auto textureKey = [&](int32_t image, bool srgb) {
            return job.SourcePath + "#" + std::to_string(image) + (srgb ? "|srgb" : "|linear");
        };

        switch (job.Current) {
        case ModelUpload::Phase::Meshes: {
            if (job.Next >= job.Data.Primitives.size()) {
                job.Current = ModelUpload::Phase::Textures;
                job.Next = 0;
                return false;
            }
            PrimitiveData& prim = job.Data.Primitives[job.Next++];
            auto mesh = VulkanMesh::Create(m_device, m_allocator, prim.Mesh.Vertices, prim.Mesh.Indices);
            if (!mesh) return fail("mesh upload failed");
            std::vector<Vertex>().swap(prim.Mesh.Vertices); // free the CPU copy right away
            std::vector<uint32_t>().swap(prim.Mesh.Indices);
            job.PrimitiveMaterial.push_back(prim.Material);
            job.Meshes.push_back(std::move(mesh));
            return false;
        }
        case ModelUpload::Phase::Textures: {
            if (job.Next >= job.Requests.size()) {
                job.Current = ModelUpload::Phase::Materials;
                return false;
            }
            const ModelUpload::TexRequest req = job.Requests[job.Next++];
            auto texture =
                m_textureCache.GetOrCreate(textureKey(req.Image, req.Srgb), [&]() -> std::shared_ptr<VulkanImage> {
                    PreparedImage& prep = job.Images[req.Image];
                    if (prep.Ktx) {
                        prep.Ktx->Format = WithColorSpace(prep.Ktx->Format, req.Srgb);
                        if (auto image = VulkanImage::CreateFromTexture(m_device, m_allocator, *prep.Ktx)) return image;
                        DA_WARN("KTX2 texture unusable, falling back to the source image.");
                    }
                    if (!prep.Pixels) { // rare: KTX2 parsed but unusable on this GPU, so decode on the main thread
                        const ImageSourceData& src = job.Data.Images[req.Image];
                        prep.Pixels = src.Encoded.empty() ? Bitmap::FromFile(src.Path)
                                                          : Bitmap::FromMemory(src.Encoded, src.Name);
                    }
                    if (!prep.Pixels) return nullptr;
                    return VulkanImage::CreateTexture2D(
                        m_device, m_allocator, req.Srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
                        prep.Pixels->GetWidth(), prep.Pixels->GetHeight(), prep.Pixels->Data());
                });
            job.Textures.push_back(std::move(texture)); // null on failure: the material falls back to white
            return false;
        }
        case ModelUpload::Phase::Materials: {
            // Lookup only: the textures were created above and are kept alive by job.Textures.
            auto lookup = [&](int32_t image, bool srgb) -> std::shared_ptr<VulkanImage> {
                if (image < 0) return nullptr;
                return m_textureCache.GetOrCreate(textureKey(image, srgb),
                                                  []() { return std::shared_ptr<VulkanImage>(); });
            };
            for (const MaterialData& md : job.Data.Materials) {
                std::array<std::shared_ptr<VulkanImage>, VulkanMaterial::SlotCount> tex{};
                tex[VulkanMaterial::BaseColor] = lookup(md.BaseColorImage, true);
                tex[VulkanMaterial::Normal] = lookup(md.NormalImage, false);
                tex[VulkanMaterial::MetallicRoughness] = lookup(md.MetallicRoughnessImage, false);
                tex[VulkanMaterial::Occlusion] = lookup(md.OcclusionImage, false);
                tex[VulkanMaterial::Emissive] = lookup(md.EmissiveImage, true);

                MaterialParams p;
                p.BaseColor = md.BaseColorFactor;
                p.Emissive = glm::vec4(md.EmissiveFactor, 0.0f);
                p.Metallic = md.Metallic;
                p.Roughness = md.Roughness;
                p.AlphaCutoff = md.AlphaCutoff;
                if (tex[VulkanMaterial::Normal]) p.Flags |= MaterialParams::FlagNormalMap;
                if (md.Alpha == EAlphaMode::EAM_MASK) p.Flags |= MaterialParams::FlagAlphaMask;
                const bool blend = (md.Alpha == EAlphaMode::EAM_BLEND);

                auto material = CreateMaterial(p, tex, md.DoubleSided, blend);
                if (!material) return fail("material creation failed");
                job.Materials.push_back(std::move(material));
            }
            job.Current = ModelUpload::Phase::Finish;
            return false;
        }
        case ModelUpload::Phase::Finish: {
            asset.m_model = VulkanModel::Assemble(std::move(job.Meshes), std::move(job.Materials),
                                                  job.PrimitiveMaterial, job.Data.Instances, job.Data.Bounds);
            if (!asset.m_model) return fail("assembly failed");
            asset.m_bounds = job.Data.Bounds;
            DA_INFO("Model '{}' ready: {:.0f} ms on workers, {:.0f} ms uploading on the main thread.", asset.m_name,
                    job.WorkerMs, job.UploadMs);
            asset.m_state.store(ModelAsset::State::Ready, std::memory_order_release);
            return true;
        }
        }
        return true;
    }

    VulkanRenderer::~VulkanRenderer() {
        // Nothing may still be using the semaphores, fences, command buffers or descriptors when they are
        // destroyed.
        m_device.WaitIdle();
        m_uploads.clear();
        m_retired.clear();
    }
} // namespace DA