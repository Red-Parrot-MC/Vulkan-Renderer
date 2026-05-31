#include "Cubamap.h"
#include "CreateInfoHelpers.h"
#include <stb_image.h>
#include <algorithm>
#include "ImageHelpers.h"
#include "DescriptorSetLayoutBuilder.h"
#include "PipelineBuilder.h"
#include "DescriptorSetBuilder.h"

struct CubemapPushConstants
{
    glm::mat4 projection;
    glm::mat4 view;
};

Cubemap::Cubemap()
{

}

Cubemap::~Cubemap()
{
    Destroy();
}

void Cubemap::Load(std::string filePath, uint32_t cubemapSize)
{
    DeletionQueue deletionQueue{};
    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(cubemapSize))) + 1;

    Image inputImage = LoadInputImage(filePath, deletionQueue);
    CreateOutputImage(cubemapImage, cubemapSize, mipLevels);
    VkSampler sampler = CreateInfoHelpers::CreateSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, &deletionQueue);
    RenderToCubeMap("shaders/cubemap_vert.spv", "shaders/cubemap_frag.spv", inputImage, sampler, cubemapImage);
    GenerateMipmaps(cubemapSize);

    CreateOutputImage(diffuseIrradianceImage, 64, 1);
    sampler = CreateInfoHelpers::CreateSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, &deletionQueue);
    RenderToCubeMap("shaders/cubemap_vert.spv", "shaders/cubemap_diffuse_frag.spv", cubemapImage, sampler, diffuseIrradianceImage);
    diffuseIrradianceImage.TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    deletionQueue.Flush();
}

