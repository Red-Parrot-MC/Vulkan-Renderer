#pragma once
#include <vector>
#include <glm/ext/vector_float3.hpp>
#include <array>
#include "structs.h"
#include "VmaUsage.h"
#include "DeletionQueue.h"


class VulkanContext
{
public:
    DeletionQueue applicationDeletionQueue{}; // gets flushed when the application closes
    DeletionQueue resizeDeletionQueue{}; // gets flushed when resized

    GLFWwindow* window{};

    VkInstance instance{};
    VkSurfaceKHR surface{};

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device{};

    VmaAllocator vmaAllocator{};
    VmaPool vmaPool{};

    VkQueue graphicsQueue{};
    VkQueue presentQueue{};

    VkSwapchainKHR swapChain{};
    std::vector<VkImage> swapChainImages{};
    std::vector<VkImageView> swapChainImageViews{};
    VkFormat swapChainImageFormat{};
    VkExtent2D swapChainExtent{};
    std::vector<VkFramebuffer> swapChainFramebuffers{};

    VkDescriptorSetLayout descriptorSetLayout{};
    VkPipeline graphicsPipeline{};
    VkPipelineLayout pipelineLayout{};
    VkPipeline depthPrepassPipeline{};
    VkPipelineLayout depthPipelineLayout{};
    VkPipeline deferredPipeline{};
    VkPipelineLayout deferredPipelineLayout{};
    VkDescriptorSetLayout deferredDescriptorSetLayout{};

    VkCommandPool commandPool{};
    std::vector<VkCommandBuffer> commandBuffers{};

    std::vector<VkSemaphore> imageAvailableSemaphores{};
    std::vector<VkSemaphore> renderFinishedSemaphores{};
    std::vector<VkFence> inFlightFences{};

    std::vector<VkSubmitInfo2> drawFrameSubmitInfos{};

    uint32_t currentFrame = 0;
    bool framebufferResized = false;

    std::vector<VkBuffer> uniformBuffers{};
    std::vector<void*> uniformBuffersMapped{};

    VkDescriptorPool descriptorPool{};

    //VkImageView depthImageView{};
    //VkImage depthImage{};
    Image depthImage{};

    int MAX_FRAMES_IN_FLIGHT{ 3 };

    std::vector<gBuffer*> gBuffers{};
};

class CurrentVulkanContext
{
public:
    static VulkanContext* context;
    static PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
};

#define CONTEXT CurrentVulkanContext::context
#define setDebugUtilsObjectName if (CurrentVulkanContext::vkSetDebugUtilsObjectNameEXT) CurrentVulkanContext::vkSetDebugUtilsObjectNameEXT
#define vkSetDebugUtilsObjectName CurrentVulkanContext::vkSetDebugUtilsObjectNameEXT