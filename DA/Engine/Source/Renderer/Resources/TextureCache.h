#pragma once

#include <Renderer/RHI/Vulkan/Resources/VulkanImage.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace DA {
    // Dedupes textures by key. Holds weak references, so a texture is freed once no material uses it.
    class TextureCache {
    public:
        using Factory = std::function<std::shared_ptr<VulkanImage>()>;

        std::shared_ptr<VulkanImage> GetOrCreate(const std::string& key, const Factory& create) {
            if (auto it = m_entries.find(key); it != m_entries.end()) {
                if (auto alive = it->second.lock()) return alive;
            }
            std::shared_ptr<VulkanImage> texture = create();
            if (texture) m_entries[key] = texture;
            return texture;
        }

    private:
        std::unordered_map<std::string, std::weak_ptr<VulkanImage>> m_entries;
    };
} // namespace DA
