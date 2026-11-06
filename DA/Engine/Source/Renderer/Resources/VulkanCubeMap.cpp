#include "PCH.h"
#include "VulkanCubeMap.h"
#include <Core/Log.h>
#include <Renderer/RHI/Vulkan/Core/VulkanDevice.h>
#include <Renderer/RHI/Vulkan/Memory/VulkanAllocator.h>
#include <Renderer/RHI/Vulkan/Memory/VulkanBuffer.h>
#include <Renderer/RHI/Vulkan/Commands/VulkanCommandPool.h>
#include <Renderer/RHI/Vulkan/Commands/VulkanCommandBuffer.h>
#include <Renderer/Resources/Bitmap.h>

#include <include/vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>

#include <array>
#include <vector>

namespace DA {
    std::shared_ptr<VulkanCubeMap> VulkanCubeMap::Create(const VulkanDevice& device, const VulkanAllocator& allocator,
                                                         const CubeMapFaces& faces) {
        // Load the six faces.
        std::array<Bitmap, 6> bitmaps;

        for (uint32_t i = 0; i < 6; ++i) {
            auto bitmap = Bitmap::FromFile(faces.Paths[i]);
            if (!bitmap) {
                DA_ERROR("Failed to load cubemap face: {}", faces.Paths[i]);
                return nullptr;
            }
            bitmaps[i] = std::move(*bitmap);
        }
        const auto width = bitmaps[0].GetWidth();
        const auto height = bitmaps[0].GetHeight();
        if (width == 0 || height == 0 || width != height) {
            DA_ERROR("Cubemap faces must be non-zero square images.");
            return nullptr;
        }

        // All faces must have identical dimensions.
        for (uint32_t i = 1; i < 6; ++i) {
            if (bitmaps[i].GetWidth() != width || bitmaps[i].GetHeight() != height) {
                DA_ERROR("Cubemap faces have different dimensions.");
                return nullptr;
            }
        }
        constexpr VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
        const VkDeviceSize faceSize = static_cast<VkDeviceSize>(width) * height * 4;
        const VkDeviceSize totalSize = faceSize * 6;

        // Pack the six faces into one staging buffer.
        std::vector<uint8_t> pixels(static_cast<size_t>(totalSize));
        for (uint32_t i = 0; i < 6; ++i) {
            std::memcpy(pixels.data() + static_cast<size_t>(faceSize * i), bitmaps[i].Data(),
                        static_cast<size_t>(faceSize));
        }
        auto staging = VulkanBuffer::CreateHostVisible(allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, totalSize);
        if (!staging || !staging->Write(pixels.data(), totalSize)) {
            DA_ERROR("Failed to create cubemap staging buffer.");
            return nullptr;
        }

        auto cube = std::shared_ptr<VulkanCubeMap>(new VulkanCubeMap(device));
        cube->m_allocator = allocator.GetHandle();
        cube->m_width = width;
        cube->m_height = height;
        cube->m_format = format;

        // Create a cube-compatible Vulkan image.
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};

        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 6;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        VkResult result = vmaCreateImage(allocator.GetHandle(), &imageInfo, &allocationInfo, &cube->m_image,
                                         &cube->m_allocation, nullptr);
        if (result != VK_SUCCESS) {
            DA_ERROR("Cubemap image creation failed: {}", string_VkResult(result));
            return nullptr;
        }

        // Create a cube image view.
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = cube->m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 6;

        result = vkCreateImageView(device.GetHandle(), &viewInfo, nullptr, &cube->m_view);
        if (result != VK_SUCCESS) {
            DA_ERROR("Cubemap image view creation failed: {}", string_VkResult(result));
            return nullptr;
        }
        // Upload using a one-time command buffer.
        auto pool = VulkanCommandPool::Create(device, device.GetGraphicsFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
        if (!pool) return nullptr;

        auto cmd = VulkanCommandBuffer::Create(device, *pool);
        if (!cmd || !cmd->Begin()) return nullptr;

        const VkCommandBuffer command = cmd->GetHandle();

        // UNDEFINED -> TRANSFER_DST.
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        barrier.srcAccessMask = VK_ACCESS_2_NONE;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = cube->m_image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6};

        VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;

        vkCmdPipelineBarrier2(command, &dependency);

        // Copy each face into its corresponding array layer.
        std::array<VkBufferImageCopy, 6> regions{};

        for (uint32_t i = 0; i < 6; ++i) {
            regions[i].bufferOffset = faceSize * i;
            regions[i].imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, i, 1};
            regions[i].imageExtent = {width, height, 1};
        }

        vkCmdCopyBufferToImage(command, staging->GetHandle(), cube->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6,
                               regions.data());

        // TRANSFER_DST -> SHADER_READ_ONLY.
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vkCmdPipelineBarrier2(command, &dependency);

        if (!cmd->SubmitAndWait(device.GetGraphicsQueue())) {
            DA_ERROR("Cubemap upload failed.");
            return nullptr;
        }
        DA_INFO("Cubemap created: {}x{}, 6 faces.", width, height);
        return cube;
    }

    VulkanCubeMap::~VulkanCubeMap() {
        if (m_view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device.GetHandle(), m_view, nullptr);
        }
        if (m_image != VK_NULL_HANDLE) {
            vmaDestroyImage(m_allocator, m_image, m_allocation);
        }
    }
} // namespace DA
