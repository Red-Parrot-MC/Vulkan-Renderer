#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include "Image.h"
#include <array>
#include <glm/glm.hpp>
#include "structs.h"
#include "DeletionQueue.h"

class Cubemap
{
public:
	Cubemap();
	~Cubemap();

	void Load(std::string filePath, uint32_t cubemapSize = 1024);
	void Destroy();

	//VkImage cubemapImage{};
	//VkImageView cubemapView{};
	Image cubemapImage{};
	Image diffuseIrradianceImage{};

private:
	Image LoadInputImage(std::string filePath, DeletionQueue& deletionQueue);
	VkDescriptorSetLayout CreateSetLayout(DeletionQueue& deletionQueue);
	Pipeline CreatePipeline(std::string vertexShader, std::string fragmentShader, VkDescriptorSetLayout layout, VkFormat colorAttachmentFormat, DeletionQueue& deletionQueue);
	void CreateOutputImage(Image& image, uint32_t size, uint32_t mipLevels);
	std::array<std::vector<VkImageView>, 6> CreateImageViews(Image image, uint32_t mipLevels, DeletionQueue& deletionQueue);
	void ConvertSphericalToCubemap(Image inputImage, VkDescriptorSetLayout layout, Pipeline pipeline, std::array<std::vector<VkImageView>, 6>& imageViews, uint32_t cubemapSize, DeletionQueue& deletionQueue);
	void GenerateMipmaps(uint32_t cubemapSize);

	void RenderToCubeMap(std::string vertexShader, std::string fragmentShader, Image inputImage, VkSampler inputSampler, Image& outputCubemapImage);

	VmaAllocation vmaImageAllocation{};
	VmaAllocation vmaDiffuseIrradianceAllocation{};
};