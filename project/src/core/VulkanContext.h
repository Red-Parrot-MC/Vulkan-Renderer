#pragma once
#include "../libs/VmaUsage.h"
#include <vector>
#include <glm/ext/vector_float3.hpp>
#include <array>
#include "structs.h"
#include "DeletionQueue.h"

const uint32_t MaxFramesInFlight = 3;

class VulkanContext
{
public:
    uint32_t MAX_FRAMES_IN_FLIGHT{ MaxFramesInFlight };

    DeletionQueue applicationDeletionQueue{}; // gets flushed when the application closes
    DeletionQueue resizeDeletionQueue{}; // gets flushed when resized
    std::array<DeletionQueue, MaxFramesInFlight> commandBuffersDeletionQueues{};

    //GLFWwindow* window{};
    int windowWidth{};
    int windowHeight{};

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
    VkDescriptorSetLayout lightingPassDescriptorSetLayout{};
    Pipeline lightingPassPipeline{};
    Pipeline outlinePassPipeline{};

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
    std::vector<Image> depthImages{};

    std::vector<std::vector<gBuffer*>> gBuffers{};

    const uint32_t MAX_LIGHTS{ 100 };
    const uint32_t MAX_SHADOW_LIGHTS{ 3 };
    const glm::ivec2 SHADOW_MAP_RESOLUTION{ 4096, 4096 };
};

class CurrentVulkanContext
{
public:
    static VulkanContext* context;
    static PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
    static PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonModeEXT;
};

#define CONTEXT CurrentVulkanContext::context
#define setDebugUtilsObjectName if (CurrentVulkanContext::vkSetDebugUtilsObjectNameEXT) CurrentVulkanContext::vkSetDebugUtilsObjectNameEXT
#define vkSetDebugUtilsObjectName CurrentVulkanContext::vkSetDebugUtilsObjectNameEXT
#define cmdSetPolygonMode if (CurrentVulkanContext::vkCmdSetPolygonModeEXT) CurrentVulkanContext::vkCmdSetPolygonModeEXT
#define vkCmdSetPolygonMode CurrentVulkanContext::vkCmdSetPolygonModeEXT