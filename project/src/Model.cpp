#include "Model.h"
#include "DescriptorSetBuilder.h"
#include "BufferHelpers.h"

Model::~Model()
{
    vkDestroyBuffer(CONTEXT->device, indexBuffer, nullptr);
    vkFreeMemory(CONTEXT->device, indexBufferMemory, nullptr);

    vkDestroyBuffer(CONTEXT->device, vertexBuffer, nullptr);
    vkFreeMemory(CONTEXT->device, vertexBufferMemory, nullptr);
}

void Model::Initialize()
{
	if (!textures.empty())
	{
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textures[0]->textureImageView;
        imageInfo.sampler = textures[0]->textureSampler;

        DescriptorSetBuilder descriptorSetBuilder{};
        descriptorSets = descriptorSetBuilder
            .AddWrite(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, bufferInfo, CONTEXT->uniformBuffers)
            .AddWrite(1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo)
            .Build();
	}

    createVertexBuffer();
    createIndexBuffer();
}

void Model::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    BufferHelpers::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(CONTEXT->device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(CONTEXT->device, stagingBufferMemory);

    BufferHelpers::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

    BufferHelpers::copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(CONTEXT->device, stagingBuffer, nullptr);
    vkFreeMemory(CONTEXT->device, stagingBufferMemory, nullptr);
}

void Model::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    BufferHelpers::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(CONTEXT->device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(CONTEXT->device, stagingBufferMemory);

    BufferHelpers::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

    BufferHelpers::copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(CONTEXT->device, stagingBuffer, nullptr);
    vkFreeMemory(CONTEXT->device, stagingBufferMemory, nullptr);
}