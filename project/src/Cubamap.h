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
	VkImage diffuseIrradianceImage{};
	VkImageView diffuseIrradianceView{};

private:
	Image LoadInputImage(std::string filePath, DeletionQueue& deletionQueue);
	VkDescriptorSetLayout CreateSetLayout(DeletionQueue& deletionQueue);
	Pipeline CreatePipeline(std::string vertexShader, std::string fragmentShader, VkDescriptorSetLayout layout, DeletionQueue& deletionQueue);
	void CreateOutputImage(uint32_t size);
	std::array<std::vector<VkImageView>, 6> CreateImageViews(uint32_t size, DeletionQueue& deletionQueue);
	void ConvertSphericalToCubemap(Image inputImage, VkDescriptorSetLayout layout, Pipeline pipeline, std::array<std::vector<VkImageView>, 6>& imageViews, uint32_t cubemapSize, DeletionQueue& deletionQueue);
	void GenerateMipmaps(uint32_t cubemapSize);

	void RenderToCubeMap(std::string vertexShader, std::string fragmentShader, VkImage inputImage, VkImageView inputImageView, VkSampler inputSampler, VkImage& outputCubemapImage);

	VmaAllocation vmaImageAllocation{};
	VmaAllocation vmaDiffuseIrradianceAllocation{};
};