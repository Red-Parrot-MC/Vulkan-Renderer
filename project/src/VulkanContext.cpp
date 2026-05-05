#include "VulkanContext.h"

VulkanContext* CurrentVulkanContext::context = nullptr;
PFN_vkSetDebugUtilsObjectNameEXT CurrentVulkanContext::vkSetDebugUtilsObjectNameEXT{};