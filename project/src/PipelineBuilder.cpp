#include "PipelineBuilder.h"
#include "BufferHelpers.h"
#include <fstream>

PipelineBuilder::PipelineBuilder()
{
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
}

PipelineBuilder::~PipelineBuilder()
{
    for (auto& shaderStage : shaderStages)
    {
        vkDestroyShaderModule(CONTEXT->device, shaderStage.module, nullptr);
    }
}

PipelineBuilder& PipelineBuilder::AddShader(const std::string shaderPath, VkShaderStageFlagBits shaderStage, const char* pName)
{
    auto shaderCode = readFile(shaderPath);

    VkShaderModule shaderModule = createShaderModule(shaderCode);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = shaderStage;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = pName;

    shaderStages.push_back(shaderStageInfo);

    return *this;
}

PipelineBuilder& PipelineBuilder::AddAttributeDescription(VkVertexInputAttributeDescription attributeDescription)
{
    attributeDescriptions.push_back(attributeDescription);

    return *this;
}

PipelineBuilder& PipelineBuilder::AddBindingDescription(VkVertexInputBindingDescription bindingDescription)
{
    bindingDescriptions.push_back(bindingDescription);

    return *this;
}

PipelineBuilder& PipelineBuilder::SetInputAssembly(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable)
{
    inputAssembly.topology = topology;
    inputAssembly.primitiveRestartEnable = primitiveRestartEnable;

    return *this;
}

PipelineBuilder& PipelineBuilder::SetViewportState(uint32_t viewportCount, uint32_t scissorCount)
{
    viewportState.viewportCount = viewportCount;
    viewportState.scissorCount = scissorCount;

    return *this;
}

PipelineBuilder& PipelineBuilder::SetRasterizer(VkBool32 depthClampEnable, VkBool32 rasterizerDiscardEnable, VkPolygonMode polygonMode, float lineWidth, VkCullModeFlags cullMode, VkFrontFace frontFace, VkBool32 depthBiasEnable, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor)
{
    rasterizer.depthClampEnable = depthClampEnable;
    rasterizer.rasterizerDiscardEnable = rasterizerDiscardEnable;
    rasterizer.polygonMode = polygonMode;
    rasterizer.lineWidth = lineWidth;
    rasterizer.cullMode = cullMode;
    rasterizer.frontFace = frontFace;
    rasterizer.depthBiasEnable = depthBiasEnable;
    rasterizer.depthBiasConstantFactor = depthBiasConstantFactor;
    rasterizer.depthBiasClamp = depthBiasClamp;
    rasterizer.depthBiasSlopeFactor = depthBiasSlopeFactor;

    return *this;
}

PipelineBuilder& PipelineBuilder::SetMultisampling(VkBool32 sampleShadingEnable, VkSampleCountFlagBits rasterizationSamples, float minSampleShading, const VkSampleMask* pSampleMask, VkBool32 alphaToCoverageEnable, VkBool32 alphaToOneEnable)
{
    multisampling.sampleShadingEnable = sampleShadingEnable;
    multisampling.rasterizationSamples = rasterizationSamples;
    multisampling.minSampleShading = minSampleShading;
    multisampling.pSampleMask = pSampleMask;
    multisampling.alphaToCoverageEnable = alphaToCoverageEnable;
    multisampling.alphaToOneEnable = alphaToOneEnable;

    return *this;
}

PipelineBuilder& PipelineBuilder::AddColorBlendAttachment(VkColorComponentFlags colorWriteMask, VkBool32 blendEnable, VkBlendFactor srcColorBlendFactor, VkBlendFactor dstColorBlendFactor, VkBlendOp colorBlendOp, VkBlendFactor srcAlphaBlendFactor, VkBlendFactor dstAlphaBlendFactor, VkBlendOp alphaBlendOp)
{
    VkPipelineColorBlendAttachmentState attachment{};
    attachment.colorWriteMask = colorWriteMask;
    attachment.blendEnable = blendEnable;
    attachment.srcColorBlendFactor = srcColorBlendFactor;
    attachment.dstColorBlendFactor = dstColorBlendFactor;
    attachment.colorBlendOp = colorBlendOp;
    attachment.srcAlphaBlendFactor = srcAlphaBlendFactor;
    attachment.dstAlphaBlendFactor = dstAlphaBlendFactor;
    attachment.alphaBlendOp = alphaBlendOp;

    colorBlendAttachments.push_back(attachment);

    return *this;
}

