#include "DirectionalShadowLight.h"
#include "Scene.h"
#include "PipelineBuilder.h"
#include "DescriptorSetLayoutBuilder.h"
#include "BufferHelpers.h"
#include "DescriptorSetBuilder.h"
#include <iostream>

Pipeline DirectionalShadowLight::pipeline = Pipeline{ VK_NULL_HANDLE, VK_NULL_HANDLE };
VkDescriptorSetLayout DirectionalShadowLight::descriptorSetLayout = VK_NULL_HANDLE;
VkDescriptorSet DirectionalShadowLight::descriptorSet = VK_NULL_HANDLE;
VkBuffer DirectionalShadowLight::buffer = VK_NULL_HANDLE;
VkDeviceMemory DirectionalShadowLight::bufferMemory = VK_NULL_HANDLE;
void* DirectionalShadowLight::bufferMapped = nullptr;
uint32_t DirectionalShadowLight::lightCount = 0;
Image DirectionalShadowLight::shadowMaps = Image{};

DirectionalShadowLight::DirectionalShadowLight(Light directionalLight, const Scene& scene)
{
	if (directionalLight.lightType != LightType::Directional) {
		std::cerr << "Error: lightType is not directional";
		return;
	}

	if (!pipeline.pipeline) InitializePipeline();

	bufferObject.direction = directionalLight.pos;
	bufferObject.intensity = directionalLight.intensity;
	bufferObject.color = directionalLight.color;
	bufferObject.shadowLayer = lightCount++;

	VkImageSubresourceRange subResourceRange{};
	subResourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	subResourceRange.levelCount = 1;
	subResourceRange.layerCount = 1;
	subResourceRange.baseMipLevel = 0;
	subResourceRange.baseArrayLayer = bufferObject.shadowLayer;

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.subresourceRange = subResourceRange;
	viewInfo.image = shadowMaps.getImage();
	viewInfo.format = shadowMaps.getFormat();
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

	vkCreateImageView(CONTEXT->device, &viewInfo, nullptr, &imageView);

	initialized = true;

	UpdateDepthMap(scene);
}

void DirectionalShadowLight::UpdateDepthMap(const Scene& scene)
{
	if (!pipeline.pipeline) InitializePipeline();
	CalculateLightBounds(scene.getBounds(), bufferObject.direction, bufferObject.view, bufferObject.proj);

	DirectionalShadowLightUniformBuffer uniformBuffer{};
	uniformBuffer.proj = bufferObject.proj;
	uniformBuffer.view = bufferObject.view;

	memcpy(bufferMapped, &uniformBuffer, sizeof(uniformBuffer));

	VkViewport viewport{};
	viewport.width = static_cast<float>(shadowMaps.getExtent().width);
	viewport.height = static_cast<float>(shadowMaps.getExtent().height);
	viewport.minDepth = 0;
	viewport.maxDepth = 1;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { shadowMaps.getExtent().width, shadowMaps.getExtent().height };

	VkImageSubresourceRange subResourceRange{};
	subResourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	subResourceRange.levelCount = 1;
	subResourceRange.layerCount = 1;
	subResourceRange.baseMipLevel = 0;
	subResourceRange.baseArrayLayer = bufferObject.shadowLayer;

	VkRenderingAttachmentInfo depthAttachment = {};
	depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.imageView = imageView;
	depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.clearValue = { 1.0f, 0 };

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, scissor.extent };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 0;
	renderingInfo.pColorAttachments = nullptr;
	renderingInfo.pDepthAttachment = &depthAttachment;
	renderingInfo.pStencilAttachment = nullptr;

	VkCommandBuffer cmd = BufferHelpers::beginSingleTimeCommands();
	{
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		shadowMaps.TransitionLayout(cmd, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, subResourceRange);

		vkCmdBeginRendering(cmd, &renderingInfo);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 1, &descriptorSet, 0, nullptr);

		for (auto model : scene.models)
		{
			for (auto primitive : model->primitives)
			{
				if (primitive->isOpaque)
				{
					VkDeviceSize offsets[] = { 0 };
					vkCmdBindVertexBuffers(cmd, 0, 1, &primitive->vertexBuffer, offsets);
					vkCmdBindIndexBuffer(cmd, primitive->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(cmd, static_cast<uint32_t>(primitive->indices.size()), 1, 0, 0, 0);
				}
			}
		}

		vkCmdEndRendering(cmd);

		shadowMaps.TransitionLayout(cmd, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, subResourceRange);
	}
	BufferHelpers::endSingleTimeCommands(cmd);
}

void DirectionalShadowLight::EditLight(Light light)
{
	if (light.lightType != LightType::Directional) {
		std::cerr << "Error: lightType is not directional";
		return;
	}

	bufferObject.direction = light.pos;
	bufferObject.intensity = light.intensity;
	bufferObject.color = light.color;
}

void DirectionalShadowLight::Destroy()
{
	vkDestroyImageView(CONTEXT->device, imageView, nullptr);
}



