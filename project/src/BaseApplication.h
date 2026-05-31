#pragma once
#include "VulkanSetup.h"
#include "Texture.h"
#include "BufferHelpers.h"
#include "structs.h"
#include <ctime>
#include <chrono>
#include <tiny_obj_loader.h>
#include <iostream>
#include "Scene.h"
#include "Camera.h"
#include "ImageHelpers.h"
#include "DescriptorSetBuilder.h"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
const std::string WINDOW_NAME = "Vulkan";

class Scene;

class BaseApplication
{
public:
	void run()
	{
        CONTEXT = new VulkanContext();
		initWindow();
		initVulkan();
        Initialize();
        RecordCommandBuffers();
        CreateSubmitInfos();
		mainLoop();
        Cleanup();
        this->~BaseApplication();
		cleanupVulkan();
	}

protected:
    Scene* scene{};
    Camera camera{};
    virtual ~BaseApplication() {};

private:
    virtual void Initialize() {}
    virtual void Update(float deltaTime) {}
    virtual void Cleanup() {}

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		CONTEXT->window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(CONTEXT->window, this);
		glfwSetFramebufferSizeCallback(CONTEXT->window, framebufferResizeCallback);
	}

	void initVulkan()
	{
        VulkanSetup::InitializeVulkan();
        scene = new Scene{};
	}

	void mainLoop()
	{
        clock_t fpsTimerStart = clock();
        clock_t prevFrame = clock();
        int fpsFrameCount = 0;

        std::string title = WINDOW_NAME + " - FPS: ???";
        glfwSetWindowTitle(CONTEXT->window, title.c_str());

        while (!glfwWindowShouldClose(CONTEXT->window)) {
            glfwPollEvents();

            float deltaTime = (clock() - prevFrame) / float(CLOCKS_PER_SEC);
            prevFrame = clock();

            Update(deltaTime);
            camera.Update(deltaTime);
            drawFrame();

            fpsFrameCount++;
            if (clock() - fpsTimerStart >= CLOCKS_PER_SEC) {
                // Add FPS to window title
                title = WINDOW_NAME + " - FPS: " + std::to_string(fpsFrameCount);
                glfwSetWindowTitle(CONTEXT->window, title.c_str());

                fpsFrameCount = 0;
                fpsTimerStart = clock();
            }
        }

        vkDeviceWaitIdle(CONTEXT->device);
	}

	void cleanupVulkan()
	{
        delete scene;
        DestroySubmitInfos();
        VulkanSetup::cleanup();

        glfwDestroyWindow(CONTEXT->window);
        glfwTerminate();
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		CONTEXT->framebufferResized = true;
	}

    void drawFrame()
    {
        vkWaitForFences(CONTEXT->device, 1, &CONTEXT->inFlightFences[CONTEXT->currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(CONTEXT->device, CONTEXT->swapChain, UINT64_MAX, CONTEXT->imageAvailableSemaphores[CONTEXT->currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || CONTEXT->framebufferResized) {
            CONTEXT->framebufferResized = false;
            VulkanSetup::recreateSwapChain();
            RecordCommandBuffers();
            CreateSubmitInfos();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        updateUniformBuffer(CONTEXT->currentFrame);

        // Only reset the fence if we are submitting work
        vkResetFences(CONTEXT->device, 1, &CONTEXT->inFlightFences[CONTEXT->currentFrame]);

        if (vkQueueSubmit2(CONTEXT->graphicsQueue, 1, &CONTEXT->drawFrameSubmitInfos[CONTEXT->currentFrame], CONTEXT->inFlightFences[CONTEXT->currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkSemaphore signalSemaphores[] = { CONTEXT->renderFinishedSemaphores[imageIndex] };

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { CONTEXT->swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(CONTEXT->presentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            VulkanSetup::recreateSwapChain();
            RecordCommandBuffers();
            CreateSubmitInfos();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        CONTEXT->currentFrame = (CONTEXT->currentFrame + 1) % CONTEXT->MAX_FRAMES_IN_FLIGHT;
    }

    void RecordCommandBuffers()
    {
        for (uint32_t imageIndex = 0; imageIndex < CONTEXT->MAX_FRAMES_IN_FLIGHT; imageIndex++)
        {
            RecordCommandBuffer(imageIndex);
        }
    }

    void RecordCommandBuffer(uint32_t imageIndex)
    {
#pragma region Variable setup

        VkCommandBuffer& commandBuffer = CONTEXT->commandBuffers[imageIndex];
        vkResetCommandBuffer(commandBuffer, /*VkCommandBufferResetFlagBits*/ 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // Set attachments for dynamic rendering
        VkRenderingAttachmentInfo colorAttachment = {};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = CONTEXT->swapChainImageViews[imageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = { {0.0f, 0.0f, 0.0f, 1.0f} };

        VkRenderingAttachmentInfo depthPassDepthAttachment = {};
        depthPassDepthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthPassDepthAttachment.imageView = CONTEXT->depthImage.getView();
        depthPassDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthPassDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthPassDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthPassDepthAttachment.clearValue = { 1.0f, 0 };

        VkRenderingAttachmentInfo geometryPassDepthAttachment = {};
        geometryPassDepthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        geometryPassDepthAttachment.imageView = CONTEXT->depthImage.getView();
        geometryPassDepthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        geometryPassDepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        geometryPassDepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        geometryPassDepthAttachment.clearValue = { 1.0f, 0 };

        std::vector<VkRenderingAttachmentInfo> geometryColorAttachments{};
        std::vector<VkRenderingAttachmentInfo> lightingColorAttachments{ colorAttachment };

        for (gBuffer* b : CONTEXT->gBuffers)
        {
            VkRenderingAttachmentInfo bufferAttachment = {};
            bufferAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            bufferAttachment.imageView = b->image.getView();
            bufferAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            bufferAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            bufferAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            bufferAttachment.clearValue = b->clearValue;

            geometryColorAttachments.push_back(bufferAttachment);
        }

        int width, height;
        glfwGetWindowSize(CONTEXT->window, &width, &height);

        VkViewport viewport{};
        viewport.width = static_cast<float>(width);
        viewport.height = static_cast<float>(height);
        viewport.minDepth = 0;
        viewport.maxDepth = 1;

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = CONTEXT->swapChainExtent;

        VkRenderingInfo depthRenderInfo{};
        depthRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        depthRenderInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ uint32_t(width), uint32_t(height) } };
        depthRenderInfo.layerCount = 1;
        depthRenderInfo.colorAttachmentCount = 0;
        depthRenderInfo.pColorAttachments = nullptr;
        depthRenderInfo.pDepthAttachment = &depthPassDepthAttachment;
        depthRenderInfo.pStencilAttachment = nullptr;

        VkRenderingInfo geometryRenderInfo{};
        geometryRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        geometryRenderInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ uint32_t(width), uint32_t(height) } };
        geometryRenderInfo.layerCount = 1;
        geometryRenderInfo.colorAttachmentCount = static_cast<uint32_t>(geometryColorAttachments.size());
        geometryRenderInfo.pColorAttachments = geometryColorAttachments.data();
        geometryRenderInfo.pDepthAttachment = &geometryPassDepthAttachment;
        geometryRenderInfo.pStencilAttachment = nullptr;

        VkRenderingInfo lightingRenderInfo{};
        lightingRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        lightingRenderInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ uint32_t(width), uint32_t(height) } };
        lightingRenderInfo.layerCount = 1;
        lightingRenderInfo.colorAttachmentCount = static_cast<uint32_t>(lightingColorAttachments.size());
        lightingRenderInfo.pColorAttachments = lightingColorAttachments.data();
        lightingRenderInfo.pDepthAttachment = nullptr;
        lightingRenderInfo.pStencilAttachment = nullptr;

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);
        bufferInfo.buffer = CONTEXT->uniformBuffers[imageIndex];

        DescriptorSetBuilder descriptorSetBuilder{ 1 };
        VkDescriptorSet depthDescriptorSet = descriptorSetBuilder
            .AddWrite(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, bufferInfo)
            .Build(CONTEXT->descriptorSetLayout)[0];

        VkDescriptorBufferInfo lightBufferInfo{};
        lightBufferInfo.offset = 0;
        lightBufferInfo.range = sizeof(Light) * CONTEXT->MAX_LIGHTS;
        lightBufferInfo.buffer = CONTEXT->lightBuffers[imageIndex];

        VkDescriptorBufferInfo shadowLightBufferInfo{};
        shadowLightBufferInfo.offset = 0;
        shadowLightBufferInfo.range = sizeof(DirectionalShadowLightBufferObject) * CONTEXT->MAX_SHADOW_LIGHTS;
        shadowLightBufferInfo.buffer = CONTEXT->shadowLightBuffers[imageIndex];

        VkDescriptorImageInfo depthImageInfo{};
        depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depthImageInfo.imageView = CONTEXT->depthImage.getView();

        VkDescriptorImageInfo cubemapImageInfo{};
        cubemapImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        cubemapImageInfo.imageView = scene->cubemap.cubemapImage.getView();

        VkDescriptorImageInfo diffuseIrradianceImageInfo{};
        diffuseIrradianceImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        diffuseIrradianceImageInfo.imageView = scene->cubemap.diffuseIrradianceImage.getView();

        VkDescriptorImageInfo shadowMapsImageInfo{};
        shadowMapsImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        shadowMapsImageInfo.imageView = DirectionalShadowLight::GetShadowMaps().getView();

        DescriptorSetBuilder lightingDescriptorSetBuilder{ 1 };
        lightingDescriptorSetBuilder
            .AddWrite(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, bufferInfo)
            .AddWrite(1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, lightBufferInfo)
            .AddWrite(2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, shadowLightBufferInfo)
            .AddWrite(3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &shadowMapsImageInfo)
            .AddWrite(4, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &cubemapImageInfo)
            .AddWrite(5, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &diffuseIrradianceImageInfo)
            .AddWrite(6, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &depthImageInfo);

        uint32_t index = 7;
        std::vector<VkDescriptorImageInfo*> imageInfos{};
        for (gBuffer* b : CONTEXT->gBuffers)
        {
            VkDescriptorImageInfo* imageInfo = new VkDescriptorImageInfo{};
            imageInfo->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo->imageView = b->image.getView();

            imageInfos.push_back(imageInfo);

            lightingDescriptorSetBuilder.AddWrite(index, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, imageInfo);
            index++;
        }

        VkDescriptorSet lightingDescriptorSet = lightingDescriptorSetBuilder.Build(CONTEXT->lightingPassDescriptorSetLayout)[0];

        for (auto i : imageInfos)
            delete i;

#pragma endregion

#pragma region Recording command buffer

        CONTEXT->depthImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        ImageHelpers::transitionImageLayout(commandBuffer, CONTEXT->swapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
        for (gBuffer* b : CONTEXT->gBuffers)
        {
            b->image.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // ------- DEPTH -------
        vkCmdBeginRendering(commandBuffer, &depthRenderInfo);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->depthPassPipeline.pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->depthPassPipeline.layout, 0, 1, &depthDescriptorSet, 0, nullptr);

        for (auto model : scene->models)
        {
            for (auto primitive : model->primitives)
            {
                if (primitive->isOpaque)
                {
                    VkDeviceSize offsets[] = { 0 };
                    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &primitive->vertexBuffer, offsets);
                    vkCmdBindIndexBuffer(commandBuffer, primitive->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(primitive->indices.size()), 1, 0, 0, 0);
                }
            }
        }

        vkCmdEndRendering(commandBuffer);

        // ------- GEOMETRY -------
        vkCmdBeginRendering(commandBuffer, &geometryRenderInfo);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->geometryPassPipeline.pipeline);

        for (auto model : scene->models)
        {
            for (auto primitive : model->primitives)
            {
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, &primitive->vertexBuffer, offsets);
                vkCmdBindIndexBuffer(commandBuffer, primitive->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->geometryPassPipeline.layout, 0, 1, &model->materials[primitive->materialIndex]->descriptorSets[imageIndex], 0, nullptr);
                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(primitive->indices.size()), 1, 0, 0, 0);
            }
        }

        vkCmdEndRendering(commandBuffer);

        for (gBuffer* b : CONTEXT->gBuffers)
        {
            b->image.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        CONTEXT->depthImage.TransitionLayout(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

        // ------- LIGHTING -------
        vkCmdBeginRendering(commandBuffer, &lightingRenderInfo);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->lightingPassPipeline.layout, 0, 1, &lightingDescriptorSet, 0, nullptr);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->lightingPassPipeline.pipeline);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdEndRendering(commandBuffer);

        ImageHelpers::transitionImageLayout(commandBuffer, CONTEXT->swapChainImages[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }

#pragma endregion
    }

    void CreateSubmitInfos()
    {
        DestroySubmitInfos();
        CONTEXT->drawFrameSubmitInfos.resize(CONTEXT->MAX_FRAMES_IN_FLIGHT);

        for (uint32_t index = 0; index < CONTEXT->MAX_FRAMES_IN_FLIGHT; index++)
        {
            VkSemaphoreSubmitInfo* waitSemaphoreInfo = new VkSemaphoreSubmitInfo{};
            waitSemaphoreInfo->sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            waitSemaphoreInfo->semaphore = CONTEXT->imageAvailableSemaphores[index];
            waitSemaphoreInfo->value = 0;
            waitSemaphoreInfo->stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            waitSemaphoreInfo->deviceIndex = 0;

            VkSemaphoreSubmitInfo* signalSemaphoreInfo = new VkSemaphoreSubmitInfo{};
            signalSemaphoreInfo->sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            signalSemaphoreInfo->semaphore = CONTEXT->renderFinishedSemaphores[index];
            signalSemaphoreInfo->value = 0;
            signalSemaphoreInfo->stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            signalSemaphoreInfo->deviceIndex = 0;

            VkCommandBufferSubmitInfo* commandBufferInfo = new VkCommandBufferSubmitInfo{};
            commandBufferInfo->sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            commandBufferInfo->commandBuffer = CONTEXT->commandBuffers[index];
            commandBufferInfo->deviceMask = 1;

            VkSubmitInfo2 submitInfo2{};
            submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
            submitInfo2.waitSemaphoreInfoCount = 1;
            submitInfo2.pWaitSemaphoreInfos = waitSemaphoreInfo;
            submitInfo2.commandBufferInfoCount = 1;
            submitInfo2.pCommandBufferInfos = commandBufferInfo;
            submitInfo2.signalSemaphoreInfoCount = 1;
            submitInfo2.pSignalSemaphoreInfos = signalSemaphoreInfo;

            CONTEXT->drawFrameSubmitInfos[index] = submitInfo2;
        }
    }

    void DestroySubmitInfos()
    {
        for (VkSubmitInfo2& submitInfo : CONTEXT->drawFrameSubmitInfos)
        {
            for (uint32_t i = 0; i < submitInfo.waitSemaphoreInfoCount; i++)
                delete &submitInfo.pWaitSemaphoreInfos[i];

            for (uint32_t i = 0; i < submitInfo.signalSemaphoreInfoCount; i++)
                delete& submitInfo.pSignalSemaphoreInfos[i];

            for (uint32_t i = 0; i < submitInfo.commandBufferInfoCount; i++)
                delete& submitInfo.pCommandBufferInfos[i];
        }
    }

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::mat4(1.0f);
        ubo.view = glm::lookAt(camera.origin, camera.origin + camera.forward, camera.up);
        ubo.proj = glm::perspective(glm::radians(camera.fov), CONTEXT->swapChainExtent.width / (float)CONTEXT->swapChainExtent.height, camera.nearPlane, camera.farPlane);
        ubo.proj[1][1] *= -1;

        ubo.cameraPos = glm::vec4(camera.origin, 0);
        ubo.resolution.x = CONTEXT->swapChainExtent.width;
        ubo.resolution.y = CONTEXT->swapChainExtent.height;

        ubo.invProj = glm::inverse(ubo.proj);
        ubo.invView = glm::inverse(ubo.view);

        memcpy(
            CONTEXT->uniformBuffersMapped[currentImage], 
            &ubo, 
            sizeof(ubo)
        );
        memcpy(
            CONTEXT->lightBuffersMapped[currentImage], 
            scene->lights.data(), 
            sizeof(Light) * std::min(CONTEXT->MAX_LIGHTS, static_cast<uint32_t>(scene->lights.size()))
        );

        std::vector<DirectionalShadowLightBufferObject> shadowLightsToCopy{ scene->directionalShadowLights.size() };
        for (uint32_t i = 0; i < scene->directionalShadowLights.size(); i++)
            shadowLightsToCopy[i] = *scene->directionalShadowLights[i];

        memcpy(
            CONTEXT->shadowLightBuffersMapped[currentImage], 
            shadowLightsToCopy.data(),
            sizeof(DirectionalShadowLightBufferObject) * std::min(CONTEXT->MAX_SHADOW_LIGHTS, static_cast<uint32_t>(shadowLightsToCopy.size()))
        );
    }

};