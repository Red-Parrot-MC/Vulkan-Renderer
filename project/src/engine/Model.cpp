#include "Model.h"
#include "../core/DescriptorSetBuilder.h"
#include "../core/BufferHelpers.h"
#include <algorithm>

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

void Primitive::CalculateBounds()
{
    boundingBox = {};
    for (const Vertex& vertex : vertices)
    {
        boundingBox.min = min(vertex.pos, boundingBox.min);
        boundingBox.max = max(vertex.pos, boundingBox.max);
    }
}


void Material::Initialize()
{
    if (color != nullptr && normal != nullptr && metalic_roughness != nullptr)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        if (!color->image.getView() || !normal->image.getView() || !metalic_roughness->image.getView())
        {
            throw std::invalid_argument("Material: image not initialized");
        }

        VkDescriptorImageInfo colorInfo{};
        colorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        colorInfo.imageView = color->image.getView();
        //colorInfo.sampler = color->textureSampler;

        VkDescriptorImageInfo normalInfo{};
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        normalInfo.imageView = normal->image.getView();
        //normalInfo.sampler = normal->textureSampler;

        VkDescriptorImageInfo metalicRoughnessInfo{};
        metalicRoughnessInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        metalicRoughnessInfo.imageView = metalic_roughness->image.getView();
        //metalicRoughnessInfo.sampler = metalic_roughness->textureSampler;

        DescriptorSetBuilder descriptorSetBuilder{ CONTEXT->MAX_FRAMES_IN_FLIGHT };
        descriptorSets = descriptorSetBuilder
            .AddWrite(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, bufferInfo, CONTEXT->uniformBuffers)
            .AddWrite(1, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &colorInfo)
            .AddWrite(2, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &normalInfo)
            .AddWrite(3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &metalicRoughnessInfo)
            .Build(CONTEXT->descriptorSetLayout);
    }
}

Material::~Material()
{
    //delete color;
    //delete normal;
    //delete metalic_roughness;
}

Material::Material(Texture* color, Texture* normal, Texture* metalic_roughness)
    : color{ color }
    , normal{ normal }
    , metalic_roughness{metalic_roughness}
{ }


Light::Light(glm::vec3 pos, float intensity, glm::vec3 color, LightType lightType)
{
    this->pos = pos;
    this->intensity = intensity;
    this->color = color;
    this->lightType = lightType;
}

void Light::SetColorFromKelvin(float kelvin)
{
    const float temp = kelvin / 100.f;
    const float r = (temp <= 66.f) ? 1.f :
        std::clamp(1.29293618606f * pow(temp - 60.f, -0.1332047592f), 0.f, 1.f);
    const float g = (temp <= 66.f) ?
        std::clamp(0.390081578769f * log(temp) - 0.63184144378f, 0.f, 1.f) :
        std::clamp(1.12989086089f * pow(temp - 60.f, -0.0755148492f), 0.f, 1.f);
    const float b = (temp >= 66.f) ? 1.f :
        (temp <= 19.f) ? 0.f :
        std::clamp(0.54320678911f * log(temp - 10.f) - 1.19625408914f, 0.f, 1.f);
    color = { r, g, b };
}


Model::~Model()
{
    for (Primitive* p : primitives)
        delete p;

    for (Material* m : materials)
        delete m;

    for (Texture* t : textures)
        delete t;

    primitives.clear();
    materials.clear();
    textures.clear();
}

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
        vkCmdSetCullMode(commandBuffer, cullMode);

        VkBuffer vertexBuffers[] = { primitives[i]->vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(commandBuffer, primitives[i]->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->geometryPassPipeline.layout, 0, 1, &materials[primitives[i]->materialIndex]->descriptorSets[CONTEXT->currentFrame], 0, nullptr);

        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(primitives[i]->indices.size()), 1, 0, 0, 0);
    }
}

void Model::CalculateBounds()
{
    boundingBox = {};
    for (Primitive* primitive : primitives)
    {
        if (primitive->boundingBox.min == primitive->boundingBox.max) // boundingbox not calculated yet
            primitive->CalculateBounds();

        boundingBox.min = min(boundingBox.min, primitive->boundingBox.min);
        boundingBox.max = max(boundingBox.max, primitive->boundingBox.max);
    }
}