void DirectionalShadowLight::CalculateLightBounds(BoundingBox boundingBox, glm::vec3 lightDirection, glm::mat4& outView, glm::mat4& outProj)
{
	lightDirection = normalize(lightDirection);

	glm::vec3 bmin = boundingBox.min;
	glm::vec3 bmax = boundingBox.max;

	std::array<glm::vec3, 8> boundCorners{
		glm::vec3{ bmin.x, bmin.y, bmin.z },
		glm::vec3{ bmax.x, bmin.y, bmin.z },
		glm::vec3{ bmin.x, bmax.y, bmin.z },
		glm::vec3{ bmax.x, bmax.y, bmin.z },
		glm::vec3{ bmin.x, bmin.y, bmax.z },
		glm::vec3{ bmax.x, bmin.y, bmax.z },
		glm::vec3{ bmin.x, bmax.y, bmax.z },
		glm::vec3{ bmax.x, bmax.y, bmax.z },
	};

	// Compute scene center
	glm::vec3 center = (bmin + bmax) * 0.5f;

	// Position light far enough back
	float distance = glm::length(bmax - bmin);

	glm::vec3 eye = center - lightDirection * distance;

	outView = glm::lookAt(
		eye,
		center,
		glm::vec3(0, 1, 0));

	glm::vec3 min{ FLT_MAX }, max{ -FLT_MAX };

	for (int i = 0; i < boundCorners.size(); i++)
	{
		glm::vec3 corner = outView * glm::vec4(boundCorners[i], 1);

		min = glm::min(min, corner);
		max = glm::max(max, corner);
	}

	outProj = glm::ortho(
		min.x,
		max.x,
		min.y,
		max.y,
		-max.z,
		-min.z);

	outProj[1][1] *= -1;
}

void DirectionalShadowLight::InitializePipeline()
{
	if (pipeline.pipeline) return; // already initialized

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = CONTEXT->SHADOW_MAP_RESOLUTION.x;
	imageInfo.extent.height = CONTEXT->SHADOW_MAP_RESOLUTION.y;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = CONTEXT->MAX_SHADOW_LIGHTS;
	imageInfo.format = VK_FORMAT_D32_SFLOAT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	shadowMaps.Create(imageInfo, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
	shadowMaps.SetDebugNameInfo("Directional light shadow maps");
	shadowMaps.TransitionLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

	CONTEXT->applicationDeletionQueue.PushDeletor(
		[]()
		{
			DirectionalShadowLight::shadowMaps.Destroy();
		}
	);

	DescriptorSetLayoutBuilder setLayoutBuilder{};
	descriptorSetLayout = setLayoutBuilder
		.AddBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
		.PushDeletionQueue(CONTEXT->applicationDeletionQueue)
		.Build();

	PipelineBuilder pipelineBuilder{};
	pipelineBuilder
		.AddShader("shaders/depth_vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
		.AddShader("shaders/depth_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBindingDescription(Vertex::getBindingDescription())
		.SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
		.SetViewportState(1, 1)
		.SetRasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, 1.0f, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_TRUE, 1.25f, 0, 1.75f)
		.SetMultisampling(VK_FALSE, VK_SAMPLE_COUNT_1_BIT)
		.SetColorBlending(VK_FALSE, VK_LOGIC_OP_COPY)
		.SetDynamicStates({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
		.AddSetLayout(descriptorSetLayout)
		.SetDepthStencil(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, VK_FALSE)
		.SetDepthFormat(PipelineBuilder::findDepthFormat())
		.PushDeletionQueue(CONTEXT->applicationDeletionQueue)
		.SetDebugName("directional light");

	for (auto& description : Vertex::getAttributeDescriptions())
		pipelineBuilder.AddAttributeDescription(description);

	pipeline = pipelineBuilder.Build();

	BufferHelpers::createBuffer(sizeof(DirectionalShadowLightUniformBuffer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffer, bufferMemory);
	vkMapMemory(CONTEXT->device, bufferMemory, 0, sizeof(DirectionalShadowLightUniformBuffer), 0, &bufferMapped);

	CONTEXT->applicationDeletionQueue.PushDeletor(
		[]()
		{
			for (size_t i = 0; i < CONTEXT->MAX_FRAMES_IN_FLIGHT; i++) {
				vkDestroyBuffer(CONTEXT->device, DirectionalShadowLight::buffer, nullptr);
				vkFreeMemory(CONTEXT->device, DirectionalShadowLight::bufferMemory, nullptr);
				DirectionalShadowLight::buffer = VK_NULL_HANDLE;
				DirectionalShadowLight::bufferMemory = VK_NULL_HANDLE;
			}
		}
	);

	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.offset = 0;
	bufferInfo.range = sizeof(DirectionalShadowLightUniformBuffer);
	bufferInfo.buffer = buffer;

	DescriptorSetBuilder setBuilder{};
	descriptorSet = setBuilder
		.AddWrite(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, bufferInfo)
		.Build(descriptorSetLayout)[0];
}