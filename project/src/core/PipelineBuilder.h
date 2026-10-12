#pragma once
#include <string>
#include <vector>
#include <array>
#include <vulkan/vulkan.h>
#include "DeletionQueue.h"
#include "structs.h"

class PipelineBuilder final
{
public:
	PipelineBuilder();
	~PipelineBuilder();

	PipelineBuilder& AddShader(const std::string shaderPath, VkShaderStageFlagBits shaderStage, const char* pName = "main");
	PipelineBuilder& AddAttributeDescription(VkVertexInputAttributeDescription attributeDescription);
	PipelineBuilder& AddBindingDescription(VkVertexInputBindingDescription bindingDescription);
	PipelineBuilder& SetInputAssembly(VkPrimitiveTopology topology, VkBool32 primitiveRestartEnable);
	PipelineBuilder& SetViewportState(uint32_t viewportCount, uint32_t scissorCount);
	PipelineBuilder& SetRasterizer(VkBool32 depthClampEnable, VkBool32 rasterizerDiscardEnable, VkPolygonMode polygonMode, float lineWidth, VkCullModeFlags cullMode, VkFrontFace frontFace, VkBool32 depthBiasEnable, float depthBiasConstantFactor = 0, float depthBiasClamp = 0, float depthBiasSlopeFactor = 0);
	PipelineBuilder& SetMultisampling(VkBool32 sampleShadingEnable, VkSampleCountFlagBits rasterizationSamples, float minSampleShading = 0, const VkSampleMask* pSampleMask = nullptr, VkBool32 alphaToCoverageEnable = VK_FALSE, VkBool32 alphaToOneEnable = VK_FALSE);
	PipelineBuilder& AddColorBlendAttachment(VkColorComponentFlags colorWriteMask, VkBool32 blendEnable, VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_ONE, VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ONE, VkBlendOp colorBlendOp = VK_BLEND_OP_ADD, VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE, VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD);
	PipelineBuilder& SetColorBlending(VkBool32 logicOpEnable, VkLogicOp logicOp, std::array<float, 4> blendConstants = std::array<float, 4>{ 0, 0, 0, 0 });
	PipelineBuilder& SetDynamicStates(std::vector<VkDynamicState> dynamicStates);
	PipelineBuilder& AddSetLayout(VkDescriptorSetLayout setLayout);
	PipelineBuilder& AddPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size);
	PipelineBuilder& SetDepthStencil(VkBool32 depthTestEnable, VkBool32 depthWriteEnable, VkCompareOp depthCompareOp, VkBool32 depthBoundsTestEnable, VkBool32 stencilTestEnable);
	PipelineBuilder& SetDepthFormat(VkFormat depthAttachmentFormat);
	PipelineBuilder& AddColorAttachmentFormat(VkFormat colorAttachmentFormat);
	PipelineBuilder& SetDebugName(std::string debugName);
	PipelineBuilder& PushDeletionQueue(DeletionQueue& deletionQueue);
	Pipeline Build();


	VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
	VkPipeline getPipeline() const { return pipeline; }

	static VkFormat findDepthFormat();
private:
	static VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	static std::vector<char> readFile(const std::string& filename);
	static VkShaderModule createShaderModule(const std::vector<char>& code);

	std::vector<VkPipelineShaderStageCreateInfo> shaderStages{};
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
	std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	VkPipelineViewportStateCreateInfo viewportState{};
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	VkPipelineMultisampleStateCreateInfo multisampling{};
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments{};
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	std::vector<VkDynamicState> dynamicStates{};
	std::vector<VkDescriptorSetLayout> setLayouts{};
	std::vector<VkPushConstantRange> pushConstantRanges{};
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	VkPipelineRenderingCreateInfo renderingInfo{};
	std::vector<VkFormat> colorAttachmentFormats{};

	VkPipelineLayout pipelineLayout{};
	VkPipeline pipeline{};

	DeletionQueue* deletionQueue{};
	std::string debugName{};
};