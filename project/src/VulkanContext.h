#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <glm/ext/vector_float3.hpp>
#include <array>
#include "structs.h"

class VulkanContext
{
public:
    GLFWwindow* window{};

    VkInstance instance{};
    VkDebugUtilsMessengerEXT debugMessenger{};
    VkSurfaceKHR surface{};

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device{};

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

    uint32_t currentFrame = 0;
    bool framebufferResized = false;


    std::vector<VkBuffer> uniformBuffers{};
    std::vector<VkDeviceMemory> uniformBuffersMemory{};
    std::vector<void*> uniformBuffersMapped{};

    VkDescriptorPool descriptorPool{};

    VkImage depthImage{};
    VkDeviceMemory depthImageMemory{};
    VkImageView depthImageView{};


    //VkBuffer vertexBuffer{};
    //VkDeviceMemory vertexBufferMemory{};
    //VkBuffer indexBuffer{};
    //VkDeviceMemory indexBufferMemory{};

    //std::vector<VkDescriptorSet> descriptorSets{};
    //std::vector<Vertex> vertices{};
    //std::vector<uint16_t> indices{};

    const int MAX_FRAMES_IN_FLIGHT{ 3 };
};

class CurrentVulkanContext
{
public:
    static VulkanContext* context;
};

#define CONTEXT CurrentVulkanContext::context