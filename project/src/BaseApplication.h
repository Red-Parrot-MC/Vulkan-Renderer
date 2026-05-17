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
const int MAX_FRAMES_IN_FLIGHT = 3;

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
		cleanupVulkan();
	}

protected:
    Scene* scene{};
    Camera camera{};

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
        //std::cout << CONTEXT->currentFrame << ", " << imageIndex << "\n";

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

        VkSemaphore signalSemaphores[] = { CONTEXT->renderFinishedSemaphores[CONTEXT->currentFrame] };

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

        CONTEXT->currentFrame = (CONTEXT->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
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
        VkCommandBuffer& commandBuffer = CONTEXT->commandBuffers[imageIndex];
        vkResetCommandBuffer(commandBuffer, /*VkCommandBufferResetFlagBits*/ 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkImageSubresourceRange subresourceRange{};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;

        ImageHelpers::transitionImageLayout(commandBuffer, CONTEXT->depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, subresourceRange);
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageHelpers::transitionImageLayout(commandBuffer, CONTEXT->swapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, subresourceRange);

        // Set attachments for dynamic rendering
        VkRenderingAttachmentInfo colorAttachment = {};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = CONTEXT->swapChainImageViews[imageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = { {0.0f, 0.0f, 0.0f, 1.0f} };

        VkRenderingAttachmentInfo depthAttachment = {};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = CONTEXT->depthImageView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue = { 1.0f, 0 };

        std::vector<VkRenderingAttachmentInfo> colorAttachments{ colorAttachment };

        for (gBuffer* b : CONTEXT->gBuffers)
        {
            VkRenderingAttachmentInfo bufferAttachment = {};
            bufferAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            bufferAttachment.imageView = b->image.view;
            bufferAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            bufferAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            bufferAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            bufferAttachment.clearValue = b->clearValue;

            colorAttachments.push_back(bufferAttachment);
        }

        int width, height;
        glfwGetWindowSize(CONTEXT->window, &width, &height);

        VkViewport viewport{};
        viewport.width = width;
        viewport.height = height;
        viewport.minDepth = 0;
        viewport.maxDepth = 1;

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = CONTEXT->swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        VkRenderingInfo depthRenderInfo{};
        depthRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        depthRenderInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ uint32_t(width), uint32_t(height) } };
        depthRenderInfo.layerCount = 1;
        depthRenderInfo.colorAttachmentCount = 0;
        depthRenderInfo.pColorAttachments = nullptr;
        depthRenderInfo.pDepthAttachment = &depthAttachment;
        depthRenderInfo.pStencilAttachment = nullptr;

        vkCmdBeginRendering(commandBuffer, &depthRenderInfo);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->depthPrepassPipeline);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        DescriptorSetBuilder descriptorSetBuilder{};
        auto descriptorSets = descriptorSetBuilder
            .AddWrite(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, bufferInfo, CONTEXT->uniformBuffers)
            .Build();

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->depthPipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, nullptr);

        for (auto model : scene->models)
        {
            for (auto primitive : model->primitives)
            {
                if (primitive->isOpaque)
                {
                    VkBuffer vertexBuffers[] = { primitive->vertexBuffer };
                    VkDeviceSize offsets[] = { 0 };
                    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

                    vkCmdBindIndexBuffer(commandBuffer, primitive->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(primitive->indices.size()), 1, 0, 0, 0);
                }
            }
        }

        vkCmdEndRendering(commandBuffer);

        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_NONE;

        VkRenderingInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.renderArea = VkRect2D{ VkOffset2D{ 0, 0 }, VkExtent2D{ uint32_t(width), uint32_t(height) } };
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = colorAttachments.size();
        renderInfo.pColorAttachments = colorAttachments.data();
        renderInfo.pDepthAttachment = &depthAttachment;
        renderInfo.pStencilAttachment = nullptr;

        vkCmdBeginRendering(commandBuffer, &renderInfo);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->graphicsPipeline);

        for (auto model : scene->models)
        {
            for (auto primitive : model->primitives)
            {
                VkBuffer vertexBuffers[] = { primitive->vertexBuffer };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

                vkCmdBindIndexBuffer(commandBuffer, primitive->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->pipelineLayout, 0, 1, &model->materials[primitive->materialIndex]->descriptorSets[imageIndex], 0, nullptr);

                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(primitive->indices.size()), 1, 0, 0, 0);
            }
        }

        vkCmdEndRendering(commandBuffer);

        ImageHelpers::transitionImageLayout(commandBuffer, CONTEXT->swapChainImages[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, subresourceRange);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
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
            for (int i = 0; i < submitInfo.waitSemaphoreInfoCount; i++)
                delete &submitInfo.pWaitSemaphoreInfos[i];

            for (int i = 0; i < submitInfo.signalSemaphoreInfoCount; i++)
                delete& submitInfo.pSignalSemaphoreInfos[i];

            for (int i = 0; i < submitInfo.commandBufferInfoCount; i++)
                delete& submitInfo.pCommandBufferInfos[i];
        }
    }

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        //ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.model = glm::mat4(1.0f);
        ubo.view = glm::lookAt(camera.origin, camera.origin + camera.forward, camera.up);
        ubo.proj = glm::perspective(glm::radians(camera.fov), CONTEXT->swapChainExtent.width / (float)CONTEXT->swapChainExtent.height, camera.nearPlane, camera.farPlane);
        ubo.proj[1][1] *= -1;

        memcpy(CONTEXT->uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

};