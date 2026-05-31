#pragma once
#include "VulkanContext.h"
#include <stdexcept>

// this sucks

class DescriptorSetBuilder
{
public:
	DescriptorSetBuilder(uint32_t amount)
	{
        descriptorWrites.resize(amount);
	}

    ~DescriptorSetBuilder()
    {
        for (VkDescriptorBufferInfo* buffer : tempBuffers)
        {
            delete buffer;
        }
    }

    DescriptorSetBuilder& AddWrite(uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType, uint32_t descriptorCount, VkDescriptorBufferInfo* bufferInfo)
    {
        VkWriteDescriptorSet writeDescriptorSet{};

        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        //writeDescriptorSet.dstSet = CONTEXT->descriptorSets[i];
        writeDescriptorSet.dstBinding = dstBinding;
        writeDescriptorSet.dstArrayElement = dstArrayElement;
        writeDescriptorSet.descriptorType = descriptorType;
        writeDescriptorSet.descriptorCount = descriptorCount;
        writeDescriptorSet.pBufferInfo = bufferInfo;

        for (size_t i = 0; i < descriptorWrites.size(); i++)
        {
            descriptorWrites[i].push_back(writeDescriptorSet);
        }

        return *this;
    }

	DescriptorSetBuilder& AddWrite(uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType, uint32_t descriptorCount, VkDescriptorBufferInfo& bufferInfo, const std::vector<VkBuffer>& buffers)
	{
        VkWriteDescriptorSet writeDescriptorSet{};

        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        //writeDescriptorSet.dstSet = CONTEXT->descriptorSets[i];
        writeDescriptorSet.dstBinding = dstBinding;
        writeDescriptorSet.dstArrayElement = dstArrayElement;
        writeDescriptorSet.descriptorType = descriptorType;
        writeDescriptorSet.descriptorCount = descriptorCount;

        for (size_t i = 0; i < descriptorWrites.size(); i++)
        {
            VkDescriptorBufferInfo* pBufferInfo = new VkDescriptorBufferInfo(bufferInfo);
            tempBuffers.push_back(pBufferInfo);

            writeDescriptorSet.pBufferInfo = pBufferInfo;
            pBufferInfo->buffer = buffers[i];
            descriptorWrites[i].push_back(writeDescriptorSet);
        }

        return *this;
	}

    DescriptorSetBuilder& AddWrite(uint32_t dstBinding, uint32_t dstArrayElement, VkDescriptorType descriptorType, uint32_t descriptorCount, VkDescriptorImageInfo* pImageInfo)
    {
        VkWriteDescriptorSet writeDescriptorSet{};

        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        //writeDescriptorSet.dstSet = CONTEXT->descriptorSets[i];
        writeDescriptorSet.dstBinding = dstBinding;
        writeDescriptorSet.dstArrayElement = dstArrayElement;
        writeDescriptorSet.descriptorType = descriptorType;
        writeDescriptorSet.descriptorCount = descriptorCount;
        writeDescriptorSet.pImageInfo = pImageInfo;

        for (size_t i = 0; i < descriptorWrites.size(); i++)
        {
            descriptorWrites[i].push_back(writeDescriptorSet);
        }

        return *this;
    }

	std::vector<VkDescriptorSet> Build(VkDescriptorSetLayout descriptorSetLayout)
	{
        std::vector<VkDescriptorSetLayout> layouts(descriptorWrites.size(), descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = CONTEXT->descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(descriptorWrites.size());
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> descriptorSets{};
        descriptorSets.resize(descriptorWrites.size());
        if (vkAllocateDescriptorSets(CONTEXT->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < descriptorWrites.size(); i++) {
            for (size_t writeSet = 0; writeSet < descriptorWrites[i].size(); writeSet++)
            {
                descriptorWrites[i][writeSet].dstSet = descriptorSets[i];

                printf("binding = %llu\n",
                    (unsigned long long)descriptorWrites[i][writeSet].dstBinding);

                if (descriptorWrites[i][writeSet].pBufferInfo)
                {
                    printf("buffer = %p\n",
                        (void*)descriptorWrites[i][writeSet].pBufferInfo->buffer);

                    printf("offset = %llu\n",
                        (unsigned long long)descriptorWrites[i][writeSet].pBufferInfo->offset);

                    printf("range = %llu\n",
                        (unsigned long long)descriptorWrites[i][writeSet].pBufferInfo->range);
                }
            }

            vkUpdateDescriptorSets(CONTEXT->device, static_cast<uint32_t>(descriptorWrites[i].size()), descriptorWrites[i].data(), 0, nullptr);
        }

        return descriptorSets;
	}

private:
	std::vector<std::vector<VkWriteDescriptorSet>> descriptorWrites{};
    std::vector<VkDescriptorBufferInfo*> tempBuffers{};
};