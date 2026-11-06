#pragma once

#include <Core/DA_API.h>
#include <Renderer/RHI/Vulkan/Resources/VulkanImage.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace DA {
    class VulkanDevice;
    class VulkanAllocator;

    // Face ordering:
    // +X, -X, +Y, -Y, +Z, -Z
    struct CubeMapFaces {
        std::array<std::string, 6> Paths;
    };

    class DA_API VulkanCubeMap {
    public:
        static std::shared_ptr<VulkanCubeMap> Create(const VulkanDevice& device, const VulkanAllocator& allocator,
                                                     const CubeMapFaces& faces);

        ~VulkanCubeMap();

        VulkanCubeMap(const VulkanCubeMap&) = delete;
        VulkanCubeMap& operator=(const VulkanCubeMap&) = delete;

        VkImage GetHandle() const { return m_image; }
        VkImageView GetView() const { return m_view; }
        VkFormat GetFormat() const { return m_format; }

        uint32_t GetWidth() const { return m_width; }
        uint32_t GetHeight() const { return m_height; }

    private:
        explicit VulkanCubeMap(const VulkanDevice& device) : m_device(device) {}

        const VulkanDevice& m_device;

        VmaAllocator m_allocator = VK_NULL_HANDLE;
        VmaAllocation m_allocation = VK_NULL_HANDLE;

        VkImage m_image = VK_NULL_HANDLE;
        VkImageView m_view = VK_NULL_HANDLE;
        VkFormat m_format = VK_FORMAT_UNDEFINED;

        uint32_t m_width = 0;
        uint32_t m_height = 0;
    };
} // namespace DA
