#pragma once
#include "VulkanContext.h"
#include <iostream>
#include <vulkan/vk_enum_string_helper.h>

class CommandBufferHelpers
{
public:
    static VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = CONTEXT->commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(CONTEXT->device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }


    static void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        VkResult result = vkEndCommandBuffer(commandBuffer);
        if (result != VK_SUCCESS)
        {
            std::cout << string_VkResult(result) << "\n";
            throw std::runtime_error("Failed to end command buffer");
        }

        VkCommandBufferSubmitInfo commandBufferInfo{};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferInfo.commandBuffer = commandBuffer;

        VkSubmitInfo2 submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.pCommandBufferInfos = &commandBufferInfo;
        submitInfo.commandBufferInfoCount = 1;

        result = vkQueueSubmit2(CONTEXT->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        if (result != VK_SUCCESS)
        {
            std::cout << string_VkResult(result) << "\n";
            throw std::runtime_error("Failed to submit command buffer");
        }
        vkQueueWaitIdle(CONTEXT->graphicsQueue);

        vkFreeCommandBuffers(CONTEXT->device, CONTEXT->commandPool, 1, &commandBuffer);
    }
};