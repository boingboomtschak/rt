// rt.cpp
// Devon McKee, 2025

#include <iostream>
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
    std::vector<int> indices;
};

struct Context {
    GLFWwindow* window;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkSurfaceKHR surface;
    Scene scene;
    void initialize();
    void loadScene();
    void createAccelerationStructures();
    void destroy();
};

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
    physicalDevice = physicalDevices[d];

    uint32_t numQueueFamilies = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(numQueueFamilies);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &numQueueFamilies, queueFamilyProperties.data());

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
    
    VkDeviceCreateInfo deviceCI {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCI,
        .enabledExtensionCount = (uint32_t)deviceExtensions.size(),
        .ppEnabledExtensionNames = deviceExtensions.data()
    };

    vkCheck(vkCreateDevice(physicalDevice, &deviceCI, nullptr, &device));
    volkLoadDevice(device);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, appCI.pApplicationName, nullptr, nullptr);

    vkCheck(glfwCreateWindowSurface(instance, window, nullptr, &surface));
}

void Context::loadScene() {
    tinyobj::ObjReaderConfig readerConfig;
    readerConfig.mtl_search_path = "./";
    readerConfig.triangulate = true;
    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile("teapot.obj", readerConfig)) {
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
        printf("Inserting shape %d of size %d...\n", i, shapes[i].mesh.num_face_vertices[0]);
        for (uint32_t j = 0; j < shapes[i].mesh.indices.size(); j++) {
            scene.indices.push_back(shapes[i].mesh.indices[j].vertex_index); // FIXME: coalesce normal and texcoord vertices together so there's one index, otherwise they won't work
        }
    }
    printf("# tris: %d\n", scene.indices.size() / 3);
}

void Context::createAccelerationStructures() {
    VkAccelerationStructureGeometryTrianglesDataKHR triangles {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT
    };

    VkAccelerationStructureGeometryKHR geometry {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR
    };
}

void Context::destroy() {
    vkDestroySurfaceKHR(instance, surface, nullptr);
    glfwDestroyWindow(window);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwTerminate();
}

int main() {
    Context ctx;
    ctx.initialize();
    printf("Initialized context.\n");

    ctx.loadScene();
    printf("Loaded scene.\n");

    printf("Rendering...\n");
    while (!glfwWindowShouldClose(ctx.window)) {
        glfwPollEvents();
    }

    printf("Destroying context...\n");
    ctx.destroy();
    return 0;
}