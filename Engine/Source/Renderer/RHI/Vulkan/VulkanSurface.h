#pragma once

#include <Core/DA_API.h>

#include <vulkan/vulkan.h>

#include <memory>

namespace DA {
    class VulkanInstance;
    class Window;

    class DA_API VulkanSurface {
    public:
        // Returns nullptr on failure (error is logged).
        static std::unique_ptr<VulkanSurface> Create(const VulkanInstance& instance, const Window& window);
        ~VulkanSurface();

        VulkanSurface(const VulkanSurface&) = delete;
        VulkanSurface& operator=(const VulkanSurface&) = delete;

        VkSurfaceKHR GetHandle() const { return m_surface; }
        operator VkSurfaceKHR() const { return m_surface; }

    private:
        explicit VulkanSurface(const VulkanInstance& instance) : m_instance(instance) {}
        bool Init(const Window& window);
        // Destruction order matters: the surface must die before the instance.
        const VulkanInstance& m_instance;
        VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    };
} // namespace DA
