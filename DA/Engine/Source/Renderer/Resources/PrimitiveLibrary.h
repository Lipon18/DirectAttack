#pragma once

#include <Renderer/Resources/ModelAsset.h>
#include <Renderer/VulkanRenderer.h>
#include <Core/Paths.h>
#include <Core/Log.h>

#include <array>
#include <memory>
#include <cstdint>
#include <string>

namespace DA {
    enum class EPrimitive : uint8_t {
        E_CUBE = 0,
        E_SPHERE,
        E_CAPSULE,
        E_CYLINDER,
        E_CONE,
        E_COUNT
    };

    class PrimitiveLibrary {
    public:
        PrimitiveLibrary() = default;

        // Load all built-in primitives asynchronously.
        bool Load(VulkanRenderer& renderer) {
            m_primitives.fill(nullptr);
            constexpr std::array<const char*, Count> paths = {
                "Models/Primitives/Cube.glb", "Models/Primitives/Sphere.glb", "Models/Primitives/Capsule.glb",
                "Models/Primitives/Cylinder.glb", "Models/Primitives/Cone.glb"};
            constexpr std::array<const char*, Count> names = {"Cube", "Sphere", "Capsule", "Cylinder", "Cone"};

            bool success = true;

            for (size_t i = 0; i < Count; ++i) {
                const std::string path = Paths::ResolveEngine(paths[i]);
                m_primitives[i] = renderer.LoadModelAsync(path);
                if (!m_primitives[i]) {
                    DA_ERROR("Failed to request primitive: {}", names[i]);
                    success = false;
                } else {
                    DA_INFO("Loading engine primitive: {}", names[i]);
                }
            }
            return success;
        }

        // Get an asset, whether loading is complete or not.
        std::shared_ptr<ModelAsset> Get(EPrimitive primitive) const {
            const auto index = static_cast<size_t>(primitive);
            if (index >= Count) return nullptr;
            return m_primitives[index];
        }

        // Check whether a particular primitive has finished loading.
        bool IsReady(EPrimitive primitive) const {
            const auto asset = Get(primitive);
            return asset && asset->IsReady();
        }

        // Check whether every primitive has finished loading.
        bool AreAllReady() const {
            for (const auto& primitive : m_primitives) {
                if (!primitive || !primitive->IsReady()) return false;
            }
            return true;
        }

        // Release all primitive references.
        void Unload() {
            for (auto& primitive : m_primitives)
                primitive.reset();
        }

    private:
        static constexpr size_t Count = static_cast<size_t>(EPrimitive::E_COUNT);
        std::array<std::shared_ptr<ModelAsset>, Count> m_primitives{};
    };
} // namespace DA
