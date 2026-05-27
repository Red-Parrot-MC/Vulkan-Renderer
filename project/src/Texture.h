#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "Image.h"

class Texture
{
public:
	Texture(const char* filename, VkFormat format);
	~Texture();
	bool isOpaque() const { 
		if (this) return opaque;
		else return true;
	}

	uint32_t mipLevels{};
	Image image{};
	VkFormat format{};

private:
	void CreateTextureImage(const char* filename);
	void CreateTextureSampler();
	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
	void CreateDebugNameInfo(const char* name);
	bool opaque{};
};