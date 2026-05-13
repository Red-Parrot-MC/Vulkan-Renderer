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

        scene->RecordCommandBuffer(commandBuffer, imageIndex);

        vkCmdEndRenderPass(commandBuffer);
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