#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

namespace DA {
    class VulkanDevice;
    class VulkanAllocator;

    class DA_API VulkanImage {
    public:
        // Uploads tightly packed 4-byte pixels and builds a full mip chain (blocks until done).
        // `format` must support blit + linear filtering. B8G8R8A8/R8G8B8A8 UNORM/SRGB always do.
        // The image is left in SHADER_READ_ONLY_OPTIMAL.
        static std::unique_ptr<VulkanImage> CreateTexture2D(const VulkanDevice& device,
                                                            const VulkanAllocator& allocator, VkFormat format,
                                                            uint32_t width, uint32_t height, const void* pixels);

        // Depth attachment image (format from the device), cleared every frame. Layout starts UNDEFINED.
        static std::unique_ptr<VulkanImage> CreateDepth(const VulkanDevice& device, const VulkanAllocator& allocator,
                                                        uint32_t width, uint32_t height);

        ~VulkanImage();

        VulkanImage(const VulkanImage&) = delete;
        VulkanImage& operator=(const VulkanImage&) = delete;

        VkImage GetHandle() const { return m_image; }
        VkImageView GetView() const { return m_view; }
        uint32_t GetWidth() const { return m_width; }
        uint32_t GetHeight() const { return m_height; }
        uint32_t GetMipLevels() const { return m_mipLevels; }
        VkFormat GetFormat() const { return m_format; }
        VkImageAspectFlags GetAspect() const { return m_aspect; }

    private:
        explicit VulkanImage(const VulkanDevice& device) : m_device(device) {}

        const VulkanDevice& m_device;
        VmaAllocator m_allocator = VK_NULL_HANDLE;
        VmaAllocation m_allocation = VK_NULL_HANDLE;
        VkImage m_image = VK_NULL_HANDLE;
        VkImageView m_view = VK_NULL_HANDLE;
        VkFormat m_format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags m_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint32_t m_mipLevels = 1;
    };

    struct VulkanSamplerDesc {
        VkFilter Filter = VK_FILTER_LINEAR;
        VkSamplerAddressMode AddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        bool Anisotropy = true;
    };

    class DA_API VulkanSampler {
    public:
        static std::unique_ptr<VulkanSampler> Create(const VulkanDevice& device, const VulkanSamplerDesc& desc = {});
        ~VulkanSampler();

        VulkanSampler(const VulkanSampler&) = delete;
        VulkanSampler& operator=(const VulkanSampler&) = delete;

        VkSampler GetHandle() const { return m_sampler; }

    private:
        explicit VulkanSampler(const VulkanDevice& device) : m_device(device) {}

        const VulkanDevice& m_device;
        VkSampler m_sampler = VK_NULL_HANDLE;
    };
} // namespace DA
