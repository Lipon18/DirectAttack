#include "PCH.h"
#include <Renderer/RHI/Vulkan/Presentation/VulkanSwapchain.h>
#include <Renderer/RHI/Vulkan/Core/VulkanDevice.h>
#include <Renderer/RHI/Vulkan/Core/VulkanPhysicalDevice.h>
#include <Renderer/RHI/Vulkan/Core/VulkanSurface.h>
#include <Core/Log.h>

#include <vulkan/vk_enum_string_helper.h>

namespace DA {
    namespace {
        VkSurfaceFormatKHR ChooseFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
            for (const auto& f : formats) {
                if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
            }
            for (const auto& f : formats) {
                if (f.format == VK_FORMAT_R8G8B8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f;
            }
            return formats[0];
        }

        VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
            if (std::ranges::find(modes, VK_PRESENT_MODE_MAILBOX_KHR) != modes.end()) {
                return VK_PRESENT_MODE_MAILBOX_KHR;
            }
            return VK_PRESENT_MODE_FIFO_KHR; // always available
        }

        VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(VkCompositeAlphaFlagsKHR supported) {
            constexpr std::array candidates = {
                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
            for (auto c : candidates) {
                if (supported & c) return c;
            }
            return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        }
    } // namespace

    std::unique_ptr<VulkanSwapchain> VulkanSwapchain::Create(const VulkanDevice& device,
                                                             const VulkanPhysicalDevice& physicalDevice,
                                                             const VulkanSurface& surface, uint32_t width,
                                                             uint32_t height) {
        std::unique_ptr<VulkanSwapchain> swapChain(new VulkanSwapchain(device, physicalDevice, surface));
        if (!swapChain->Build(width, height)) {
            DA_FATAL("Failed to create the Vulkan swapchain.");
            return nullptr;
        }
        return swapChain;
    }

    bool VulkanSwapchain::Recreate(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0) return false; // minimized
        m_device.WaitIdle();                         // nothing may still use the old images
        return Build(width, height);
    }

    bool VulkanSwapchain::Build(uint32_t width, uint32_t height) {
        const VkPhysicalDevice pd = m_physicalDevice.GetHandle();
        const VkSurfaceKHR surface = m_surface.GetHandle();

        // Fresh queries every time: capabilities change when the window changes.
        VkSurfaceCapabilitiesKHR caps{};
        if (VkResult r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface, &caps); r != VK_SUCCESS) {
            DA_ERROR("vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: {}", string_VkResult(r));
            return false;
        }

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface, &formatCount, formats.data());

        uint32_t modeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &modeCount, nullptr);
        std::vector<VkPresentModeKHR> modes(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface, &modeCount, modes.data());

        if (formats.empty() || modes.empty()) {
            DA_ERROR("Surface reports no formats or present modes.");
            return false;
        }

        // 0xFFFFFFFF means "the swapchain decides the size"; otherwise we must match exactly.
        VkExtent2D extent = caps.currentExtent;
        if (extent.width == UINT32_MAX) {
            extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }
        if (extent.width == 0 || extent.height == 0) return false;

        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

        const VkSurfaceFormatKHR format = ChooseFormat(formats);

        VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        info.surface = surface;
        info.minImageCount = imageCount;
        info.imageFormat = format.format;
        info.imageColorSpace = format.colorSpace;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (caps.supportedUsageFlags && VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = ChooseCompositeAlpha(caps.supportedCompositeAlpha);
        info.presentMode = ChoosePresentMode(modes);
        info.clipped = VK_TRUE;
        info.oldSwapchain = m_swapchain; // lets the driver reuse resources during a resize

        const std::array<uint32_t, 2> families = {m_device.GetGraphicsFamily(), m_device.GetPresentFamily()};
        if (families[0] != families[1]) {
            info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            info.queueFamilyIndexCount = 2;
            info.pQueueFamilyIndices = families.data();
        } else {
            info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        VkSwapchainKHR created = VK_NULL_HANDLE;
        if (VkResult r = vkCreateSwapchainKHR(m_device.GetHandle(), &info, nullptr, &created); r != VK_SUCCESS) {
            DA_ERROR("vkCreateSwapchainKHR failed: {}", string_VkResult(r));
            return false; // old swapchain (if any) is untouched and still valid
        }

        // Retire the old swapchain and its views (the caller already waited for the GPU).
        DestroyImageViews();
        if (m_swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(m_device.GetHandle(), m_swapchain, nullptr);
        m_swapchain = created;
        m_format = format;
        m_extent = extent;

        uint32_t count = 0;
        vkGetSwapchainImagesKHR(m_device.GetHandle(), m_swapchain, &count, nullptr);
        m_images.resize(count);
        vkGetSwapchainImagesKHR(m_device.GetHandle(), m_swapchain, &count, m_images.data());

        m_imageViews.reserve(count);
        for (VkImage image : m_images) {
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view.image = image;
            view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view.format = m_format.format;
            view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            VkImageView imageView = VK_NULL_HANDLE;
            if (VkResult r = vkCreateImageView(m_device.GetHandle(), &view, nullptr, &imageView); r != VK_SUCCESS) {
                DA_ERROR("vkCreateImageView failed: {}", string_VkResult(r));
                DestroyImageViews();
                return false;
            }
            m_imageViews.push_back(imageView);
        }
        DA_INFO("Swapchain: {}x{}, {} images, format {}, present mode {}.", m_extent.width, m_extent.height, count,
                string_VkFormat(m_format.format), string_VkPresentModeKHR(info.presentMode));
        return true;
    }

    void VulkanSwapchain::DestroyImageViews() {
        for (VkImageView view : m_imageViews)
            vkDestroyImageView(m_device.GetHandle(), view, nullptr);
        m_imageViews.clear();
    }

    VkResult VulkanSwapchain::AcquireNextImage(VkSemaphore signalSemaphore, uint32_t& imageIndex,
                                               uint64_t timeoutNs) const {
        return vkAcquireNextImageKHR(m_device.GetHandle(), m_swapchain, timeoutNs, signalSemaphore, VK_NULL_HANDLE,
                                     &imageIndex);
    }

    VkResult VulkanSwapchain::Present(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) const {
        VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &waitSemaphore;
        info.swapchainCount = 1;
        info.pSwapchains = &m_swapchain;
        info.pImageIndices = &imageIndex;
        return vkQueuePresentKHR(queue, &info);
    }

    VulkanSwapchain::~VulkanSwapchain() {
        DestroyImageViews();
        if (m_swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(m_device.GetHandle(), m_swapchain, nullptr);
    }
} // namespace DA