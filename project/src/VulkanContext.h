#pragma once
#include <vector>
#include <glm/ext/vector_float3.hpp>
#include <array>
#include "structs.h"
#include "VmaUsage.h"


class VulkanContext
{
public:
    GLFWwindow* window{};

    VkInstance instance{};
    VkDebugUtilsMessengerEXT debugMessenger{};
    VkSurfaceKHR surface{};

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device{};

    VmaAllocator vmaAllocator{};
    VmaPool vmaPool{};

    VkQueue graphicsQueue{};
    VkQueue presentQueue{};

    VkSwapchainKHR swapChain{};
    std::vector<VkImage> swapChainImages{};
    VkFormat swapChainImageFormat{};
    VkExtent2D swapChainExtent{};
    std::vector<VkImageView> swapChainImageViews{};
    std::vector<VkFramebuffer> swapChainFramebuffers{};

    VkRenderPass renderPass{};
    VkDescriptorSetLayout descriptorSetLayout{};
    VkPipelineLayout pipelineLayout{};
    VkPipeline graphicsPipeline{};

    VkCommandPool commandPool{};
    std::vector<VkCommandBuffer> commandBuffers{};

    std::vector<VkSemaphore> imageAvailableSemaphores{};
    std::vector<VkSemaphore> renderFinishedSemaphores{};
    std::vector<VkFence> inFlightFences{};

    std::vector<VkSubmitInfo2> drawFrameSubmitInfos{};

    uint32_t currentFrame = 0;
    bool framebufferResized = false;


    std::vector<VkBuffer> uniformBuffers{};
    std::vector<VkDeviceMemory> uniformBuffersMemory{};
    std::vector<void*> uniformBuffersMapped{};

    VkDescriptorPool descriptorPool{};

    VkImage depthImage{};
    VkDeviceMemory depthImageMemory{};
    VkImageView depthImageView{};

    const int MAX_FRAMES_IN_FLIGHT{ 3 };

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