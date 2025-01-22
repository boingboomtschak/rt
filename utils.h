// utils.h
// Devon McKee, 2025

// Switch for vulkan result types to strings
inline const char* vkResultString(VkResult res) {
    switch (res) {
        // 1.0
        case VK_SUCCESS:
            return "VK_SUCCESS";
            break;
        case VK_NOT_READY:
            return "VK_NOT_READY";
            break;
        case VK_TIMEOUT:
            return "VK_TIMEOUT";
            break;
        case VK_EVENT_SET:
            return "VK_EVENT_SET";
            break;
        case VK_EVENT_RESET:
            return "VK_EVENT_RESET";
            break;
        case VK_INCOMPLETE:
            return "VK_INCOMPLETE";
            break;
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "VK_ERROR_OUT_OF_HOST_MEMORY";
            break;
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            break;
        case VK_ERROR_INITIALIZATION_FAILED:
            return "VK_ERROR_INITIALIZATION_FAILED";
            break;
        case VK_ERROR_DEVICE_LOST:
            return "VK_ERROR_DEVICE_LOST";
            break;
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "VK_ERROR_MEMORY_MAP_FAILED";
            break;
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "VK_ERROR_LAYER_NOT_PRESENT";
            break;
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "VK_ERROR_EXTENSION_NOT_PRESENT";
            break;
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "VK_ERROR_FEATURE_NOT_PRESENT";
            break;
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "VK_ERROR_INCOMPATIBLE_DRIVER";
            break;
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "VK_ERROR_TOO_MANY_OBJECTS";
            break;
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            break;
        case VK_ERROR_FRAGMENTED_POOL:
            return "VK_ERROR_FRAGMENTED_POOL";
            break;
        case VK_ERROR_UNKNOWN:
            return "VK_ERROR_UNKNOWN";
            break;
        // 1.1
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            return "VK_ERROR_OUT_OF_POOL_MEMORY";
            break;
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:
            return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
            break;
        // 1.2
        case VK_ERROR_FRAGMENTATION:
            return "VK_ERROR_FRAGMENTATION";
            break;
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
            return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
            break;
        // 1.3
        case VK_PIPELINE_COMPILE_REQUIRED: 
            return "VK_PIPELINE_COMPILE_REQUIRED"; break;
        default:
            return "UNKNOWN_ERROR";
            break;
    }
}

// Macros for checking Vulkan callbacks
inline void vkAssert(VkResult result, const char *file, int line, bool abort = true) {
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vkAssert: ERROR %s in '%s', line %d\n", vkResultString(result), file, line);
        if (abort) exit(1);
    }
}
#define vkCheck(result) { vkAssert((result), __FILE__, __LINE__); }

inline const char* vkDeviceTypeString(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            return "OTHER_DEVICE";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "INTEGRATED_GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "DISCRETE_GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "VIRTUAL_GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "CPU";
            break;
        default:
            return "UNKNOWN_DEVICE_TYPE";
            break;
    }
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device, std::vector<const char*> deviceExtensions) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    std::set<const char*> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    for (VkExtensionProperties ext : availableExtensions) {
        requiredExtensions.erase(ext.extensionName);
    }
    return requiredExtensions.empty();
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

struct Device {
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue queue;
    uint32_t queueFamilyId;
};

struct Buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
};

void createBuffer(Device device, VkDeviceSize size, Buffer& buffer, VkBufferUsageFlags usage, bool deviceLocal = true, bool deviceAddress = true) {
    VkBufferCreateInfo bufferCI {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (deviceAddress) bufferCI.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    vkCheck(vkCreateBuffer(device.device, &bufferCI, nullptr, &buffer.buffer));
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device.device, buffer.buffer, &memRequirements);

    VkMemoryPropertyFlags properties = deviceLocal ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VkMemoryAllocateFlagsInfo allocFlagsInfo { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
    if (deviceAddress) allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &allocFlagsInfo,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(device.physicalDevice, memRequirements.memoryTypeBits, properties)
    };



    vkCheck(vkAllocateMemory(device.device, &allocInfo, nullptr, &buffer.memory));
    vkCheck(vkBindBufferMemory(device.device, buffer.buffer, buffer.memory, 0));
}

void destroyBuffer(Device device, Buffer buffer) {
    vkDestroyBuffer(device.device, buffer.buffer, nullptr);
    vkFreeMemory(device.device, buffer.memory, nullptr);
}

VkDeviceAddress getBufferDeviceAddress(Device device, Buffer buffer) {
    VkBufferDeviceAddressInfo bufferDeviceAddressInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer.buffer
    };
    return vkGetBufferDeviceAddress(device.device, &bufferDeviceAddressInfo);
}

