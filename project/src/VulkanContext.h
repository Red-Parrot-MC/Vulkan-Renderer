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
    Pipeline geometryPassPipeline{};
    Pipeline depthPassPipeline{};
    Pipeline lightingPassPipeline{};
    VkDescriptorSetLayout lightingPassDescriptorSetLayout{};

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
    std::vector<VkBuffer> lightBuffers{};
    std::vector<void*> lightBuffersMapped{};
    std::vector<VkBuffer> shadowLightBuffers{};
    std::vector<void*> shadowLightBuffersMapped{};

    VkDescriptorPool descriptorPool{};

    //VkImageView depthImageView{};
    //VkImage depthImage{};
    Image depthImage{};

    int MAX_FRAMES_IN_FLIGHT{ 3 };

    std::vector<gBuffer*> gBuffers{};

    const uint32_t MAX_LIGHTS{ 100 };
    const uint32_t MAX_SHADOW_LIGHTS{ 10 };
    const glm::ivec2 SHADOW_MAP_RESOLUTION{ 4096 / 1, 4096 / 1 };
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