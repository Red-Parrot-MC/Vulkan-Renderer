#include "Image.h"
#include <iostream>
#include <vulkan/vk_enum_string_helper.h>
#include "VulkanContext.h"
#include "ImageHelpers.h"

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
        std::cout << string_VkResult(result) << "\n";
        throw std::runtime_error("failed to create image using vma! ");
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
}

void Image::TransitionLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout, VkImageSubresourceRange subResourceRange)
{
    ImageHelpers::transitionImageLayout(commandBuffer, image, layout, newLayout, subResourceRange);
}