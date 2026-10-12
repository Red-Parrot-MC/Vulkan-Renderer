#include "Image.h"
#include <iostream>
#include <vulkan/vk_enum_string_helper.h>
#include "VulkanContext.h"
#include "ImageHelpers.h"

//static void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange subresourceRange)
//{
//    LayoutTransitionInfo srcInfo = LayoutTransitionInfo::getLayoutInfo(oldLayout);
//    LayoutTransitionInfo dstInfo = LayoutTransitionInfo::getLayoutInfo(newLayout);
//
//    VkImageMemoryBarrier2 barrier{};
//    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
//
//    barrier.oldLayout = oldLayout;
//    barrier.newLayout = newLayout;
//
//    barrier.srcStageMask = srcInfo.stage;
//    barrier.srcAccessMask = srcInfo.accessMask;
//
//    barrier.dstStageMask = dstInfo.stage;
//    barrier.dstAccessMask = dstInfo.accessMask;
//
//    barrier.image = image;
//    barrier.subresourceRange = subresourceRange;
//
//    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//
//    VkDependencyInfo dependencyInfo{};
//    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
//    dependencyInfo.pImageMemoryBarriers = &barrier;
//    dependencyInfo.imageMemoryBarrierCount = 1;
//
//    vkCmdPipelineBarrier2(
//        commandBuffer,
//        &dependencyInfo
//    );
//}
//
//static void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask, uint32_t mipLevels = 1)
//{
//    VkImageSubresourceRange subresourceRange{};
//    subresourceRange.aspectMask = aspectMask;
//    subresourceRange.baseMipLevel = 0;
//    subresourceRange.levelCount = mipLevels;
//    subresourceRange.baseArrayLayer = 0;
//    subresourceRange.layerCount = 1;
//    transitionImageLayout(commandBuffer, image, oldLayout, newLayout, subresourceRange);
//}
//
//static void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange subresourceRange)
//{
//    VkCommandBuffer commandBuffer = CommandBufferHelpers::beginSingleTimeCommands();
//
//    transitionImageLayout(commandBuffer, image, oldLayout, newLayout, subresourceRange);
//
//    CommandBufferHelpers::endSingleTimeCommands(commandBuffer);
//}
//
//static void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, VkImageAspectFlags aspectMask)
//{
//    VkImageSubresourceRange subresourceRange{};
//    subresourceRange.aspectMask = aspectMask;
//    subresourceRange.baseMipLevel = 0;
//    subresourceRange.levelCount = mipLevels;
//    subresourceRange.baseArrayLayer = 0;
//    subresourceRange.layerCount = 1;
//    transitionImageLayout(image, oldLayout, newLayout, subresourceRange);
//}

static void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, VkImageSubresourceRange subresourceRange) {
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(CONTEXT->physicalDevice, imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    subresourceRange.levelCount = 1;

    VkCommandBuffer commandBuffer = CommandBufferHelpers::beginSingleTimeCommands();

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        subresourceRange.baseMipLevel = i - 1;

        ImageHelpers::transitionImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresourceRange);

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = subresourceRange.aspectMask;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = subresourceRange.baseArrayLayer;
        blit.srcSubresource.layerCount = subresourceRange.layerCount;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = subresourceRange.aspectMask;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = subresourceRange.baseArrayLayer;
        blit.dstSubresource.layerCount = subresourceRange.layerCount;

        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        ImageHelpers::transitionImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    subresourceRange.baseMipLevel = mipLevels - 1;

    ImageHelpers::transitionImageLayout(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);

    CommandBufferHelpers::endSingleTimeCommands(commandBuffer);
}

Image::Image()
{

}

Image::~Image()
{
}

void Image::SetDebugNameInfo(const char* name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
    nameInfo.objectHandle = (uint64_t)image;
    nameInfo.pObjectName = name;
    setDebugUtilsObjectName(CONTEXT->device, &nameInfo);

    nameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    nameInfo.objectHandle = (uint64_t)view;
    setDebugUtilsObjectName(CONTEXT->device, &nameInfo);
}

void Image::Create(const VkImageCreateInfo& createInfo, VkImageAspectFlags aspectMask, VkImageViewType viewType)
{
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.pool = CONTEXT->vmaPool;

    VmaAllocationInfo vmaAllocationInfo{};

	VkResult result = vmaCreateImage(CONTEXT->vmaAllocator, &createInfo, &allocInfo, &image, &vmaAllocation, &vmaAllocationInfo);
    if (result != VK_SUCCESS)
    {
        std::cout << "IMAGE CREATION ERROR: " << string_VkResult(result) << "\n";
        //throw std::runtime_error("failed to create image using vma! ");
        return;
    }

    layout = createInfo.initialLayout;
    format = createInfo.format;
    mipLevels = createInfo.mipLevels;
    extent = createInfo.extent;
    arrayLayers = createInfo.arrayLayers;
    this->aspectMask = aspectMask;

    VkImageSubresourceRange subResourceRange{};
    subResourceRange.aspectMask = aspectMask;
    subResourceRange.levelCount = mipLevels;
    subResourceRange.layerCount = createInfo.arrayLayers;

    view = ImageHelpers::createImageView(image, format, subResourceRange, viewType);
}

void Image::Create(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectMask)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    Create(imageInfo, aspectMask);
}

void Image::Destroy()
{
    vmaDestroyImage(CONTEXT->vmaAllocator, image, vmaAllocation);
    if (view) vkDestroyImageView(CONTEXT->device, view, nullptr);
    image = VK_NULL_HANDLE;
    view = VK_NULL_HANDLE;
    vmaAllocation = VK_NULL_HANDLE;
}

void Image::DestroyConst() const
{
    vmaDestroyImage(CONTEXT->vmaAllocator, image, vmaAllocation);
    if (view) vkDestroyImageView(CONTEXT->device, view, nullptr);
}

void Image::TransitionLayout(VkImageLayout newLayout)
{
    VkImageSubresourceRange subResourceRange{};
    subResourceRange.aspectMask = aspectMask;
    subResourceRange.layerCount = arrayLayers;
    subResourceRange.levelCount = mipLevels;

    TransitionLayout(newLayout, subResourceRange);
}

void Image::TransitionLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout)
{
    VkImageSubresourceRange subResourceRange{};
    subResourceRange.aspectMask = aspectMask;
    subResourceRange.layerCount = arrayLayers;
    subResourceRange.levelCount = mipLevels;

    TransitionLayout(commandBuffer, newLayout, subResourceRange);
}

void Image::TransitionLayout(VkImageLayout newLayout, VkImageSubresourceRange subResourceRange)
{
    ImageHelpers::transitionImageLayout(image, layout, newLayout, subResourceRange);
    layout = newLayout;
}

void Image::TransitionLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout, VkImageSubresourceRange subResourceRange)
{
    ImageHelpers::transitionImageLayout(commandBuffer, image, layout, newLayout, subResourceRange);
    layout = newLayout;
}

void Image::GenerateMipmaps(uint32_t mipLevels, VkImageSubresourceRange subresourceRange)
{
    TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
    generateMipmaps(image, format, extent.width, extent.height, mipLevels, subresourceRange);
    layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}