void copyBuffer(Device device, VkCommandPool commandPool, Buffer srcBuffer, Buffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer commandBuffer;
    vkCheck(vkAllocateCommandBuffers(device.device, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo  beginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, 
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    VkBufferCopy copyRegion {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(commandBuffer, srcBuffer.buffer, dstBuffer.buffer, 1, &copyRegion);
    vkCheck(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };
    vkCheck(vkQueueSubmit(device.queue, 1, &submitInfo, VK_NULL_HANDLE));
    
    vkCheck(vkQueueWaitIdle(device.queue));
    vkFreeCommandBuffers(device.device, commandPool, 1, &commandBuffer);
}

std::vector<char> readFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file!");
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule createShaderModule(Device device, std::vector<char> shaderCode) {
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo shaderModuleCreateInfo {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaderCode.size(),
        .pCode = (const uint32_t*)(shaderCode.data())
    };
    vkCheck(vkCreateShaderModule(device.device, &shaderModuleCreateInfo, nullptr, &shaderModule));
    return shaderModule;
}

struct Swapchain {
    VkSwapchainKHR swapchain;
    std::vector<VkImage> images;
    VkFormat imageFormat;
    VkExtent2D extent;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;
    void create(Device device, GLFWwindow* window, VkSurfaceKHR surface);
    void buildFramebuffers(Device device, VkRenderPass renderPass);
    void destroy(Device device);
};

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
    for (const VkSurfaceFormatKHR &availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
    for (const VkPresentModeKHR &availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, GLFWwindow* window) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D actualExtent = {
            (uint32_t)width,
            (uint32_t)height
        };
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}

void Swapchain::create(Device device, GLFWwindow* window, VkSurfaceKHR surface) {
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.physicalDevice, surface, &formatCount, formats.data());

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.physicalDevice, surface, &presentModeCount, presentModes.data());

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice, surface, &capabilities);

    assert(!formats.empty() && !presentModes.empty());

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats);
    imageFormat = surfaceFormat.format;
    VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
    extent = chooseSwapExtent(capabilities, window);

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR swapchainCI {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = imageFormat,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &device.queueFamilyId,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    vkCheck(vkCreateSwapchainKHR(device.device, &swapchainCI, nullptr, &swapchain));

    vkCheck(vkGetSwapchainImagesKHR(device.device, swapchain, &imageCount, nullptr));
    images.resize(imageCount);
    vkCheck(vkGetSwapchainImagesKHR(device.device, swapchain, &imageCount, images.data()));
    
    imageViews.resize(images.size());
    for (size_t i = 0; i < images.size(); i++) {
        VkImageViewCreateInfo imageViewCI {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = imageFormat,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        vkCheck(vkCreateImageView(device.device, &imageViewCI, nullptr, &imageViews[i]));
    }
}

void Swapchain::buildFramebuffers(Device device, VkRenderPass renderPass) {
    framebuffers.resize(imageViews.size());
    for (size_t i = 0; i < imageViews.size(); i++) {
        VkImageView attachments[] = {
            imageViews[i]
        };
        VkFramebufferCreateInfo framebufferCI {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderPass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = extent.width,
            .height = extent.height,
            .layers = 1
        };
        vkCheck(vkCreateFramebuffer(device.device, &framebufferCI, nullptr, &framebuffers[i]));
    }
}

void Swapchain::destroy(Device device) {
    if (framebuffers.size() > 0) {
        for (VkFramebuffer framebuffer : framebuffers) {
            vkDestroyFramebuffer(device.device, framebuffer, nullptr);
        }
    }
    for (VkImageView imageView : imageViews) {
        vkDestroyImageView(device.device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(device.device, swapchain, nullptr);
}

VkDeviceSize alignedSize(VkDeviceSize value, VkDeviceSize alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

struct ShaderBindingTable {
    Buffer buffer;
    VkStridedDeviceAddressRegionKHR rgenSBTEntry;
    VkStridedDeviceAddressRegionKHR hitGroupSBTEntry;
    VkStridedDeviceAddressRegionKHR missSBTEntry;
    VkStridedDeviceAddressRegionKHR callableSBTEntry;
    void create(Device device, VkPipeline rtPipeline, std::vector<VkRayTracingShaderGroupCreateInfoKHR> rtShaderGroups);
    void destroy(Device device);
};

void ShaderBindingTable::create(Device device, VkPipeline rtPipeline, std::vector<VkRayTracingShaderGroupCreateInfoKHR> rtShaderGroups) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
    VkPhysicalDeviceProperties2 devProp2 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &rtProperties };
    vkGetPhysicalDeviceProperties2(device.physicalDevice, &devProp2);

    VkDeviceSize handleSize = rtProperties.shaderGroupHandleSize;
    VkDeviceSize handleAlignment = rtProperties.shaderGroupBaseAlignment;
    std::vector<uint8_t> shaderHandleStorage(rtShaderGroups.size() * handleSize);
    vkCheck(vkGetRayTracingShaderGroupHandlesKHR(device.device, rtPipeline, 0, rtShaderGroups.size(), shaderHandleStorage.size(), shaderHandleStorage.data()));

    VkDeviceSize sbtSize = handleSize * rtShaderGroups.size();
    VkDeviceSize rgenOffset = 0;
    VkDeviceSize hitGroupOffset = alignedSize(handleSize, handleAlignment);
    VkDeviceSize missOffset = alignedSize(hitGroupOffset + handleSize, handleAlignment);
    createBuffer(device, sbtSize, buffer, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false);

    void* data;
    vkMapMemory(device.device, buffer.memory, 0, sbtSize, 0, &data);
    memcpy((uint8_t*)data + rgenOffset, shaderHandleStorage.data(), handleSize);
    memcpy((uint8_t*)data + hitGroupOffset, shaderHandleStorage.data() + handleSize, handleSize);
    memcpy((uint8_t*)data + missOffset, shaderHandleStorage.data() + handleSize * 2, handleSize);
    vkUnmapMemory(device.device, buffer.memory);

    VkDeviceAddress devAddress = getBufferDeviceAddress(device, buffer);

    rgenSBTEntry = { .deviceAddress = devAddress + rgenOffset, .stride = handleSize, .size = handleSize };
    hitGroupSBTEntry = { .deviceAddress = devAddress + hitGroupOffset, .stride = handleSize, .size = handleSize };
    missSBTEntry = { .deviceAddress = devAddress + missOffset, .stride = handleSize, .size = handleSize };
    callableSBTEntry = {};
}

void ShaderBindingTable::destroy(Device device) {
    destroyBuffer(device, buffer);
}