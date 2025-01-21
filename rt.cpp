// rt.cpp
// Devon McKee, 2025

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include "volk/volk.h"
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#define TINYOBJLOADER_IMPLEMENTATION
#include <obj/tiny_obj_loader.h>

#include "utils.h"

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

struct Scene {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<float> colors;
    std::vector<uint32_t> indices;
};

struct Context {
    GLFWwindow* window;
    VkInstance instance;
    Device device;
    VkCommandPool commandPool;
    VkSurfaceKHR surface;
    VkAccelerationStructureKHR accelerationStructure;
    VkDescriptorSetLayout rtDescriptorSetLayout;
    VkDescriptorPool rtDescriptorPool;
    VkDescriptorSet rtDescriptorSet;
    VkPipeline rtPipeline;
    Scene scene;
    Buffer vertexBuffer;
    Buffer indexBuffer;
    Buffer accelerationBuffer;
    void initialize();
    void loadScene();
    void createAccelerationStructure();
    void createRTPipeline();
    void render();
    void destroy();
};

void handleKeys(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // Check CTRL + Q
    if (key == GLFW_KEY_Q && action == GLFW_PRESS && mods == GLFW_MOD_CONTROL) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

void Context::initialize() {
    vkCheck(volkInitialize());
    if(!glfwInit() || !glfwVulkanSupported()) {
        fprintf(stderr, "Failed to initialize GLFW!");
        exit(1);
    }

    std::vector<const char*> instanceLayers;
    std::vector<const char*> instanceExtensions;
    std::vector<const char*> deviceExtensions;

    instanceLayers.push_back("VK_LAYER_KHRONOS_validation"); // enable validation layer
    deviceExtensions.push_back("VK_KHR_swapchain");
    deviceExtensions.push_back("VK_KHR_deferred_host_operations");
    deviceExtensions.push_back("VK_KHR_acceleration_structure");
    deviceExtensions.push_back("VK_KHR_ray_tracing_pipeline");

    uint32_t numRequiredInstanceExtensions = 0;
    const char** requiredInstanceExtensions = glfwGetRequiredInstanceExtensions(&numRequiredInstanceExtensions);
    for (uint32_t i = 0; i < numRequiredInstanceExtensions; i++) {
        instanceExtensions.push_back(requiredInstanceExtensions[i]);
    }

    VkApplicationInfo appCI {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "rt",
        .applicationVersion = 1,
        .pEngineName = "None",
        .engineVersion = 1,
        .apiVersion = VK_MAKE_VERSION(1, 3, 0)
    };

    VkInstanceCreateInfo instanceCI {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appCI,
        .enabledLayerCount = (uint32_t)instanceLayers.size(),
        .ppEnabledLayerNames = instanceLayers.data(),
        .enabledExtensionCount = (uint32_t)instanceExtensions.size(),
        .ppEnabledExtensionNames = instanceExtensions.data()
    };

    vkCheck(vkCreateInstance(&instanceCI, nullptr, &instance));
    volkLoadInstance(instance);

    uint32_t numPhysicalDevices = 0;
    vkCheck(vkEnumeratePhysicalDevices(instance, &numPhysicalDevices, nullptr));
    std::vector<VkPhysicalDevice> physicalDevices(numPhysicalDevices);
    vkCheck(vkEnumeratePhysicalDevices(instance, &numPhysicalDevices, physicalDevices.data()));

    if (numPhysicalDevices == 0) {
        fprintf(stderr, "No available/supported devices!\n");
        vkDestroyInstance(instance, nullptr);
        exit(1);
    }

    int d = 0;
    if (physicalDevices.size() > 1) {
        printf("Available GPUs:\n");
        for (uint32_t i = 0; i < numPhysicalDevices; i++) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(physicalDevices[i], &properties);
            printf("%d - %s (%s)\n", i, properties.deviceName, vkDeviceTypeString(properties.deviceType));
        }
        printf("Enter device number: ");
        std::string d_s;
        getline(std::cin, d_s);
        d = stoi(d_s);
        if (d < 0 || d >= physicalDevices.size()) {
            fprintf(stderr, "Incorrect device number '%d'!", d);
            vkDestroyInstance(instance, nullptr);
            exit(1);
        }
    } else {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevices[0], &properties);
        printf("Using '%s (%s)'\n", properties.deviceName, vkDeviceTypeString(properties.deviceType));
    }
    device.physicalDevice = physicalDevices[d];

    uint32_t numQueueFamilies = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device.physicalDevice, &numQueueFamilies, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(numQueueFamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(device.physicalDevice, &numQueueFamilies, queueFamilyProperties.data());

    uint32_t queueFamilyId = -1;
    for (uint32_t i = 0; i < numQueueFamilies; i++) {
        VkQueueFlags queueFlags = queueFamilyProperties[i].queueFlags;
        if ((queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFlags & VK_QUEUE_COMPUTE_BIT) && (queueFlags & VK_QUEUE_TRANSFER_BIT)) {
            queueFamilyId = i;
        }
    }
    if (queueFamilyId == -1) {
        fprintf(stderr, "Failed to find valid queue family!\n");
        vkDestroyInstance(instance, nullptr);
        exit(1);
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCI {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyId,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufferDeviceAddressFeatures {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
        .bufferDeviceAddress = VK_TRUE
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &bufferDeviceAddressFeatures,
        .rayTracingPipeline = VK_TRUE,
        .rayTraversalPrimitiveCulling = VK_TRUE
    };

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &rayTracingPipelineFeatures,
        .accelerationStructure = VK_TRUE
    };

    VkPhysicalDeviceFeatures deviceFeatures {
        .shaderInt64 = VK_TRUE
    };

    VkPhysicalDeviceFeatures2 deviceFeatures2 {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &accelerationStructureFeatures,
        .features = deviceFeatures
    };
    
    VkDeviceCreateInfo deviceCI {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &deviceFeatures2,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCI,
        .enabledExtensionCount = (uint32_t)deviceExtensions.size(),
        .ppEnabledExtensionNames = deviceExtensions.data()
    };

    vkCheck(vkCreateDevice(device.physicalDevice, &deviceCI, nullptr, &device.device));
    volkLoadDevice(device.device);

    vkGetDeviceQueue(device.device, queueFamilyId, 0, &device.queue);

    VkCommandPoolCreateInfo poolCI {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyId
    };
    vkCheck(vkCreateCommandPool(device.device, &poolCI, nullptr, &commandPool));

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, appCI.pApplicationName, nullptr, nullptr);
    glfwSetKeyCallback(window, handleKeys);

    vkCheck(glfwCreateWindowSurface(instance, window, nullptr, &surface));
}

