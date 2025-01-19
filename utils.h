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
};

struct Buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
};

void createBuffer(Device device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, Buffer& buffer) {
    VkBufferCreateInfo bufferCI {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    vkCheck(vkCreateBuffer(device.device, &bufferCI, nullptr, &buffer.buffer));
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device.device, buffer.buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
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

VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
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