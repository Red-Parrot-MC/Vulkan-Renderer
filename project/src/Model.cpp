#include "Model.h"
#include "DescriptorSetBuilder.h"
#include "BufferHelpers.h"

Primitive::~Primitive()
{
    vkDestroyBuffer(CONTEXT->device, indexBuffer, nullptr);
    vkFreeMemory(CONTEXT->device, indexBufferMemory, nullptr);

    vkDestroyBuffer(CONTEXT->device, vertexBuffer, nullptr);
    vkFreeMemory(CONTEXT->device, vertexBufferMemory, nullptr);
}

void Primitive::Initialize()
{
    createVertexBuffer();
    createIndexBuffer();
}

void Primitive::createVertexBuffer() {
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

void Primitive::createIndexBuffer() {
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


void Material::Initialize()
{
    if (color != nullptr)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = color->textureImageView;
        imageInfo.sampler = color->textureSampler;

        DescriptorSetBuilder descriptorSetBuilder{};
        descriptorSets = descriptorSetBuilder
            .AddWrite(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, bufferInfo, CONTEXT->uniformBuffers)
            .AddWrite(1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageInfo)
            .Build();
    }
}

Material::~Material()
{
    delete color;
    delete normal;
    delete metalic_roughness;
}

Material::Material(Texture* color)
    : color{color}
{ }
Material::Material(Texture* color, Texture* normal)
    : color{color}
    , normal{normal}
{ }
Material::Material(Texture* color, Texture* normal, Texture* metalic_roughness)
    : color{ color }
    , normal{ normal }
    , metalic_roughness{metalic_roughness}
{ }


void Model::Initialize()
{
    for (Material* m : materials)
        m->Initialize();

    for (Primitive* p : primitives)
        p->Initialize();
}

void Model::RecordCommandBuffer(VkCommandBuffer commandBuffer)
{
    for (size_t i = 0; i < primitives.size(); i++)
    {
        VkBuffer vertexBuffers[] = { primitives[i]->vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(commandBuffer, primitives[i]->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->pipelineLayout, 0, 1, &materials[primitives[i]->materialIndex]->descriptorSets[CONTEXT->currentFrame], 0, nullptr);

        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(primitives[i]->indices.size()), 1, 0, 0, 0);
    }
}

Model::~Model()
{
    for (Primitive* p : primitives)
        delete p;

    for (Material* m : materials)
        delete m;

    primitives.clear();
}
