#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Texture
{
public:
	Texture(const char* filename);
	~Texture();

	uint32_t mipLevels;
	VkImage textureImage{};
	VkDeviceMemory textureImageMemory{};
	VkImageView textureImageView{};
	VkSampler textureSampler{};

private:
	void CreateTextureImage(const char* filename);
	void CreateTextureImageView();
	void CreateTextureSampler();
	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

};