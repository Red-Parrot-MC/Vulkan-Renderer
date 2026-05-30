#pragma once
#include "structs.h"
#include "Model.h"

struct DirectionalShadowLightUniformBuffer
{
	glm::mat4 model{ 1 };
	glm::mat4 view{};
	glm::mat4 proj{};
};

struct DirectionalShadowLightBufferObject
{
	glm::vec3 direction{};
	float intensity{};
	glm::vec3 color{};
	uint32_t shadowLayer{};
	glm::mat4 view{};
	glm::mat4 proj{};
};

class Scene;

class DirectionalShadowLight
{
	bool initialized{ false };
	DirectionalShadowLightBufferObject bufferObject{};
	VkImageView imageView{}; // view for rendering depth

	static Pipeline pipeline;
	static VkDescriptorSetLayout descriptorSetLayout;
	static VkDescriptorSet descriptorSet;
	static VkBuffer buffer;
	static VkDeviceMemory bufferMemory;
	static void* bufferMapped;
	static uint32_t lightCount;
	static Image shadowMaps;

public:

	DirectionalShadowLight(Light directionalLight, const Scene& scene);

	void UpdateDepthMap(const Scene& scene);
	void EditLight(Light newDirectionalLight);

	void Destroy();

	DirectionalShadowLightBufferObject* GetBufferObject() { return &bufferObject; }
	static Image GetShadowMaps() { return shadowMaps; }

private:

	static void CalculateLightBounds(BoundingBox boundingBox, glm::vec3 lightDirection, glm::mat4& outView, glm::mat4& outProj);

	static void InitializePipeline();
};