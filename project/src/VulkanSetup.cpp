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

const int MAX_FRAMES_IN_FLIGHT = 3;
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
    initializeVMA();
    createSwapChain();
    createGBuffers();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createDepthResources();
    createFramebuffers();
    createUniformBuffers();
    createCommandBuffers();
    createSyncObjects();
    createDescriptorPool();

    vkSetDebugUtilsObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(CONTEXT->device, "vkSetDebugUtilsObjectNameEXT");

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
    createFramebuffers();
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
    appInfo.apiVersion = VK_API_VERSION_1_4;

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

    VkPhysicalDeviceVulkan14Features features14{};
    features14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.pNext = &features14;
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
            vmaDestroyPool(CONTEXT->vmaAllocator, CONTEXT->vmaPool);
            vmaDestroyAllocator(CONTEXT->vmaAllocator);
        }
    );

    //VkImageCreateInfo imageInfo{};
    //imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    //imageInfo.imageType = VK_IMAGE_TYPE_2D;
    //imageInfo.extent.width = 128;
    //imageInfo.extent.height = 128;
    //imageInfo.extent.depth = 1;
    //imageInfo.mipLevels = 1;
    //imageInfo.arrayLayers = 1;
    //imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    //imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    //imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //imageInfo.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    //imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    //imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    //VmaAllocationCreateInfo allocInfo{};
    //allocInfo.pool = CONTEXT->vmaPool;

    //VmaAllocationInfo vmaAllocationInfo{};

    //VkResult result = vmaCreateImage(CONTEXT->vmaAllocator, &imageInfo, &allocInfo, &CONTEXT->image, &CONTEXT->vmaAllocation, &vmaAllocationInfo);
    //if (result != VK_SUCCESS)
    //{
    //    std::cout << string_VkResult(result) << "\n";
    //    throw std::runtime_error("failed to create image using vma! ");
    //}

    //vmaDestroyImage(CONTEXT->vmaAllocator, CONTEXT->image, CONTEXT->vmaAllocation);
}