void Context::loadScene() {
    const char* objFile = "teapot.obj";
    tinyobj::ObjReaderConfig readerConfig;
    readerConfig.mtl_search_path = "./";
    readerConfig.triangulate = true;
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(objFile, readerConfig)) {
        if (!reader.Error().empty()) {
            fprintf(stderr, "TinyObjReader: %s\n", reader.Error().c_str());
        }
    }
    if (!reader.Warning().empty()) {
        fprintf(stderr, "TinyObjReader: %s\n", reader.Warning().c_str());
    }
    tinyobj::attrib_t attrib = reader.GetAttrib();
    scene.vertices = attrib.vertices;
    scene.normals = attrib.normals;
    scene.texcoords = attrib.texcoords;
    scene.colors = attrib.colors;
    std::vector<tinyobj::shape_t> shapes = reader.GetShapes();
    for (uint32_t i = 0; i < shapes.size(); i++) {
        for (uint32_t j = 0; j < shapes[i].mesh.indices.size(); j++) {
            scene.indices.push_back(shapes[i].mesh.indices[j].vertex_index); // FIXME: coalesce normal and texcoord vertices together so there's one index, otherwise they won't work
        }
    }
    printf("Loaded '%s', %d vertices and %d triangles\n", objFile, scene.vertices.size() / 3, scene.indices.size() / 3);

    Buffer stagingBuffer;
    createBuffer(device, std::max(scene.vertices.size() * sizeof(float), scene.indices.size() * sizeof(uint32_t)), stagingBuffer, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, false, false);

    createBuffer(device, scene.vertices.size() * sizeof(float), vertexBuffer, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    createBuffer(device, scene.indices.size() * sizeof(uint32_t), indexBuffer, 
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    void* data;
    vkMapMemory(device.device, stagingBuffer.memory, 0, scene.vertices.size() * sizeof(float), 0, &data);
    memcpy(data, scene.vertices.data(), scene.vertices.size() * sizeof(float));
    vkUnmapMemory(device.device, stagingBuffer.memory);

    copyBuffer(device, commandPool, stagingBuffer, vertexBuffer, scene.vertices.size() * sizeof(float));

    vkMapMemory(device.device, stagingBuffer.memory, 0, scene.indices.size() * sizeof(uint32_t), 0, &data);
    memcpy(data, scene.indices.data(), scene.indices.size() * sizeof(uint32_t));
    vkUnmapMemory(device.device, stagingBuffer.memory);

    copyBuffer(device, commandPool, stagingBuffer, indexBuffer, scene.indices.size() * sizeof(uint32_t));

    destroyBuffer(device, stagingBuffer);
}

void Context::createAccelerationStructure() {
    VkDeviceAddress vertexBufferAddress = getBufferDeviceAddress(device, vertexBuffer);
    VkDeviceAddress indexBufferAddress = getBufferDeviceAddress(device, indexBuffer);

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {
            .triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData = { .deviceAddress = vertexBufferAddress },
                .vertexStride = sizeof(float) * 3,
                .maxVertex = (uint32_t)(scene.vertices.size() / 3) - 1,
                .indexType = VK_INDEX_TYPE_UINT32,
                .indexData = { .deviceAddress = indexBufferAddress },
                .transformData = { .deviceAddress = 0 }
            }
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &accelerationStructureGeometry
    };

    uint32_t primitiveCount = (uint32_t)(scene.indices.size() / 3);

    VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR(device.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &accelerationStructureBuildGeometryInfo, &primitiveCount, &accelerationStructureBuildSizesInfo);

    createBuffer(device, accelerationStructureBuildSizesInfo.accelerationStructureSize, accelerationBuffer, 
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    VkAccelerationStructureCreateInfoKHR accelerationStructureCI {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = accelerationBuffer.buffer,
        .size = accelerationStructureBuildSizesInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

    vkCheck(vkCreateAccelerationStructureKHR(device.device, &accelerationStructureCI, nullptr, &accelerationStructure));

    VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo  {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = accelerationStructure
    };
    VkDeviceAddress accelerationStructureDeviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device.device, &accelerationStructureDeviceAddressInfo);
    
    Buffer scratchBuffer;
    createBuffer(device, accelerationStructureBuildSizesInfo.buildScratchSize, scratchBuffer, 
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    VkDeviceAddress scratchBufferAddress = getBufferDeviceAddress(device, scratchBuffer);

    accelerationStructureBuildGeometryInfo.dstAccelerationStructure = accelerationStructure;
    accelerationStructureBuildGeometryInfo.scratchData = { .deviceAddress = scratchBufferAddress };

    VkCommandBufferAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer commandBuffer;
    vkCheck(vkAllocateCommandBuffers(device.device, &allocInfo, &commandBuffer));
    VkCommandBufferBeginInfo beginInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo {
        .primitiveCount = primitiveCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };

    const VkAccelerationStructureBuildRangeInfoKHR* pAccelerationStructureBuildRangeInfos = &accelerationStructureBuildRangeInfo;
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &accelerationStructureBuildGeometryInfo, &pAccelerationStructureBuildRangeInfos);

    vkCheck(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };

    vkCheck(vkQueueSubmit(device.queue, 1, &submitInfo, VK_NULL_HANDLE));
    vkCheck(vkQueueWaitIdle(device.queue));

    vkFreeCommandBuffers(device.device, commandPool, 1, &commandBuffer);

    destroyBuffer(device, scratchBuffer);
}

void Context::createRTPipeline() {
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
        }, 
        { // TODO need to create image
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
        },
        { // TODO need to create UBO
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
        }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = (uint32_t)bindings.size(),
        .pBindings = bindings.data()
    };
    vkCheck(vkCreateDescriptorSetLayout(device.device, &layoutInfo, nullptr, &rtDescriptorSetLayout));

    std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {
        { .type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .descriptorCount = 1 },
        { .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1 },
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1 }
    };

    VkDescriptorPoolCreateInfo descriptorPoolCI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = 1,
        .poolSizeCount = (uint32_t)descriptorPoolSizes.size(),
        .pPoolSizes = descriptorPoolSizes.data()
    };
    vkCheck(vkCreateDescriptorPool(device.device, &descriptorPoolCI, nullptr, &rtDescriptorPool));

    // allocate descriptor set from layout
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rtDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &rtDescriptorSetLayout
    };
    vkCheck(vkAllocateDescriptorSets(device.device, &descriptorSetAllocInfo, &rtDescriptorSet));

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &rtDescriptorSetLayout,
        // TODO need to figure out push constant range here
    };
    VkPipelineLayout pipelineLayout;
    vkCheck(vkCreatePipelineLayout(device.device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

    VkShaderModule rgenShader = createShaderModule(device, readFile("shaders/gen.spv"));
    VkShaderModule chitShader = createShaderModule(device, readFile("shaders/chit.spv"));
    VkShaderModule missShader = createShaderModule(device, readFile("shaders/miss.spv"));
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = rgenShader,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = chitShader,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = missShader,
            .pName = "main"
        }
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> rayTracingShaderGroups {
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 0,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 1,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        },
        {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = 2,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        }
    };

    VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = (uint32_t)shaderStages.size(),
        .pStages = shaderStages.data(),
        .groupCount = (uint32_t)rayTracingShaderGroups.size(),
        .pGroups = rayTracingShaderGroups.data(),
        .layout = pipelineLayout,
    };
    vkCheck(vkCreateRayTracingPipelinesKHR(device.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &rtPipeline));

    vkDestroyShaderModule(device.device, rgenShader, nullptr);
    vkDestroyShaderModule(device.device, chitShader, nullptr);
    vkDestroyShaderModule(device.device, missShader, nullptr);
    vkDestroyPipelineLayout(device.device, pipelineLayout, nullptr);
}

void Context::render() {

}

void Context::destroy() {
    vkCheck(vkResetDescriptorPool(device.device, rtDescriptorPool, 0));
    vkDestroyDescriptorPool(device.device, rtDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device.device, rtDescriptorSetLayout, nullptr);
    vkDestroyPipeline(device.device, rtPipeline, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyAccelerationStructureKHR(device.device, accelerationStructure, nullptr);
    destroyBuffer(device, accelerationBuffer);
    destroyBuffer(device, vertexBuffer);
    destroyBuffer(device, indexBuffer);
    vkDestroyCommandPool(device.device, commandPool, nullptr);
    vkDestroyDevice(device.device, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
}

int main() {
    Context ctx;
    ctx.initialize();
    printf("Initialized context.\n");

    ctx.loadScene();
    printf("Loaded scene.\n");

    ctx.createAccelerationStructure();
    printf("Created acceleration structure.\n");

    ctx.createRTPipeline();
    printf("Created RT pipeline.\n");

    printf("Rendering...\n");
    while (!glfwWindowShouldClose(ctx.window)) {
        ctx.render();
        glfwPollEvents();
    }

    printf("Destroying context...\n");
    ctx.destroy();
    return 0;
}