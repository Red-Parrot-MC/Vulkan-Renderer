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
		mainLoop();
        Cleanup();
		cleanupVulkan();
	}

protected:
    Scene* scene;
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
        VulkanSetup::cleanup();
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
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        updateUniformBuffer(CONTEXT->currentFrame);

        // Only reset the fence if we are submitting work
        vkResetFences(CONTEXT->device, 1, &CONTEXT->inFlightFences[CONTEXT->currentFrame]);

        vkResetCommandBuffer(CONTEXT->commandBuffers[CONTEXT->currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
        //recordCommandBuffer(CONTEXT->commandBuffers[CONTEXT->currentFrame], imageIndex);
        scene->RecordCommandBuffer(CONTEXT->commandBuffers[CONTEXT->currentFrame], imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { CONTEXT->imageAvailableSemaphores[CONTEXT->currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &CONTEXT->commandBuffers[CONTEXT->currentFrame];

        VkSemaphore signalSemaphores[] = { CONTEXT->renderFinishedSemaphores[CONTEXT->currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(CONTEXT->graphicsQueue, 1, &submitInfo, CONTEXT->inFlightFences[CONTEXT->currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

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
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        CONTEXT->currentFrame = (CONTEXT->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        //VkCommandBufferBeginInfo beginInfo{};
        //beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        //if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        //    throw std::runtime_error("failed to begin recording command buffer!");
        //}

        //VkRenderPassBeginInfo renderPassInfo{};
        //renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        //renderPassInfo.renderPass = CONTEXT->renderPass;
        //renderPassInfo.framebuffer = CONTEXT->swapChainFramebuffers[imageIndex];
        //renderPassInfo.renderArea.offset = { 0, 0 };
        //renderPassInfo.renderArea.extent = CONTEXT->swapChainExtent;

        //std::array<VkClearValue, 2> clearValues{};
        //clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        //clearValues[1].depthStencil = { 1.0f, 0 };

        //renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        //renderPassInfo.pClearValues = clearValues.data();

        //vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        //vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->graphicsPipeline);

        //VkViewport viewport{};
        //viewport.x = 0.0f;
        //viewport.y = 0.0f;
        //viewport.width = (float)CONTEXT->swapChainExtent.width;
        //viewport.height = (float)CONTEXT->swapChainExtent.height;
        //viewport.minDepth = 0.0f;
        //viewport.maxDepth = 1.0f;
        //vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        //VkRect2D scissor{};
        //scissor.offset = { 0, 0 };
        //scissor.extent = CONTEXT->swapChainExtent;
        //vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        //VkBuffer vertexBuffers[] = { CONTEXT->vertexBuffer };
        //VkDeviceSize offsets[] = { 0 };
        //vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        //vkCmdBindIndexBuffer(commandBuffer, CONTEXT->indexBuffer, 0, VK_INDEX_TYPE_UINT16);

        //vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CONTEXT->pipelineLayout, 0, 1, &CONTEXT->descriptorSets[CONTEXT->currentFrame], 0, nullptr);

        //vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(CONTEXT->indices.size()), 1, 0, 0, 0);

        //vkCmdEndRenderPass(commandBuffer);

        //if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        //    throw std::runtime_error("failed to record command buffer!");
        //}

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