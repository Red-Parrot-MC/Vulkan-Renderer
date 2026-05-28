#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "DeletionQueue.h"

class DescriptorSetLayoutBuilder
{
public:
	DescriptorSetLayoutBuilder();

	DescriptorSetLayoutBuilder& AddBinding(uint32_t binding, uint32_t descriptorCount, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, const VkSampler* pImmutableSamplers = nullptr);
	VkDescriptorSetLayout Build(VkDescriptorSetLayoutCreateFlags flags = 0);
	DescriptorSetLayoutBuilder& PushDeletionQueue(DeletionQueue& deletionQueue);

private:
	std::vector<VkDescriptorSetLayoutBinding> bindings{};
	VkDescriptorSetLayout layout{};
	DeletionQueue* deletionQueue{};
};