Image Cubemap::LoadInputImage(std::string filePath, DeletionQueue& deletionQueue)
{
    // Assumes we use VK_FORMAT_R32G32B32A32_SFLOAT
    int w = 0, h = 0, texChannels = 0;
    float* const pixels = stbi_loadf(filePath.c_str(),
        &w, &h, &texChannels, 4);

    if (!pixels)
    {
        throw std::runtime_error("failed to load cubamap image!");
    }

    uint64_t imageSize = static_cast<uint64_t>(w) * h * 4 * sizeof(float);
    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    BufferHelpers::createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data;
    vkMapMemory(CONTEXT->device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(CONTEXT->device, stagingBufferMemory);

    stbi_image_free(pixels);

    Image image{};
    image.Create(w, h, 1,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    image.SetDebugNameInfo("Cubemap Input");

    image.TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    ImageHelpers::copyBufferToImage(stagingBuffer, image.getImage(), w, h);

    vkDestroyBuffer(CONTEXT->device, stagingBuffer, nullptr);
    vkFreeMemory(CONTEXT->device, stagingBufferMemory, nullptr);

    deletionQueue.PushDeletor(
        [image]()
        {
            image.DestroyConst();
        }
    );

    return image;
}

VkDescriptorSetLayout Cubemap::CreateSetLayout(DeletionQueue& deletionQueue)
{
    DescriptorSetLayoutBuilder layoutBuilder{};
    layoutBuilder
        .AddBinding(0, 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBinding(1, 1, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .PushDeletionQueue(deletionQueue);

    return layoutBuilder.Build();
}

Pipeline Cubemap::CreatePipeline(std::string vertexShader, std::string fragmentShader, VkDescriptorSetLayout layout, VkFormat colorAttachmentFormat, DeletionQueue& deletionQueue)
{
    PipelineBuilder builder{};
    builder
        .AddShader(vertexShader, VK_SHADER_STAGE_VERTEX_BIT)
        .AddShader(fragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT)
        .AddBindingDescription(Vertex::getBindingDescription())
        .SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
        .SetViewportState(1, 1)
        .SetRasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, 1.0f, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE)
        .SetMultisampling(VK_FALSE, VK_SAMPLE_COUNT_1_BIT)
        .SetColorBlending(VK_FALSE, VK_LOGIC_OP_COPY)
        .SetDynamicStates({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
        .AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CubemapPushConstants))
        .AddColorAttachmentFormat(colorAttachmentFormat)
        .AddColorBlendAttachment(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE)
        .AddSetLayout(layout)
        .PushDeletionQueue(deletionQueue);

    return builder.Build();
}

void Cubemap::CreateOutputImage(Image& image, uint32_t size, uint32_t mipLevels)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = size;
    imageInfo.extent.height = size;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 6;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    image.Create(imageInfo, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_CUBE);
}

std::array<std::vector<VkImageView>, 6> Cubemap::CreateImageViews(Image image, uint32_t mipLevels, DeletionQueue& deletionQueue)
{
    std::array<std::vector<VkImageView>, 6> imageViews{};

    for (auto& views : imageViews)
    {
        views.resize(mipLevels);
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image.getImage();
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    for (uint32_t arrayLayer = 0; arrayLayer < 6; arrayLayer++)
    {
        for (uint32_t mipLevel = 0; mipLevel < mipLevels; mipLevel++)
        {
            viewInfo.subresourceRange.baseArrayLayer = arrayLayer;
            viewInfo.subresourceRange.baseMipLevel = mipLevel;

            VkImageView view{};

            vkCreateImageView(CONTEXT->device, &viewInfo, nullptr, &view);

            imageViews[arrayLayer][mipLevel] = view;
        }
    }

    deletionQueue.PushDeletor(
        [imageViews]()
        {
            for (auto& views : imageViews)
            {
                for (auto& view : views)
                {
                    vkDestroyImageView(CONTEXT->device, view, nullptr);
                }
            }
        }
    );

    return imageViews;
}

void Cubemap::ConvertSphericalToCubemap(Image inputImage, VkDescriptorSetLayout layout, Pipeline pipeline, std::array<std::vector<VkImageView>, 6>& imageViews, uint32_t cubemapSize, DeletionQueue& deletionQueue)
{
    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(cubemapSize))) + 1;

    glm::vec3 eye = glm::vec3(0.0f); // Center of cube position
    glm::mat4 captureViews[6] = {
        glm::lookAt(eye, eye + glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +X
        glm::lookAt(eye, eye + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // -X
        glm::lookAt(eye, eye + glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)), // -Y
        glm::lookAt(eye, eye + glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)), // +Y
        glm::lookAt(eye, eye + glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +Z
        glm::lookAt(eye, eye + glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // -Z
    };

    glm::mat4 captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    captureProj[1][1] *= -1.0f; // Inverted Y-axis for mathing face orientation

    VkViewport viewport{};
    viewport.width = static_cast<float>(cubemapSize);
    viewport.height = static_cast<float>(cubemapSize);
    viewport.minDepth = 0;
    viewport.maxDepth = 1;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { cubemapSize, cubemapSize };

    VkRenderingAttachmentInfo cubemapAttachment{};
    cubemapAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    cubemapAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    cubemapAttachment.imageView = imageViews[0][0];

    inputImage.TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ cubemapSize, cubemapSize } };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &cubemapAttachment;
    renderInfo.pDepthAttachment = nullptr;
    renderInfo.pStencilAttachment = nullptr;

    VkSamplerCreateInfo samplerCreateInfo = CreateInfoHelpers::GetSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

    VkSampler sampler{};
    vkCreateSampler(CONTEXT->device, &samplerCreateInfo, nullptr, &sampler);
    deletionQueue.PushDeletor(
        [sampler]()
        {
            vkDestroySampler(CONTEXT->device, sampler, nullptr);
        }
    );

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = inputImage.getView();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo samplerInfo{};
    samplerInfo.sampler = sampler;

    DescriptorSetBuilder builder{ 1 };
    VkDescriptorSet descriptorSet = builder
        .AddWrite(0, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, &imageInfo)
        .AddWrite(1, 0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, &samplerInfo)
        .Build(layout)[0];

    std::vector<VkRenderingAttachmentInfo> cubemapAttachments{ 6 };
    std::vector<VkRenderingInfo> renderInfos{ 6 };

    for (uint32_t i = 0; i < 6; i++)
    {
        cubemapAttachment.imageView = imageViews[i][0];
        cubemapAttachments[i] = cubemapAttachment;

        renderInfo.pColorAttachments = &cubemapAttachments[i];
        renderInfos[i] = renderInfo;
    }

    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 6;

    cubemapImage.TransitionLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, subresourceRange);

    // COMMAND BUFFER
    VkCommandBuffer cmd = BufferHelpers::beginSingleTimeCommands();
    {
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 1, &descriptorSet, 0, nullptr);

        for (uint32_t i = 0; i < 6; i++)
        {
            vkCmdBeginRendering(cmd, &renderInfos[i]);
            {
                CubemapPushConstants pushConstants{ captureProj, captureViews[i] };
                vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CubemapPushConstants), &pushConstants);
                vkCmdDraw(cmd, 36, 1, 0, 0);
            }
            vkCmdEndRendering(cmd);
        }
    }
    BufferHelpers::endSingleTimeCommands(cmd);
    vkDeviceWaitIdle(CONTEXT->device);
}

