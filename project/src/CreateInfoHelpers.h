#pragma once
#include <vulkan/vulkan.h>
#include "VulkanContext.h"

VkSamplerCreateInfo GetSamplerCreateInfo(VkFilter filter, VkSamplerAddressMode addressMode)
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(CONTEXT->physicalDevice, &properties);


	VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    samplerInfo.minLod = 0.0f; // Optional
    samplerInfo.mipLodBias = 0.0f; // Optional

    return samplerInfo;
}