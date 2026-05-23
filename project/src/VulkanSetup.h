#pragma once
#include "structs.h"
#include <string>
#include "Texture.h"

class VulkanSetup
{
public:
	static void InitializeVulkan();
    static void recreateSwapChain();
    static void cleanup();

private:
    static void createInstance();
    static void setupDebugMessenger();
    static void createSurface();
    static void pickPhysicalDevice();
    static void createLogicalDevice();
    static void initializeVMA();
    static void createSwapChain();
    static void createGBuffers();
    static void createImageViews();
    static void createDescriptorSetLayout();
    static void createGraphicsPipeline();
    static void createDepthPrepassPipeline();
    static void createDeferredPipeline();
    static void createCommandPool();
    static void createDepthResources();
    static void createUniformBuffers();
    static void createCommandBuffers();
    static void createSyncObjects();
    static void createDescriptorPool();
    static void createDebugNameInfo();


    static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static std::vector<const char*> getRequiredExtensions();
    static bool checkValidationLayerSupport();
    static bool isDeviceSuitable(VkPhysicalDevice device);
    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    static bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    static VkFormat findDepthFormat();
    static VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    static std::vector<char> readFile(const std::string& filename);
    static VkShaderModule createShaderModule(const std::vector<char>& code);
};