PipelineBuilder& PipelineBuilder::SetColorBlending(VkBool32 logicOpEnable, VkLogicOp logicOp, std::array<float, 4> blendConstants)
{
    colorBlending.logicOpEnable = logicOpEnable;
    colorBlending.logicOp = logicOp;
    colorBlending.blendConstants[0] = blendConstants[0];
    colorBlending.blendConstants[1] = blendConstants[1];
    colorBlending.blendConstants[2] = blendConstants[2];
    colorBlending.blendConstants[3] = blendConstants[3];

    return *this;
}

PipelineBuilder& PipelineBuilder::SetDynamicStates(std::vector<VkDynamicState> dynamicStates)
{
    this->dynamicStates = dynamicStates;

    return *this;
}

PipelineBuilder& PipelineBuilder::AddSetLayout(VkDescriptorSetLayout setLayout)
{
    setLayouts.push_back(setLayout);

    return *this;
}

PipelineBuilder& PipelineBuilder::AddPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size)
{
    pushConstantRanges.push_back(VkPushConstantRange{ stageFlags , offset, size });

    return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthStencil(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp, VkBool32 depthBoundsTestEnable, VkBool32 stencilTestEnable)
{
    depthStencil.depthTestEnable = depthTestEnable;
    depthStencil.depthWriteEnable = depthWriteEnable;
    depthStencil.depthCompareOp = depthCompareOp;
    depthStencil.depthBoundsTestEnable = depthBoundsTestEnable;
    depthStencil.stencilTestEnable = stencilTestEnable;

    return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthFormat(VkFormat depthAttachmentFormat)
{
    renderingInfo.depthAttachmentFormat = depthAttachmentFormat;

    return *this;
}

PipelineBuilder& PipelineBuilder::AddColorAttachmentFormat(VkFormat colorAttachmentFormat)
{
    colorAttachmentFormats.push_back(colorAttachmentFormat);

    return *this;
}

PipelineBuilder& PipelineBuilder::SetDebugName(std::string debugName)
{
    if (pipeline)
    {
        std::string name = "Pipeline (" + debugName + ")";

        VkDebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = VK_OBJECT_TYPE_PIPELINE;
        nameInfo.objectHandle = (uint64_t)pipeline;
        nameInfo.pObjectName = name.c_str();
        setDebugUtilsObjectName(CONTEXT->device, &nameInfo);

        name = "Pipeline Layout (" + debugName + ")";

        nameInfo.objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
        nameInfo.objectHandle = (uint64_t)pipelineLayout;
        nameInfo.pObjectName = name.c_str();
        setDebugUtilsObjectName(CONTEXT->device, &nameInfo);
    }

    this->debugName = debugName;
    return *this;
}

PipelineBuilder& PipelineBuilder::PushDeletionQueue(DeletionQueue& deletionQueue)
{
    if (pipeline)
    {
        VkPipeline pipelineToDelete = pipeline;
        VkPipelineLayout layoutToDelete = pipelineLayout;

        deletionQueue.PushDeletor(
            [pipelineToDelete, layoutToDelete]()
            {
                vkDestroyPipeline(CONTEXT->device, pipelineToDelete, nullptr);
                vkDestroyPipelineLayout(CONTEXT->device, layoutToDelete, nullptr);
            }
        );
    }

    this->deletionQueue = &deletionQueue;
    return *this;
}

Pipeline PipelineBuilder::Build()
{
    colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
    colorBlending.pAttachments = colorBlendAttachments.data();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    pipelineLayoutInfo.pSetLayouts = setLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

    if (vkCreatePipelineLayout(CONTEXT->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentFormats.size());
    renderingInfo.pColorAttachmentFormats = colorAttachmentFormats.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional
    pipelineInfo.pDepthStencilState = &depthStencil;

    if (vkCreateGraphicsPipelines(CONTEXT->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    if (deletionQueue)
    {
        PushDeletionQueue(*deletionQueue);
    }

    if (debugName.length() > 0)
    {
        SetDebugName(debugName);
    }

    return { pipeline, pipelineLayout };
}



VkFormat PipelineBuilder::findDepthFormat() {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}



VkFormat PipelineBuilder::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(CONTEXT->physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

std::vector<char> PipelineBuilder::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

VkShaderModule PipelineBuilder::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(CONTEXT->device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
    return shaderModule;
}
