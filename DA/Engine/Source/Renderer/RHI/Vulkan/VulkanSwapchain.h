#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace DA {
    class VulkanDevice;
    class VulkanPhysicalDevice;
    class VulkanSurface;

    class DA_API VulkanSwapchain {
    public:
        static std::unique_ptr<VulkanSwapchain> Create(const VulkanDevice& device,
                                                       const VulkanPhysicalDevice& physicalDevice,
                                                       const VulkanSurface& surface, uint32_t width, uint32_t height);
        ~VulkanSwapchain();

        VulkanSwapchain(const VulkanSwapchain&) = delete;
        VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

        // Rebuilds the swapchain for a new size (waits for the GPU first).
        // Returns false if the size is 0x0 (minimized) or on error. The old swapchain stays valid in that case.
        bool Recreate(uint32_t width, uint32_t height);

        // Signals `signalSemaphore` when the image is ready. Check the result:
        // VK_ERROR_OUT_OF_DATE_KHR -> recreate and skip the frame; VK_SUBOPTIMAL_KHR -> usable, recreate soon.
        VkResult AcquireNextImage(VkSemaphore signalSemaphore, uint32_t& imageIndex,
                                  uint64_t timeoutNs = UINT64_MAX) const;
        VkResult Present(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) const;
        VkSwapchainKHR GetHandle() const { return m_swapchain; }
        VkFormat GetFormat() const { return m_format.format; }
        VkExtent2D GetExtent() const { return m_extent; }
        uint32_t GetImageCount() const { return static_cast<uint32_t>(m_images.size()); }
        VkImage GetImage(uint32_t i) const { return m_images[i]; }
        VkImageView GetImageView(uint32_t i) const { return m_imageViews[i]; }

    private:
        VulkanSwapchain(const VulkanDevice& device, const VulkanPhysicalDevice& physicalDevice,
                        const VulkanSurface& surface)
            : m_device(device), m_physicalDevice(physicalDevice), m_surface(surface) {}

        bool Build(uint32_t width, uint32_t height);
        void DestroyImageViews();

        const VulkanDevice& m_device;
        const VulkanPhysicalDevice& m_physicalDevice;
        const VulkanSurface& m_surface;

        VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
        VkSurfaceFormatKHR m_format{};
        VkExtent2D m_extent{};
        std::vector<VkImage> m_images;
        std::vector<VkImageView> m_imageViews;
    };
} // namespace DA