void Cubemap::GenerateMipmaps(uint32_t cubemapSize)
{
    uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(cubemapSize))) + 1;
    
    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = mipLevels;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 6;
    subresourceRange.baseArrayLayer = 0;

    cubemapImage.TransitionLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
    ImageHelpers::generateMipmaps(cubemapImage.getImage(), VK_FORMAT_R32G32B32A32_SFLOAT, cubemapSize, cubemapSize, mipLevels, subresourceRange);
}

void Cubemap::RenderToCubeMap(std::string vertexShader, std::string fragmentShader, Image inputImage, VkSampler inputSampler, Image& outputCubemapImage)
{
    DeletionQueue deletionQueue{};

    VkDescriptorSetLayout setLayout = CreateSetLayout(deletionQueue);
    Pipeline pipeline = CreatePipeline(vertexShader, fragmentShader, setLayout, inputImage.getFormat(), deletionQueue);
    auto imageViews = CreateImageViews(outputCubemapImage, outputCubemapImage.getMipLevels(), deletionQueue);

    glm::vec3 eye = glm::vec3(0.0f); // Center of cube position
    glm::mat4 captureViews[6] = {
        glm::lookAt(eye, eye + glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +X
        glm::lookAt(eye, eye + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // -X
        glm::lookAt(eye, eye + glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)), // -Y
        glm::lookAt(eye, eye + glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)), // +Y
        glm::lookAt(eye, eye + glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // +Z
        glm::lookAt(eye, eye + glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // -Z
    };

    glm::mat4 captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    captureProj[1][1] *= -1.0f; // Inverted Y-axis for mathing face orientation

    VkViewport viewport{};
    viewport.width = static_cast<float>(outputCubemapImage.getExtent().width);
    viewport.height = static_cast<float>(outputCubemapImage.getExtent().height);
    viewport.minDepth = 0;
    viewport.maxDepth = 1;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { outputCubemapImage.getExtent().width, outputCubemapImage.getExtent().height };

    VkRenderingAttachmentInfo cubemapAttachment{};
    cubemapAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    cubemapAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    cubemapAttachment.imageView = imageViews[0][0];

    inputImage.TransitionLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, scissor.extent };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &cubemapAttachment;
    renderInfo.pDepthAttachment = nullptr;
    renderInfo.pStencilAttachment = nullptr;

    VkSamplerCreateInfo samplerCreateInfo = CreateInfoHelpers::GetSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = inputImage.getView();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo samplerInfo{};
    samplerInfo.sampler = inputSampler;

    DescriptorSetBuilder builder{ 1 };
    VkDescriptorSet descriptorSet = builder
        .AddWrite(0, 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, &imageInfo)
        .AddWrite(1, 0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, &samplerInfo)
        .Build(setLayout)[0];

    std::vector<VkRenderingAttachmentInfo> cubemapAttachments{ 6 };
    std::vector<VkRenderingInfo> renderInfos{ 6 };

    for (uint32_t i = 0; i < 6; i++)
    {
        cubemapAttachment.imageView = imageViews[i][0];
        cubemapAttachments[i] = cubemapAttachment;

        renderInfo.pColorAttachments = &cubemapAttachments[i];
        renderInfos[i] = renderInfo;
    }

    VkImageSubresourceRange subresourceRange{};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = outputCubemapImage.getMipLevels();
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 6;

    outputCubemapImage.TransitionLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, subresourceRange);

    // COMMAND BUFFER
    VkCommandBuffer cmd = BufferHelpers::beginSingleTimeCommands();
    {
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 1, &descriptorSet, 0, nullptr);

        for (uint32_t i = 0; i < 6; i++)
        {
            vkCmdBeginRendering(cmd, &renderInfos[i]);
            {
                CubemapPushConstants pushConstants{ captureProj, captureViews[i] };
                vkCmdPushConstants(cmd, pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CubemapPushConstants), &pushConstants);
                vkCmdDraw(cmd, 36, 1, 0, 0);
            }
            vkCmdEndRendering(cmd);
        }
    }
    BufferHelpers::endSingleTimeCommands(cmd);

    deletionQueue.Flush();
}


void Cubemap::Destroy()
{
    cubemapImage.Destroy();
    diffuseIrradianceImage.Destroy();
}