#include "DescriptorSetLayoutBuilder.h"
#include "VulkanContext.h"

DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder()
{

}

DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::AddBinding(uint32_t binding, uint32_t descriptorCount, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, const VkSampler* pImmutableSamplers)
{
	VkDescriptorSetLayoutBinding descriptorBinding{};
	descriptorBinding.binding = binding;
	descriptorBinding.descriptorCount = descriptorCount;
	descriptorBinding.descriptorType = descriptorType;
	descriptorBinding.stageFlags = stageFlags;
	descriptorBinding.pImmutableSamplers = pImmutableSamplers;
	
	bindings.push_back(descriptorBinding);
	return *this;
}

VkDescriptorSetLayout DescriptorSetLayoutBuilder::Build(VkDescriptorSetLayoutCreateFlags flags)
{
	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	createInfo.pBindings = bindings.data();
	createInfo.flags = flags;

	if (vkCreateDescriptorSetLayout(CONTEXT->device, &createInfo, nullptr, &layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	return layout;
}

void DescriptorSetLayoutBuilder::PushDeletionQueue(DeletionQueue& deletionQueue)
{
	VkDescriptorSetLayout layoutToDelete = layout;
	deletionQueue.PushDeletor(
		[layoutToDelete]()
		{
			vkDestroyDescriptorSetLayout(CONTEXT->device, layoutToDelete, nullptr);
		}
	);
}