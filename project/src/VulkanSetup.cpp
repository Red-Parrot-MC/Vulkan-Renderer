#include "VulkanSetup.h"
#include "VulkanContext.h"
#include "BufferHelpers.h"
#include <iostream>
#include <set>
#include <algorithm>
#include <fstream>
#include "ImageHelpers.h"
#include "DescriptorSetBuilder.h"
#include <vulkan/vk_enum_string_helper.h>
#include "PipelineBuilder.h"
#include "Model.h"
#include "DescriptorSetLayoutBuilder.h"
#include "CreateInfoHelpers.h"

const int MAX_MODELS = 200;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

void VulkanSetup::InitializeVulkan()
{
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    vkSetDebugUtilsObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(CONTEXT->device, "vkSetDebugUtilsObjectNameEXT");
    initializeVMA();
    createSwapChain();
    createCommandPool();
    createGBuffers();
    createImageViews();
    createPipelines();
    createDepthResources();
    createUniformBuffers();
    createCommandBuffers();
    createSyncObjects();
    createDescriptorPool();

    if (vkSetDebugUtilsObjectName)
        createDebugNameInfo();
}

void VulkanSetup::recreateSwapChain() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(CONTEXT->window, &width, &height);
    while (width == 0 || height == 0) {
        if (glfwWindowShouldClose(CONTEXT->window))
            break;

        glfwGetFramebufferSize(CONTEXT->window, &width, &height);
        glfwWaitEvents();
    }

    if (glfwWindowShouldClose(CONTEXT->window)) return;

    vkDeviceWaitIdle(CONTEXT->device);
    CONTEXT->resizeDeletionQueue.Flush();

    createSwapChain();
    createGBuffers();
    createImageViews();
    createDepthResources();
    createSyncObjects();

    CONTEXT->currentFrame = 0;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

void VulkanSetup::cleanup()
{
    CONTEXT->resizeDeletionQueue.Flush();
    CONTEXT->applicationDeletionQueue.Flush();
}



void VulkanSetup::createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }
    else {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &CONTEXT->instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }

    CONTEXT->applicationDeletionQueue.PushDeletor(
        []()
        {
            vkDestroyInstance(CONTEXT->instance, nullptr);
        }
    );
    
}

void VulkanSetup::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerEXT debugMessenger{};

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(CONTEXT->instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }

    CONTEXT->applicationDeletionQueue.PushDeletor(
        [debugMessenger]()
        {
            DestroyDebugUtilsMessengerEXT(CONTEXT->instance, debugMessenger, nullptr);
        }
    );
}