void VulkanSetup::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(CONTEXT->physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

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
    CONTEXT->gBuffers.push_back(ImageHelpers::createGBuffer(VK_FORMAT_R8G8B8A8_SRGB, clearValue));

    // normals
    CONTEXT->gBuffers.push_back(ImageHelpers::createGBuffer(VK_FORMAT_R32G32_SFLOAT, clearValue));

    // metalic roughness
    CONTEXT->gBuffers.push_back(ImageHelpers::createGBuffer(VK_FORMAT_R8G8B8A8_UNORM, clearValue));

    CONTEXT->resizeDeletionQueue.PushDeletor(
        []()
        {
            for (gBuffer* b : CONTEXT->gBuffers)
            {
                vkDestroyImageView(CONTEXT->device, b->image.view, nullptr);

                vkDestroyImage(CONTEXT->device, b->image.image, nullptr);
                vkFreeMemory(CONTEXT->device, b->image.memory, nullptr);
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

void VulkanSetup::createRenderPass()
{
    // Attachments
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = CONTEXT->swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::vector<VkAttachmentDescription> attachments = {
        colorAttachment,
        depthAttachment,
    };

    for (gBuffer* b : CONTEXT->gBuffers)
    {
        VkAttachmentDescription bufferAttachment{};
        bufferAttachment.format = b->format;
        bufferAttachment.samples = b->sampleCount;
        bufferAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        bufferAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        bufferAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        bufferAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        bufferAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        bufferAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        attachments.push_back(bufferAttachment);
    }

    // references
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::vector<VkAttachmentReference> references
    {
        colorAttachmentRef
    };

    uint32_t attachmentIndex = references.size();

    for (gBuffer* b : CONTEXT->gBuffers)
    {
        attachmentIndex++;

        VkAttachmentReference bufferReference{};
        bufferReference.attachment = attachmentIndex;
        bufferReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        references.push_back(bufferReference);
    }

    // subpasses
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = references.size();
    subpass.pColorAttachments = references.data();
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDescription subpass2{};
    subpass2.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass2.colorAttachmentCount = references.size();
    subpass2.pColorAttachments = references.data();
    subpass2.pDepthStencilAttachment = &depthAttachmentRef;

    std::vector<VkSubpassDescription> subpasses
    {
        subpass,
    };

    // dependency
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // render pass info
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = subpasses.size();
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(CONTEXT->device, &renderPassInfo, nullptr, &CONTEXT->renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

    CONTEXT->applicationDeletionQueue.PushDeletor(
        []()
        {
            vkDestroyRenderPass(CONTEXT->device, CONTEXT->renderPass, nullptr);
        }
    );
}

void VulkanSetup::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(CONTEXT->device, &layoutInfo, nullptr, &CONTEXT->descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    CONTEXT->applicationDeletionQueue.PushDeletor(
        []()
        {
            vkDestroyDescriptorSetLayout(CONTEXT->device, CONTEXT->descriptorSetLayout, nullptr);
        }
    );
}

void VulkanSetup::createGraphicsPipeline() {
    auto vertShaderCode = readFile("shaders/shader_vert.spv");
    auto fragShaderCode = readFile("shaders/shader_frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments
    {
        colorBlendAttachment,
    };

    for (gBuffer* b : CONTEXT->gBuffers)
    {
        colorBlendAttachments.push_back(colorBlendAttachment);
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = colorBlendAttachments.size();
    colorBlending.pAttachments = colorBlendAttachments.data();
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pSetLayouts = &CONTEXT->descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if (vkCreatePipelineLayout(CONTEXT->device, &pipelineLayoutInfo, nullptr, &CONTEXT->pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = CONTEXT->pipelineLayout;
    pipelineInfo.renderPass = CONTEXT->renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional
    pipelineInfo.pDepthStencilState = &depthStencil;

    if (vkCreateGraphicsPipelines(CONTEXT->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &CONTEXT->graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(CONTEXT->device, fragShaderModule, nullptr);
    vkDestroyShaderModule(CONTEXT->device, vertShaderModule, nullptr);

    CONTEXT->applicationDeletionQueue.PushDeletor(
        []()
        {
            vkDestroyPipeline(CONTEXT->device, CONTEXT->graphicsPipeline, nullptr);
            vkDestroyPipelineLayout(CONTEXT->device, CONTEXT->pipelineLayout, nullptr);
        }
    );
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
    VkImage depthImage{};
    VkDeviceMemory depthImageMemory{};

    VkFormat depthFormat = findDepthFormat();
    ImageHelpers::createImage(CONTEXT->swapChainExtent.width, CONTEXT->swapChainExtent.height, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    CONTEXT->depthImageView = ImageHelpers::createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

    ImageHelpers::transitionImageLayout(depthImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, VK_IMAGE_ASPECT_DEPTH_BIT);

    CONTEXT->resizeDeletionQueue.PushDeletor(
        [depthImage, depthImageMemory]()
        {
            vkDestroyImageView(CONTEXT->device, CONTEXT->depthImageView, nullptr);
            vkDestroyImage(CONTEXT->device, depthImage, nullptr);
            vkFreeMemory(CONTEXT->device, depthImageMemory, nullptr);
        }
    );
}

void VulkanSetup::createFramebuffers() {
    std::vector<VkFramebuffer> swapChainFramebuffers{};
    CONTEXT->swapChainFramebuffers.resize(CONTEXT->swapChainImageViews.size());

    for (size_t i = 0; i < CONTEXT->swapChainImageViews.size(); i++) {
        std::vector<VkImageView> attachments = {
            CONTEXT->swapChainImageViews[i],
            CONTEXT->depthImageView,
        };

        for (gBuffer* b : CONTEXT->gBuffers)
        {
            attachments.push_back(b->image.view);
        }

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = CONTEXT->renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = CONTEXT->swapChainExtent.width;
        framebufferInfo.height = CONTEXT->swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(CONTEXT->device, &framebufferInfo, nullptr, &CONTEXT->swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }

    CONTEXT->resizeDeletionQueue.PushDeletor(
        []()
        {
            for (auto framebuffer : CONTEXT->swapChainFramebuffers) {
                vkDestroyFramebuffer(CONTEXT->device, framebuffer, nullptr);
            }
        }
    );
}

void VulkanSetup::createUniformBuffers() {
    std::vector<VkDeviceMemory> uniformBuffersMemory{};

    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    CONTEXT->uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    CONTEXT->uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        BufferHelpers::createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, CONTEXT->uniformBuffers[i], uniformBuffersMemory[i]);

        vkMapMemory(CONTEXT->device, uniformBuffersMemory[i], 0, bufferSize, 0, &CONTEXT->uniformBuffersMapped[i]);
    }

    CONTEXT->applicationDeletionQueue.PushDeletor(
        [uniformBuffersMemory]()
        {
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroyBuffer(CONTEXT->device, CONTEXT->uniformBuffers[i], nullptr);
                vkFreeMemory(CONTEXT->device, uniformBuffersMemory[i], nullptr);
            }
        }
    );
}

void VulkanSetup::createCommandBuffers() {
    CONTEXT->commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

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
    CONTEXT->imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    CONTEXT->renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    CONTEXT->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(CONTEXT->device, &semaphoreInfo, nullptr, &CONTEXT->imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(CONTEXT->device, &semaphoreInfo, nullptr, &CONTEXT->renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(CONTEXT->device, &fenceInfo, nullptr, &CONTEXT->inFlightFences[i]) != VK_SUCCESS) {

            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    CONTEXT->resizeDeletionQueue.PushDeletor(
        []()
        {
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                vkDestroySemaphore(CONTEXT->device, CONTEXT->renderFinishedSemaphores[i], nullptr);
                vkDestroySemaphore(CONTEXT->device, CONTEXT->imageAvailableSemaphores[i], nullptr);
                vkDestroyFence(CONTEXT->device, CONTEXT->inFlightFences[i], nullptr);
            }
        }
    );
}

void VulkanSetup::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * MAX_MODELS);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * MAX_MODELS);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * MAX_MODELS);

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

    nameInfo.objectType = VK_OBJECT_TYPE_PIPELINE;
    nameInfo.objectHandle = (uint64_t)CONTEXT->graphicsPipeline;
    nameInfo.pObjectName = "Pipeline (default)";
    vkSetDebugUtilsObjectName(CONTEXT->device, &nameInfo);

    nameInfo.objectType = VK_OBJECT_TYPE_QUEUE;
    nameInfo.objectHandle = (uint64_t)CONTEXT->graphicsQueue;
    nameInfo.pObjectName = "Graphics Queue (default)";
    vkSetDebugUtilsObjectName(CONTEXT->device, &nameInfo);

    nameInfo.objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
    nameInfo.objectHandle = (uint64_t)CONTEXT->pipelineLayout;
    nameInfo.pObjectName = "Pipeline Layout (default)";
    vkSetDebugUtilsObjectName(CONTEXT->device, &nameInfo);

    nameInfo.objectType = VK_OBJECT_TYPE_RENDER_PASS;
    nameInfo.objectHandle = (uint64_t)CONTEXT->renderPass;
    nameInfo.pObjectName = "Render Pass (default)";
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

std::vector<char> VulkanSetup::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

VkShaderModule VulkanSetup::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(CONTEXT->device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }
    return shaderModule;
}
