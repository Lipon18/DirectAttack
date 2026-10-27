#include "PCH.h"
#include "VulkanImage.h"
#include <Renderer/RHI/Vulkan/Memory/VulkanAllocator.h>
#include <Renderer/RHI/Vulkan/Memory/VulkanBuffer.h>
#include <Renderer/RHI/Vulkan/Commands/VulkanCommandBuffer.h>
#include <Renderer/RHI/Vulkan/Commands/VulkanCommandPool.h>
#include <Renderer/RHI/Vulkan/Core/VulkanDevice.h>

#include <vulkan/vk_enum_string_helper.h>

#include <include/vk_mem_alloc.h>

#include <bit>

namespace DA {
    namespace {
        void ImageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                          VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                          VkAccessFlags2 dstAccess, uint32_t baseMip, uint32_t levelCount) {
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
            barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, baseMip, levelCount, 0, 1};

            VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
            dependency.imageMemoryBarrierCount = 1;
            dependency.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(cmd, &dependency);
        }
    } // namespace
    std::unique_ptr<VulkanImage> VulkanImage::CreateTexture2D(const VulkanDevice& device,
                                                              const VulkanAllocator& allocator, VkFormat format,
                                                              uint32_t width, uint32_t height, const void* pixels) {
        if (width == 0 || height == 0 || !pixels) {
            DA_ERROR("CreateTexture2D: invalid size ot null pixels.");
            return nullptr;
        }
        const VkDeviceSize byteSize = static_cast<VkDeviceSize>(width) * height * 4;
        auto staging = VulkanBuffer::CreateHostVisible(allocator, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, byteSize);
        if (!staging || !staging->Write(pixels, byteSize)) return nullptr;

        std::unique_ptr<VulkanImage> image(new VulkanImage(device));
        image->m_width = width;
        image->m_height = height;
        image->m_format = format;
        image->m_mipLevels = static_cast<uint32_t>(std::bit_width(std::max(width, height)));

        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {width, height, 1};
        info.mipLevels = image->m_mipLevels;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        if (VkResult r = vmaCreateImage(allocator.GetHandle(), &info, &allocInfo, &image->m_image, &image->m_allocation,
                                        nullptr);
            r != VK_SUCCESS) {
            DA_ERROR("vmaCreateImage failed: {}", string_VkResult(r));
            image->m_image = VK_NULL_HANDLE;
            return nullptr;
        }
        image->m_allocator = allocator.GetHandle();

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image->m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, image->m_mipLevels, 0, 1};
        if (VkResult r = vkCreateImageView(device.GetHandle(), &viewInfo, nullptr, &image->m_view); r != VK_SUCCESS) {
            DA_ERROR("vkCreateImageView failed: {}", string_VkResult(r));
            image->m_view = VK_NULL_HANDLE;
            return nullptr;
        }

        // One command buffer: upload mip 0, blit down the chain, leave everything shader-readable.
        auto pool = VulkanCommandPool::Create(device, device.GetGraphicsFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
        if (!pool) return nullptr;
        auto cmd = VulkanCommandBuffer::Create(device, *pool); // destroyed before the pool
        if (!cmd || !cmd->Begin()) return nullptr;
        const VkCommandBuffer c = cmd->GetHandle();
        const VkImage img = image->m_image;
        const uint32_t mips = image->m_mipLevels;

        constexpr VkPipelineStageFlags2 transfer = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;

        // All mips: UNDEFINED -> TRANSFER_DST.
        ImageBarrier(c, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE,
                     VK_ACCESS_2_NONE, transfer, VK_ACCESS_2_TRANSFER_WRITE_BIT, 0, mips);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(c, staging->GetHandle(), img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        for (uint32_t i = 1; i < mips; ++i) {
            // Previous level: finished writing, about to be read by the blit.
            ImageBarrier(c, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, transfer,
                         VK_ACCESS_2_TRANSFER_WRITE_BIT, transfer, VK_ACCESS_2_TRANSFER_READ_BIT, i - 1, 1);
            VkImageBlit blit{};
            blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1};
            blit.srcOffsets[1] = {static_cast<int32_t>(std::max(1u, width >> (i - 1))),
                                  static_cast<int32_t>(std::max(1u, height >> (i - 1))), 1};
            blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1};
            blit.dstOffsets[1] = {static_cast<int32_t>(std::max(1u, width >> i)),
                                  static_cast<int32_t>(std::max(1u, height >> i)), 1};
            vkCmdBlitImage(c, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &blit, VK_FILTER_LINEAR);

            // Previous level is done for good: make it sampleable.
            ImageBarrier(c, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         transfer, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                         VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, i - 1, 1);
        }
        // The last level was only ever written.
        ImageBarrier(c, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, transfer,
                     VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                     VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, mips - 1, 1);
        if (!cmd->SubmitAndWait(device.GetGraphicsQueue())) {
            DA_ERROR("Texture upload failed.");
            return nullptr;
        }
        DA_INFO("Texture created: {}x{}, {} mip levels.", width, height, mips);
        return image;
    }

    std::unique_ptr<VulkanImage> VulkanImage::CreateDepth(const VulkanDevice& device, const VulkanAllocator& allocator,
                                                          uint32_t width, uint32_t height) {
        const VkFormat format = device.GetDepthFormat();
        if (format == VK_FORMAT_UNDEFINED || width == 0 || height == 0) {
            DA_ERROR("CreateDepth: no depth format or zero size.");
            return nullptr;
        }
        std::unique_ptr<VulkanImage> image(new VulkanImage(device));
        image->m_width = width;
        image->m_height = height;
        image->m_format = format;
        image->m_mipLevels = 1;
        const bool hasStencil = format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
        image->m_aspect =
            hasStencil ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_DEPTH_BIT;
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {width, height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT; // recommended for attachments
        if (VkResult r = vmaCreateImage(allocator.GetHandle(), &info, &allocInfo, &image->m_image, &image->m_allocation,
                                        nullptr);
            r != VK_SUCCESS) {
            DA_ERROR("vmaCreateImage (depth) failed: {}", string_VkResult(r));
            image->m_image = VK_NULL_HANDLE;
            return nullptr;
        }
        image->m_allocator = allocator.GetHandle();

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image->m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}; // the view only exposes depth
        if (VkResult r = vkCreateImageView(device.GetHandle(), &viewInfo, nullptr, &image->m_view); r != VK_SUCCESS) {
            DA_ERROR("vkCreateImageView (depth) failed: {}", string_VkResult(r));
            image->m_view = VK_NULL_HANDLE;
            return nullptr;
        }
        return image;
    }

    VulkanImage::~VulkanImage() {
        if (m_view != VK_NULL_HANDLE) vkDestroyImageView(m_device.GetHandle(), m_view, nullptr);
        if (m_image != VK_NULL_HANDLE) vmaDestroyImage(m_allocator, m_image, m_allocation);
    }

    std::unique_ptr<VulkanSampler> VulkanSampler::Create(const VulkanDevice& device, const VulkanSamplerDesc& desc) {
        std::unique_ptr<VulkanSampler> sampler(new VulkanSampler(device));

        const bool anisotropy = desc.Anisotropy && device.IsAnisotropyEnabled();

        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        info.magFilter = desc.Filter;
        info.minFilter = desc.Filter;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.addressModeU = desc.AddressMode;
        info.addressModeV = desc.AddressMode;
        info.addressModeW = desc.AddressMode;
        info.anisotropyEnable = anisotropy ? VK_TRUE : VK_FALSE;
        info.maxAnisotropy = anisotropy ? std::min(16.0f, device.GetMaxSamplerAnisotropy()) : 1.0f;
        info.minLod = 0.0f;
        info.maxLod = VK_LOD_CLAMP_NONE;
        info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

        if (VkResult r = vkCreateSampler(device.GetHandle(), &info, nullptr, &sampler->m_sampler); r != VK_SUCCESS) {
            DA_ERROR("vkCreateSampler failed: {}", string_VkResult(r));
            sampler->m_sampler = VK_NULL_HANDLE;
            return nullptr;
        }
        return sampler;
    }

    VulkanSampler::~VulkanSampler() {
        if (m_sampler != VK_NULL_HANDLE) vkDestroySampler(m_device.GetHandle(), m_sampler, nullptr);
    }
} // namespace DA
