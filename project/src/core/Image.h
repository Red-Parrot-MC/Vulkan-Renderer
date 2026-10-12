#pragma once
#include "vulkan/vulkan.h"
#include "../libs/VmaUsage.h"

class Image final
{
public:
	Image();
	~Image();

	void SetDebugNameInfo(const char* name);
	void Create(const VkImageCreateInfo& createInfo, VkImageAspectFlags aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D);
	void Create(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
	void GenerateMipmaps(uint32_t mipLevels, VkImageSubresourceRange subresourceRange);
	void Destroy();
	void DestroyConst() const;

	void TransitionLayout(VkImageLayout newLayout);
	void TransitionLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout);
	void TransitionLayout(VkImageLayout newLayout, VkImageSubresourceRange subResourceRange);
	void TransitionLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout, VkImageSubresourceRange subResourceRange);

private:
	VkImage image{};
	VkImageView view{};
	VmaAllocation vmaAllocation{};

	VkImageLayout layout{};
	VkFormat format{};
	uint32_t mipLevels{};
	VkImageAspectFlags aspectMask{};
	VkExtent3D extent{};
	uint32_t arrayLayers{};

public:
	VkImage getImage() const { return image; }
	VkImageView getView() const { return view; }
	VkImageLayout getLayout() const { return layout; }
	VkFormat getFormat() const { return format; }
	uint32_t getMipLevels() const { return mipLevels; }
	VkImageAspectFlags getAspectMask() const { return aspectMask; }
	VkExtent3D getExtent() const { return extent; }

	void setAspectMask(VkImageAspectFlags aspectMask) { this->aspectMask = aspectMask; }
};