void VulkanSetup::createSurface() {
    if (glfwCreateWindowSurface(CONTEXT->instance, CONTEXT->window, nullptr, &CONTEXT->surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }

    CONTEXT->applicationDeletionQueue.PushDeletor(
        []()
        {
            vkDestroySurfaceKHR(CONTEXT->instance, CONTEXT->surface, nullptr);
        }
    );
}

void VulkanSetup::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(CONTEXT->instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(CONTEXT->instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            CONTEXT->physicalDevice = device;
            break;
        }
    }

    if (CONTEXT->physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

void VulkanSetup::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(CONTEXT->physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }


    // Feature select

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.synchronization2 = VK_TRUE;
    features13.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{}; 
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;

    VkPhysicalDeviceVulkan11Features features11{};
    features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    features11.pNext = &features12;
    
    VkPhysicalDeviceFeatures features10{};
    features10.samplerAnisotropy = VK_TRUE;

    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &features11;
    features.features = features10;

    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT vertexInputDynamicStateFeatures = {};
    vertexInputDynamicStateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT;
    vertexInputDynamicStateFeatures.pNext = &features;
    vertexInputDynamicStateFeatures.vertexInputDynamicState = VK_TRUE;
    

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pNext = &vertexInputDynamicStateFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(CONTEXT->physicalDevice, &createInfo, nullptr, &CONTEXT->device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(CONTEXT->device, indices.graphicsFamily.value(), 0, &CONTEXT->graphicsQueue);
    vkGetDeviceQueue(CONTEXT->device, indices.presentFamily.value(), 0, &CONTEXT->presentQueue);

    CONTEXT->applicationDeletionQueue.PushDeletor(
        []()
        {
            vkDestroyDevice(CONTEXT->device, nullptr);
        }
    );
}

void VulkanSetup::initializeVMA()
{
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.device = CONTEXT->device;
    allocatorInfo.instance = CONTEXT->instance;
    allocatorInfo.physicalDevice = CONTEXT->physicalDevice;
    if (vmaCreateAllocator(&allocatorInfo, &CONTEXT->vmaAllocator) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create vma allocator!");
    }

    VmaPoolCreateInfo poolInfo{};

    if (vmaCreatePool(CONTEXT->vmaAllocator, &poolInfo, &CONTEXT->vmaPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create vma pool!");
    }

    CONTEXT->applicationDeletionQueue.PushDeletor(
        []()
        {
            //char* statsString = nullptr;
            //vmaBuildStatsString(CONTEXT->vmaAllocator, &statsString, VK_TRUE);
            //printf("%s\n", statsString);
            //vmaFreeStatsString(CONTEXT->vmaAllocator, statsString);

            vmaDestroyPool(CONTEXT->vmaAllocator, CONTEXT->vmaPool);
            vmaDestroyAllocator(CONTEXT->vmaAllocator);
        }
    );
}

void VulkanSetup::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(CONTEXT->physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = CONTEXT->MAX_FRAMES_IN_FLIGHT;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    if (imageCount < swapChainSupport.capabilities.minImageCount)
    {
        imageCount = swapChainSupport.capabilities.minImageCount;
    }
    CONTEXT->MAX_FRAMES_IN_FLIGHT = imageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = CONTEXT->surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(CONTEXT->physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(CONTEXT->device, &createInfo, nullptr, &CONTEXT->swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(CONTEXT->device, CONTEXT->swapChain, &imageCount, nullptr);
    CONTEXT->swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(CONTEXT->device, CONTEXT->swapChain, &imageCount, CONTEXT->swapChainImages.data());

    CONTEXT->swapChainImageFormat = surfaceFormat.format;
    CONTEXT->swapChainExtent = extent;

    CONTEXT->resizeDeletionQueue.PushDeletor(
        []()
        {
            vkDestroySwapchainKHR(CONTEXT->device, CONTEXT->swapChain, nullptr);
        }
    );
}

void VulkanSetup::createGBuffers()
{
    VkClearValue clearValue{};
    clearValue.color = { 0, 0, 0, 1 };

    // albedo
    CONTEXT->gBuffers.push_back(ImageHelpers::createGBuffer(VK_FORMAT_R8G8B8A8_UNORM, clearValue, "gBuffer Albedo"));

    // normals
    CONTEXT->gBuffers.push_back(ImageHelpers::createGBuffer(VK_FORMAT_R32G32_SFLOAT, clearValue, "gBuffer Normal"));

    // metalic roughness
    CONTEXT->gBuffers.push_back(ImageHelpers::createGBuffer(VK_FORMAT_R8G8_UNORM, clearValue, "gBuffer Metalic"));

    CONTEXT->resizeDeletionQueue.PushDeletor(
        []()
        {
            for (gBuffer* b : CONTEXT->gBuffers)
            {
                b->image.Destroy();
            }
            CONTEXT->gBuffers.clear();
        }
    );
}

void VulkanSetup::createImageViews() {
    CONTEXT->swapChainImageViews.resize(CONTEXT->swapChainImages.size());

    for (uint32_t i = 0; i < CONTEXT->swapChainImages.size(); i++) {
        CONTEXT->swapChainImageViews[i] = ImageHelpers::createImageView(CONTEXT->swapChainImages[i], CONTEXT->swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }

    CONTEXT->resizeDeletionQueue.PushDeletor(
        []()
        {
            for (auto imageView : CONTEXT->swapChainImageViews) {
                vkDestroyImageView(CONTEXT->device, imageView, nullptr);
            }
        }
    );
}

void VulkanSetup::createPipelines()
{
    // ---------- SAMPLER ----------
    VkSampler sampler{};
    {
        auto samplerInfo = CreateInfoHelpers::GetSamplerCreateInfo(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
        vkCreateSampler(CONTEXT->device, &samplerInfo, nullptr, &sampler);

        CONTEXT->applicationDeletionQueue.PushDeletor(
            [sampler]()
            {
                vkDestroySampler(CONTEXT->device, sampler, nullptr);
            }
        );
    }

    // ---------- GEOMETRY PASS ----------
    {
        DescriptorSetLayoutBuilder setLayoutBuilder{};
        CONTEXT->descriptorSetLayout = setLayoutBuilder
            .AddBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
            .AddBinding(1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler)
            .AddBinding(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler)
            .AddBinding(3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler)
            .Build();
        setLayoutBuilder.PushDeletionQueue(CONTEXT->applicationDeletionQueue);


        PipelineBuilder gBufferPipelineBuilder{};
        gBufferPipelineBuilder
            .AddShader("shaders/shader_vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
            .AddShader("shaders/shader_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBindingDescription(Vertex::getBindingDescription())
            .SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetViewportState(1, 1)
            .SetRasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, 1.0f, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE)
            .SetMultisampling(VK_FALSE, VK_SAMPLE_COUNT_1_BIT)
            .SetColorBlending(VK_FALSE, VK_LOGIC_OP_COPY)
            .SetDynamicStates({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
            .AddSetLayout(CONTEXT->descriptorSetLayout)
            .SetDepthStencil(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL, VK_FALSE, VK_FALSE)
            .SetDepthFormat(PipelineBuilder::findDepthFormat())
            .PushDeletionQueue(CONTEXT->applicationDeletionQueue)
            .SetDebugName("geometry");

        for (auto& description : Vertex::getAttributeDescriptions())
            gBufferPipelineBuilder.AddAttributeDescription(description);

        for (gBuffer* b : CONTEXT->gBuffers)
            gBufferPipelineBuilder
            .AddColorAttachmentFormat(b->format)
            .AddColorBlendAttachment(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);

        CONTEXT->geometryPassPipeline = gBufferPipelineBuilder.Build();
    }


    // ---------- DEPTH PASS ----------
    {
        PipelineBuilder depthPipelineBuilder{};
        depthPipelineBuilder
            .AddShader("shaders/depth_vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
            .AddShader("shaders/depth_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
            .AddBindingDescription(Vertex::getBindingDescription())
            .SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetViewportState(1, 1)
            .SetRasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, 1.0f, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE)
            .SetMultisampling(VK_FALSE, VK_SAMPLE_COUNT_1_BIT)
            .SetColorBlending(VK_FALSE, VK_LOGIC_OP_COPY)
            .SetDynamicStates({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
            .AddSetLayout(CONTEXT->descriptorSetLayout)
            .SetDepthStencil(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, VK_FALSE)
            .SetDepthFormat(PipelineBuilder::findDepthFormat())
            .PushDeletionQueue(CONTEXT->applicationDeletionQueue)
            .SetDebugName("depth");

        for (auto& description : Vertex::getAttributeDescriptions())
            depthPipelineBuilder.AddAttributeDescription(description);

        CONTEXT->depthPassPipeline = depthPipelineBuilder.Build();
    }


    // ---------- LIGHTING PASS ----------
    {
        DescriptorSetLayoutBuilder deferredSetLayoutBuilder{};
        deferredSetLayoutBuilder
            .PushDeletionQueue(CONTEXT->applicationDeletionQueue)
            .AddBinding(0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // ubo
            .AddBinding(1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // lights
            .AddBinding(2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler) // Cubemap
            .AddBinding(3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler) // Diffuse irradiance
            .AddBinding(4, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler); // depth
        uint32_t index = 5;

        for (gBuffer* g : CONTEXT->gBuffers)
            deferredSetLayoutBuilder.AddBinding(index++, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, &sampler); // gBuffer
        
        CONTEXT->deferredDescriptorSetLayout = deferredSetLayoutBuilder.Build();


        PipelineBuilder deferredPipelineBuilder{};
        CONTEXT->lightingPassPipeline = deferredPipelineBuilder
            .AddShader("shaders/deferred_vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
            .AddShader("shaders/deferred_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetInputAssembly(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetViewportState(1, 1)
            .SetRasterizer(VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, 1.0f, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE)
            .SetMultisampling(VK_FALSE, VK_SAMPLE_COUNT_1_BIT)
            .AddColorBlendAttachment(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE)
            .SetColorBlending(VK_FALSE, VK_LOGIC_OP_COPY)
            .SetDynamicStates({ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR })
            .AddSetLayout(CONTEXT->deferredDescriptorSetLayout)
            .SetDepthStencil(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS, VK_FALSE, VK_FALSE)
            .SetDepthFormat(PipelineBuilder::findDepthFormat())
            .AddColorAttachmentFormat(CONTEXT->swapChainImageFormat)
            .PushDeletionQueue(CONTEXT->applicationDeletionQueue)
            .SetDebugName("lighting")
            .Build();
    }
}

void VulkanSetup::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(CONTEXT->physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(CONTEXT->device, &poolInfo, nullptr, &CONTEXT->commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    CONTEXT->applicationDeletionQueue.PushDeletor(
        []()
        {
            vkDestroyCommandPool(CONTEXT->device, CONTEXT->commandPool, nullptr);
        }
    );
}

void VulkanSetup::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();
    //ImageHelpers::createImage(CONTEXT->swapChainExtent.width, CONTEXT->swapChainExtent.height, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, CONTEXT->depthImage.image, CONTEXT->depthImage.memory);
    //CONTEXT->depthImage.view = ImageHelpers::createImageView(CONTEXT->depthImage.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

    //ImageHelpers::transitionImageLayout(CONTEXT->depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = CONTEXT->swapChainExtent.width;
    imageInfo.extent.height = CONTEXT->swapChainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    CONTEXT->depthImage.Create(imageInfo, VK_IMAGE_ASPECT_DEPTH_BIT);
    CONTEXT->depthImage.SetDebugNameInfo("DepthImage");
    CONTEXT->depthImage.TransitionLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    CONTEXT->resizeDeletionQueue.PushDeletor(
        []()
        {
            CONTEXT->depthImage.Destroy();
        }
    );
}

void VulkanSetup::createUniformBuffers() {
    std::vector<VkDeviceMemory> uniformBuffersMemory{};

    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    CONTEXT->uniformBuffers.resize(CONTEXT->MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(CONTEXT->MAX_FRAMES_IN_FLIGHT);
    CONTEXT->uniformBuffersMapped.resize(CONTEXT->MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < CONTEXT->MAX_FRAMES_IN_FLIGHT; i++) {
        BufferHelpers::createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, CONTEXT->uniformBuffers[i], uniformBuffersMemory[i]);

        vkMapMemory(CONTEXT->device, uniformBuffersMemory[i], 0, bufferSize, 0, &CONTEXT->uniformBuffersMapped[i]);
    }

    CONTEXT->applicationDeletionQueue.PushDeletor(
        [uniformBuffersMemory]()
        {
            for (size_t i = 0; i < CONTEXT->MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroyBuffer(CONTEXT->device, CONTEXT->uniformBuffers[i], nullptr);
                vkFreeMemory(CONTEXT->device, uniformBuffersMemory[i], nullptr);
            }
        }
    );


    // Lights
    std::vector<VkDeviceMemory> lightsBuffersMemory{};
    bufferSize = sizeof(Light) * CONTEXT->MAX_LIGHTS;
    CONTEXT->lightBuffers.resize(CONTEXT->MAX_FRAMES_IN_FLIGHT);
    lightsBuffersMemory.resize(CONTEXT->MAX_FRAMES_IN_FLIGHT);
    CONTEXT->lightBuffersMapped.resize(CONTEXT->MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < CONTEXT->MAX_FRAMES_IN_FLIGHT; i++) {
        BufferHelpers::createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, CONTEXT->lightBuffers[i], lightsBuffersMemory[i]);

        vkMapMemory(CONTEXT->device, lightsBuffersMemory[i], 0, bufferSize, 0, &CONTEXT->lightBuffersMapped[i]);
    }

    CONTEXT->applicationDeletionQueue.PushDeletor(
        [lightsBuffersMemory]()
        {
            for (size_t i = 0; i < CONTEXT->MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroyBuffer(CONTEXT->device, CONTEXT->lightBuffers[i], nullptr);
                vkFreeMemory(CONTEXT->device, lightsBuffersMemory[i], nullptr);
            }
        }
    );
}

void VulkanSetup::createCommandBuffers() {
    CONTEXT->commandBuffers.resize(CONTEXT->MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = CONTEXT->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)CONTEXT->commandBuffers.size();

    if (vkAllocateCommandBuffers(CONTEXT->device, &allocInfo, CONTEXT->commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void VulkanSetup::createSyncObjects() {
    CONTEXT->imageAvailableSemaphores.resize(CONTEXT->MAX_FRAMES_IN_FLIGHT);
    CONTEXT->renderFinishedSemaphores.resize(CONTEXT->MAX_FRAMES_IN_FLIGHT);
    CONTEXT->inFlightFences.resize(CONTEXT->MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < CONTEXT->MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(CONTEXT->device, &semaphoreInfo, nullptr, &CONTEXT->imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(CONTEXT->device, &semaphoreInfo, nullptr, &CONTEXT->renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(CONTEXT->device, &fenceInfo, nullptr, &CONTEXT->inFlightFences[i]) != VK_SUCCESS) {

            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    CONTEXT->resizeDeletionQueue.PushDeletor(
        []()
        {
            for (size_t i = 0; i < CONTEXT->MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroySemaphore(CONTEXT->device, CONTEXT->renderFinishedSemaphores[i], nullptr);
                vkDestroySemaphore(CONTEXT->device, CONTEXT->imageAvailableSemaphores[i], nullptr);
                vkDestroyFence(CONTEXT->device, CONTEXT->inFlightFences[i], nullptr);
            }
        }
    );
}

void VulkanSetup::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(CONTEXT->MAX_FRAMES_IN_FLIGHT * MAX_MODELS);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(CONTEXT->MAX_FRAMES_IN_FLIGHT * MAX_MODELS);
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[2].descriptorCount = static_cast<uint32_t>(CONTEXT->MAX_FRAMES_IN_FLIGHT * MAX_MODELS);
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[3].descriptorCount = static_cast<uint32_t>(CONTEXT->MAX_FRAMES_IN_FLIGHT * MAX_MODELS);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(CONTEXT->MAX_FRAMES_IN_FLIGHT * MAX_MODELS);

    if (vkCreateDescriptorPool(CONTEXT->device, &poolInfo, nullptr, &CONTEXT->descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    CONTEXT->applicationDeletionQueue.PushDeletor(
        []()
        {
            vkDestroyDescriptorPool(CONTEXT->device, CONTEXT->descriptorPool, nullptr);
        }
    );
}

void VulkanSetup::createDebugNameInfo()
{
    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_DEVICE;
    nameInfo.objectHandle = (uint64_t)CONTEXT->device;
    nameInfo.pObjectName = "Device (default)";
    vkSetDebugUtilsObjectName(CONTEXT->device, &nameInfo);

    nameInfo.objectType = VK_OBJECT_TYPE_INSTANCE;
    nameInfo.objectHandle = (uint64_t)CONTEXT->instance;
    nameInfo.pObjectName = "Instance (default)";
    vkSetDebugUtilsObjectName(CONTEXT->device, &nameInfo);

    nameInfo.objectType = VK_OBJECT_TYPE_PHYSICAL_DEVICE;
    nameInfo.objectHandle = (uint64_t)CONTEXT->physicalDevice;
    nameInfo.pObjectName = "Physical Device (default)";
    vkSetDebugUtilsObjectName(CONTEXT->device, &nameInfo);

    nameInfo.objectType = VK_OBJECT_TYPE_QUEUE;
    nameInfo.objectHandle = (uint64_t)CONTEXT->graphicsQueue;
    nameInfo.pObjectName = "Graphics Queue (default)";
    vkSetDebugUtilsObjectName(CONTEXT->device, &nameInfo);
}



VkResult VulkanSetup::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanSetup::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl << std::endl;
    }

    return VK_FALSE;
}

void VulkanSetup::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

std::vector<const char*> VulkanSetup::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool VulkanSetup::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

bool VulkanSetup::isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

SwapChainSupportDetails VulkanSetup::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, CONTEXT->surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, CONTEXT->surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, CONTEXT->surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, CONTEXT->surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, CONTEXT->surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool VulkanSetup::checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices VulkanSetup::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, CONTEXT->surface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

VkSurfaceFormatKHR VulkanSetup::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR VulkanSetup::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSetup::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    else {
        int width, height;
        glfwGetFramebufferSize(CONTEXT->window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

VkFormat VulkanSetup::findDepthFormat() {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat VulkanSetup::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(CONTEXT->physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}
