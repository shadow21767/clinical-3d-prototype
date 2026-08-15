// 3D Clinical Procedure Walkthrough - native Vulkan (MoltenVK on macOS)
//
// An interactive walkthrough of a trauma primary survey. An animated responder
// carries out each step on a patient; the teaching panel, step rail, and vitals
// monitor are CPU-painted textures composited by the GPU.
//
// Controls: arrows/space step, mouse drag orbit, right-drag pan, scroll zoom,
// click a marker to jump, R resets the camera, H toggles markers, ESC quits.

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "canvas.hpp"
#include "mesh.hpp"
#include "scene.hpp"
#include "steps.hpp"
#include "ui.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

namespace {

constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 800;
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

constexpr int MONITOR_TEX_W = 640;
constexpr int MONITOR_TEX_H = 320;
constexpr int UI_TEX_W = 1280;
constexpr int UI_TEX_H = 800;

struct PushConstants {
    glm::mat4 model;
    glm::vec4 color;
};

struct ViewProjUBO {
    glm::mat4 view;
    glm::mat4 proj;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

// Directory of the running executable, set from argv[0].
std::string gExeDir = ".";

// SPIR-V lands next to the binary, but the app may be launched from anywhere.
std::string shaderPath(const std::string& name) {
    for (const std::string& dir : {gExeDir + "/shaders", std::string("shaders"), std::string("build/shaders")}) {
        const std::string candidate = dir + "/" + name;
        if (std::ifstream(candidate).good()) return candidate;
    }
    throw std::runtime_error("could not locate compiled shader: " + name +
                             " (build the project so shaders/*.spv exist)");
}

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open shader file: " + filename);
    const size_t size = (size_t)file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), size);
    return buffer;
}

// A CPU-updated texture: image plus a persistently mapped staging buffer.
struct DynamicTexture {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    void* mapped = nullptr;
    uint32_t width = 0, height = 0;
};

}  // namespace

class WalkthroughApp {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    // ---- window / input ----
    GLFWwindow* window = nullptr;
    bool framebufferResized = false;

    int currentStep = 0;
    bool showHotspots = true;

    bool dragging = false;
    bool panning = false;
    double pressX = 0, pressY = 0, lastX = 0, lastY = 0;
    float orbitYaw = 0, orbitPitch = 0, orbitDist = 4.0f;
    glm::vec3 orbitTarget{0.0f};

    // ---- Vulkan core ----
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass;
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    VkDescriptorSetLayout dslUbo;
    VkDescriptorSetLayout dslTex;
    VkPipelineLayout layoutLit;      // set0 = UBO
    VkPipelineLayout layoutTex3d;    // set0 = UBO, set1 = texture
    VkPipelineLayout layoutOverlay;  // set0 = texture
    VkPipeline pipelineLit;
    VkPipeline pipelineFlat;
    VkPipeline pipelineTex3d;
    VkPipeline pipelineOverlay;

    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> uboSets;
    std::vector<VkDescriptorSet> monitorSets;
    std::vector<VkDescriptorSet> uiSets;

    VkSampler sampler;
    std::vector<DynamicTexture> monitorTextures;
    std::vector<DynamicTexture> uiTextures;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<    VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    uint32_t lastImageIndex = 0;

    // ---- content ----
    MeshLibrary meshes;
    Scene scene;
    Canvas monitorCanvas{MONITOR_TEX_W, MONITOR_TEX_H};
    Canvas uiCanvas{UI_TEX_W, UI_TEX_H};
    int uiDirtyFrames = MAX_FRAMES_IN_FLIGHT;

    const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // =====================================================================
    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Clinical Procedure Walkthrough", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, onResize);
        glfwSetKeyCallback(window, onKey);
        glfwSetMouseButtonCallback(window, onMouseButton);
        glfwSetCursorPosCallback(window, onCursorPos);
        glfwSetScrollCallback(window, onScroll);
    }

    static WalkthroughApp* self(GLFWwindow* w) {
        return reinterpret_cast<WalkthroughApp*>(glfwGetWindowUserPointer(w));
    }

    static void onResize(GLFWwindow* w, int, int) { self(w)->framebufferResized = true; }

    static void onKey(GLFWwindow* w, int key, int, int action, int) {
        if (action != GLFW_PRESS) return;
        WalkthroughApp* app = self(w);
        switch (key) {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(w, GLFW_TRUE);
                break;
            case GLFW_KEY_RIGHT:
            case GLFW_KEY_SPACE:
                app->goToStep(app->currentStep + 1);
                break;
            case GLFW_KEY_LEFT:
                app->goToStep(app->currentStep - 1);
                break;
            case GLFW_KEY_R:
                app->scene.setFollowing(true);
                app->uiDirtyFrames = MAX_FRAMES_IN_FLIGHT;
                break;
            case GLFW_KEY_H:
                app->showHotspots = !app->showHotspots;
                app->uiDirtyFrames = MAX_FRAMES_IN_FLIGHT;
                break;
            default:
                break;
        }
    }

    static void onMouseButton(GLFWwindow* w, int button, int action, int) {
        WalkthroughApp* app = self(w);
        double x, y;
        glfwGetCursorPos(w, &x, &y);

        if (action == GLFW_PRESS) {
            app->pressX = app->lastX = x;
            app->pressY = app->lastY = y;
            app->dragging = button == GLFW_MOUSE_BUTTON_LEFT;
            app->panning = button == GLFW_MOUSE_BUTTON_RIGHT;
            app->beginOrbitFromCamera();
        } else if (action == GLFW_RELEASE) {
            const double moved = std::abs(x - app->pressX) + std::abs(y - app->pressY);
            if (moved < 4.0 && button == GLFW_MOUSE_BUTTON_LEFT) app->pickHotspot(x, y);
            app->dragging = app->panning = false;
        }
    }

    static void onCursorPos(GLFWwindow* w, double x, double y) {
        WalkthroughApp* app = self(w);
        const double dx = x - app->lastX;
        const double dy = y - app->lastY;
        app->lastX = x;
        app->lastY = y;
        if (!app->dragging && !app->panning) return;

        if (app->dragging) {
            app->orbitYaw -= (float)dx * 0.006f;
            app->orbitPitch = std::clamp(app->orbitPitch - (float)dy * 0.006f, -1.45f, 1.45f);
        } else {
            // Pan across the camera's own right/up axes.
            const glm::vec3 fwd = glm::normalize(app->orbitTarget - app->scene.cameraPos());
            const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
            const glm::vec3 up = glm::cross(right, fwd);
            const float scale = app->orbitDist * 0.0016f;
            app->orbitTarget += right * (float)-dx * scale + up * (float)dy * scale;
        }
        app->takeOverCamera();
        app->applyOrbit();
    }

    static void onScroll(GLFWwindow* w, double, double dy) {
        WalkthroughApp* app = self(w);
        app->beginOrbitFromCamera();
        app->orbitDist = std::clamp(app->orbitDist * (float)std::pow(0.9, dy), 0.5f, 12.0f);
        app->takeOverCamera();
        app->applyOrbit();
    }

    void goToStep(int index) {
        const int clamped = std::clamp(index, 0, (int)steps().size() - 1);
        if (clamped == currentStep && scene.following()) return;
        currentStep = clamped;
        scene.setFollowing(true);
        uiDirtyFrames = MAX_FRAMES_IN_FLIGHT;
        const Step& s = steps()[currentStep];
        std::cout << "Step " << currentStep + 1 << "/" << steps().size() << ": " << s.title << std::endl;
    }

    void takeOverCamera() {
        if (scene.following()) {
            scene.setFollowing(false);
            uiDirtyFrames = MAX_FRAMES_IN_FLIGHT;
        }
    }

    // Derive orbit angles from wherever the camera currently is.
    void beginOrbitFromCamera() {
        orbitTarget = scene.cameraTarget();
        const glm::vec3 offset = scene.cameraPos() - orbitTarget;
        orbitDist = std::max(0.5f, glm::length(offset));
        orbitPitch = std::asin(std::clamp(offset.y / orbitDist, -1.0f, 1.0f));
        orbitYaw = std::atan2(offset.x, offset.z);
    }

    void applyOrbit() {
        const glm::vec3 offset{orbitDist * std::cos(orbitPitch) * std::sin(orbitYaw),
                               orbitDist * std::sin(orbitPitch),
                               orbitDist * std::cos(orbitPitch) * std::cos(orbitYaw)};
        scene.setCamera(orbitTarget + offset, orbitTarget);
    }

    void pickHotspot(double sx, double sy) {
        if (!showHotspots) return;

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        double scaleX = 1.0, scaleY = 1.0;
        int ww, wh;
        glfwGetWindowSize(window, &ww, &wh);
        if (ww > 0 && wh > 0) {
            scaleX = (double)w / ww;
            scaleY = (double)h / wh;
        }

        const float ndcX = (float)(2.0 * sx * scaleX / w - 1.0);
        const float ndcY = (float)(2.0 * sy * scaleY / h - 1.0);

        const ViewProjUBO vp = viewProj();
        const glm::mat4 invVP = glm::inverse(vp.proj * vp.view);
        glm::vec4 nearP = invVP * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
        glm::vec4 farP = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        nearP /= nearP.w;
        farP /= farP.w;

        const glm::vec3 origin{nearP};
        const glm::vec3 dir = glm::normalize(glm::vec3(farP) - origin);

        int best = -1;
        float bestT = std::numeric_limits<float>::max();
        for (size_t i = 0; i < steps().size(); i++) {
            const glm::vec3 center = steps()[i].hotspot;
            const glm::vec3 oc = origin - center;
            const float b = glm::dot(oc, dir);
            const float c = glm::dot(oc, oc) - 0.12f * 0.12f;
            const float disc = b * b - c;
            if (disc < 0.0f) continue;
            const float t = -b - std::sqrt(disc);
            if (t > 0.0f && t < bestT) {
                bestT = t;
                best = (int)i;
            }
        }
        if (best >= 0) goToStep(best);
    }

    // =====================================================================
    void initVulkan() {
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayouts();
        createGraphicsPipelines();
        createCommandPool();
        createDepthResources();
        createFramebuffers();
        createGeometryBuffers();
        createUniformBuffers();
        createSampler();
        createDynamicTextures();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();

        std::cout << "\n=== Clinical Procedure Walkthrough ===\n"
                  << "Arrows/space: step   drag: orbit   right-drag: pan   scroll: zoom\n"
                  << "Click a marker to jump   R: reset view   H: markers   ESC: quit\n\n"
                  << "Step 1/" << steps().size() << ": " << steps()[0].title << std::endl;
    }

    void createInstance() {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "ClinicalWalkthrough";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.apiVersion = VK_API_VERSION_1_0;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        createInfo.enabledExtensionCount = (uint32_t)extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
            throw std::runtime_error("failed to create Vulkan instance");
    }

    void createSurface() {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("failed to create window surface");
    }

    void pickPhysicalDevice() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        if (count == 0) throw std::runtime_error("no GPUs with Vulkan support found");
        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance, &count, devices.data());

        for (const auto& dev : devices) {
            if (findQueueFamilies(dev).isComplete()) {
                const SwapChainSupportDetails support = querySwapChainSupport(dev);
                if (!support.formats.empty() && !support.presentModes.empty()) {
                    physicalDevice = dev;
                    break;
                }
            }
        }
        if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("no suitable GPU found");
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev) {
        QueueFamilyIndices indices;
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

        for (uint32_t i = 0; i < count; i++) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
            if (present) indices.presentFamily = i;
            if (indices.isComplete()) break;
        }
        return indices;
    }

    void createLogicalDevice() {
        const QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        std::set<uint32_t> unique = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        const float priority = 1.0f;
        for (uint32_t family : unique) {
            VkDeviceQueueCreateInfo qci{};
            qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qci.queueFamilyIndex = family;
            qci.queueCount = 1;
            qci.pQueuePriorities = &priority;
            queueInfos.push_back(qci);
        }

        // portability_subset is required by MoltenVK but not always enumerated.
        std::vector<const char*> enabled = deviceExtensions;
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> available(extCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, available.data());
        for (const auto& ext : available) {
            if (std::strcmp(ext.extensionName, "VK_KHR_portability_subset") == 0)
                enabled.push_back("VK_KHR_portability_subset");
        }

        VkPhysicalDeviceFeatures features{};
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = (uint32_t)queueInfos.size();
        createInfo.pQueueCreateInfos = queueInfos.data();
        createInfo.pEnabledFeatures = &features;
        createInfo.enabledExtensionCount = (uint32_t)enabled.size();
        createInfo.ppEnabledExtensionNames = enabled.data();

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
            throw std::runtime_error("failed to create logical device");

        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev) {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &details.capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, nullptr);
        details.formats.resize(formatCount);
        if (formatCount) vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, details.formats.data());

        uint32_t modeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &modeCount, nullptr);
        details.presentModes.resize(modeCount);
        if (modeCount)
            vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &modeCount, details.presentModes.data());
        return details;
    }

    void createSwapChain() {
        const SwapChainSupportDetails support = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = support.formats[0];
        for (const auto& f : support.formats) {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                surfaceFormat = f;
        }
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& m : support.presentModes) {
            if (m == VK_PRESENT_MODE_MAILBOX_KHR) presentMode = m;
        }

        VkExtent2D extent = support.capabilities.currentExtent;
        if (extent.width == std::numeric_limits<uint32_t>::max()) {
            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            extent = {(uint32_t)w, (uint32_t)h};
            extent.width = std::clamp(extent.width, support.capabilities.minImageExtent.width,
                                      support.capabilities.maxImageExtent.width);
            extent.height = std::clamp(extent.height, support.capabilities.minImageExtent.height,
                                       support.capabilities.maxImageExtent.height);
        }

        uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
            imageCount = support.capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        // TRANSFER_SRC lets --capture read frames back for verification.
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        const QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        const uint32_t families[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = families;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
            throw std::runtime_error("failed to create swap chain");

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView view;
        if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS)
            throw std::runtime_error("failed to create image view");
        return view;
    }

    void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++)
            swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat,
                                                     VK_IMAGE_ASPECT_COLOR_BIT);
    }

    VkFormat findDepthFormat() {
        for (VkFormat format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) return format;
        }
        throw std::runtime_error("failed to find supported depth format");
    }

    void createRenderPass() {
        VkAttachmentDescription color{};
        color.format = swapChainImageFormat;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depth{};
        depth.format = findDepthFormat();
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        const std::array<VkAttachmentDescription, 2> attachments = {color, depth};
        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = (uint32_t)attachments.size();
        info.pAttachments = attachments.data();
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &info, nullptr, &renderPass) != VK_SUCCESS)
            throw std::runtime_error("failed to create render pass");
    }

    void createDescriptorSetLayouts() {
        VkDescriptorSetLayoutBinding ubo{};
        ubo.binding = 0;
        ubo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo.descriptorCount = 1;
        ubo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo uboInfo{};
        uboInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        uboInfo.bindingCount = 1;
        uboInfo.pBindings = &ubo;
        if (vkCreateDescriptorSetLayout(device, &uboInfo, nullptr, &dslUbo) != VK_SUCCESS)
            throw std::runtime_error("failed to create UBO descriptor set layout");

        VkDescriptorSetLayoutBinding tex{};
        tex.binding = 0;
        tex.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        tex.descriptorCount = 1;
        tex.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo texInfo{};
        texInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        texInfo.bindingCount = 1;
        texInfo.pBindings = &tex;
        if (vkCreateDescriptorSetLayout(device, &texInfo, nullptr, &dslTex) != VK_SUCCESS)
            throw std::runtime_error("failed to create texture descriptor set layout");
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = code.size();
        info.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule module;
        if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS)
            throw std::runtime_error("failed to create shader module");
        return module;
    }

    void createGraphicsPipelines() {
        // ---- shared state ----
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::array<VkVertexInputAttributeDescription, 3> attrs{};
        attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)};
        attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
        attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)};

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = (uint32_t)attrs.size();
        vertexInput.pVertexAttributeDescriptions = attrs.data();

        VkPipelineVertexInputStateCreateInfo emptyVertexInput{};
        emptyVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.lineWidth = 1.0f;
        // Generated meshes wind counter-clockwise; the projection flips Y.
        raster.cullMode = VK_CULL_MODE_BACK_BIT;
        raster.frontFace = VK_FRONT_FACE_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthOpaque{};
        depthOpaque.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthOpaque.depthTestEnable = VK_TRUE;
        depthOpaque.depthWriteEnable = VK_TRUE;
        depthOpaque.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineDepthStencilStateCreateInfo depthBlend = depthOpaque;
        depthBlend.depthWriteEnable = VK_FALSE;  // transparency must not occlude

        VkPipelineDepthStencilStateCreateInfo depthNone{};
        depthNone.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

        VkPipelineColorBlendAttachmentState noBlend{};
        noBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        noBlend.blendEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState alphaBlend = noBlend;
        alphaBlend.blendEnable = VK_TRUE;
        alphaBlend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        alphaBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        alphaBlend.colorBlendOp = VK_BLEND_OP_ADD;
        alphaBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        alphaBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        alphaBlend.alphaBlendOp = VK_BLEND_OP_ADD;

        auto blendState = [](const VkPipelineColorBlendAttachmentState& attachment) {
            VkPipelineColorBlendStateCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            info.attachmentCount = 1;
            info.pAttachments = &attachment;
            return info;
        };
        const VkPipelineColorBlendStateCreateInfo blendOff = blendState(noBlend);
        const VkPipelineColorBlendStateCreateInfo blendOn = blendState(alphaBlend);

        const std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = (uint32_t)dynamicStates.size();
        dynamic.pDynamicStates = dynamicStates.data();

        // ---- pipeline layouts ----
        VkPushConstantRange push{};
        push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push.offset = 0;
        push.size = sizeof(PushConstants);

        auto makeLayout = [&](std::vector<VkDescriptorSetLayout> sets, bool withPush) {
            VkPipelineLayoutCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            info.setLayoutCount = (uint32_t)sets.size();
            info.pSetLayouts = sets.data();
            info.pushConstantRangeCount = withPush ? 1 : 0;
            info.pPushConstantRanges = withPush ? &push : nullptr;
            VkPipelineLayout layout;
            if (vkCreatePipelineLayout(device, &info, nullptr, &layout) != VK_SUCCESS)
                throw std::runtime_error("failed to create pipeline layout");
            return layout;
        };
        layoutLit = makeLayout({dslUbo}, true);
        layoutTex3d = makeLayout({dslUbo, dslTex}, true);
        layoutOverlay = makeLayout({dslTex}, false);

        // ---- shader modules ----
        VkShaderModule litVert = createShaderModule(readFile(shaderPath("lit.vert.spv")));
        VkShaderModule litFrag = createShaderModule(readFile(shaderPath("lit.frag.spv")));
        VkShaderModule flatFrag = createShaderModule(readFile(shaderPath("flat.frag.spv")));
        VkShaderModule texFrag = createShaderModule(readFile(shaderPath("tex3d.frag.spv")));
        VkShaderModule overlayVert = createShaderModule(readFile(shaderPath("overlay.vert.spv")));
        VkShaderModule overlayFrag = createShaderModule(readFile(shaderPath("overlay.frag.spv")));

        auto stage = [](VkShaderStageFlagBits bit, VkShaderModule module) {
            VkPipelineShaderStageCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            info.stage = bit;
            info.module = module;
            info.pName = "main";
            return info;
        };

        auto build = [&](VkShaderModule vert, VkShaderModule frag, VkPipelineLayout layout,
                         const VkPipelineVertexInputStateCreateInfo& vin,
                         const VkPipelineDepthStencilStateCreateInfo& depth,
                         const VkPipelineColorBlendStateCreateInfo& blend, VkCullModeFlags cull) {
            const VkPipelineShaderStageCreateInfo stages[] = {stage(VK_SHADER_STAGE_VERTEX_BIT, vert),
                                                              stage(VK_SHADER_STAGE_FRAGMENT_BIT, frag)};
            VkPipelineRasterizationStateCreateInfo r = raster;
            r.cullMode = cull;

            VkGraphicsPipelineCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            info.stageCount = 2;
            info.pStages = stages;
            info.pVertexInputState = &vin;
            info.pInputAssemblyState = &inputAssembly;
            info.pViewportState = &viewportState;
            info.pRasterizationState = &r;
            info.pMultisampleState = &multisample;
            info.pDepthStencilState = &depth;
            info.pColorBlendState = &blend;
            info.pDynamicState = &dynamic;
            info.layout = layout;
            info.renderPass = renderPass;
            info.subpass = 0;

            VkPipeline pipeline;
            if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
                throw std::runtime_error("failed to create graphics pipeline");
            return pipeline;
        };

        pipelineLit = build(litVert, litFrag, layoutLit, vertexInput, depthOpaque, blendOff,
                            VK_CULL_MODE_BACK_BIT);
        // Rings and cones are viewed from both sides, so they don't cull.
        pipelineFlat = build(litVert, flatFrag, layoutLit, vertexInput, depthBlend, blendOn, VK_CULL_MODE_NONE);
        pipelineTex3d = build(litVert, texFrag, layoutTex3d, vertexInput, depthOpaque, blendOff,
                              VK_CULL_MODE_NONE);
        pipelineOverlay = build(overlayVert, overlayFrag, layoutOverlay, emptyVertexInput, depthNone, blendOn,
                                VK_CULL_MODE_NONE);

        for (VkShaderModule m : {litVert, litFrag, flatFrag, texFrag, overlayVert, overlayFrag})
            vkDestroyShaderModule(device, m, nullptr);
    }

    void createCommandPool() {
        VkCommandPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        info.queueFamilyIndex = findQueueFamilies(physicalDevice).graphicsFamily.value();
        if (vkCreateCommandPool(device, &info, nullptr, &commandPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create command pool");
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProps;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }
        throw std::runtime_error("failed to find suitable memory type");
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory) {
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = size;
        info.usage = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &info, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("failed to create buffer");

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device, buffer, &req);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = findMemoryType(req.memoryTypeBits, properties);
        if (vkAllocateMemory(device, &alloc, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate buffer memory");
        vkBindBufferMemory(device, buffer, memory, 0);
    }

    VkCommandBuffer beginSingleTime() {
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandPool = commandPool;
        alloc.commandBufferCount = 1;
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device, &alloc, &cmd);

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);
        return cmd;
    }

    void endSingleTime(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        vkQueueSubmit(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
    }

    void createImage(uint32_t w, uint32_t h, VkFormat format, VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory) {
        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.extent = {w, h, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.format = format;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        info.usage = usage;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device, &info, nullptr, &image) != VK_SUCCESS)
            throw std::runtime_error("failed to create image");

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(device, image, &req);
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = findMemoryType(req.memoryTypeBits, properties);
        if (vkAllocateMemory(device, &alloc, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate image memory");
        vkBindImageMemory(device, image, memory, 0);
    }

    void createDepthResources() {
        const VkFormat format = findDepthFormat();
        createImage(swapChainExtent.width, swapChainExtent.height, format,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    depthImage, depthImageMemory);
        depthImageView = createImageView(depthImage, format, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            const std::array<VkImageView, 2> attachments = {swapChainImageViews[i], depthImageView};
            VkFramebufferCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = renderPass;
            info.attachmentCount = (uint32_t)attachments.size();
            info.pAttachments = attachments.data();
            info.width = swapChainExtent.width;
            info.height = swapChainExtent.height;
            info.layers = 1;
            if (vkCreateFramebuffer(device, &info, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create framebuffer");
        }
    }

    template <typename T>
    void uploadViaStaging(const std::vector<T>& data, VkBufferUsageFlags usage, VkBuffer& buffer,
                          VkDeviceMemory& memory) {
        const VkDeviceSize size = sizeof(T) * data.size();
        VkBuffer staging;
        VkDeviceMemory stagingMemory;
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging,
                     stagingMemory);

        void* mapped;
        vkMapMemory(device, stagingMemory, 0, size, 0, &mapped);
        std::memcpy(mapped, data.data(), (size_t)size);
        vkUnmapMemory(device, stagingMemory);

        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     buffer, memory);

        VkCommandBuffer cmd = beginSingleTime();
        VkBufferCopy region{};
        region.size = size;
        vkCmdCopyBuffer(cmd, staging, buffer, 1, &region);
        endSingleTime(cmd);

        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
    }

    void createGeometryBuffers() {
        uploadViaStaging(meshes.vertices(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer, vertexBufferMemory);
        uploadViaStaging(meshes.indices(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer, indexBufferMemory);
    }

    void createUniformBuffers() {
        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(sizeof(ViewProjUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         uniformBuffers[i], uniformBuffersMemory[i]);
            vkMapMemory(device, uniformBuffersMemory[i], 0, sizeof(ViewProjUBO), 0, &uniformBuffersMapped[i]);
        }
    }

    void createSampler() {
        VkSamplerCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        if (vkCreateSampler(device, &info, nullptr, &sampler) != VK_SUCCESS)
            throw std::runtime_error("failed to create sampler");
    }

    DynamicTexture createDynamicTexture(uint32_t w, uint32_t h) {
        DynamicTexture tex;
        tex.width = w;
        tex.height = h;
        // sRGB so the sampler linearizes the CPU-painted colors for us.
        createImage(w, h, VK_FORMAT_R8G8B8A8_SRGB,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tex.image, tex.memory);
        tex.view = createImageView(tex.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

        createBuffer((VkDeviceSize)w * h * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tex.staging,
                     tex.stagingMemory);
        vkMapMemory(device, tex.stagingMemory, 0, (VkDeviceSize)w * h * 4, 0, &tex.mapped);

        // Start in the layout the shader expects; per-frame uploads round-trip
        // through TRANSFER_DST and back.
        VkCommandBuffer cmd = beginSingleTime();
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = tex.image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTime(cmd);
        return tex;
    }

    void createDynamicTextures() {
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            monitorTextures.push_back(createDynamicTexture(MONITOR_TEX_W, MONITOR_TEX_H));
            uiTextures.push_back(createDynamicTexture(UI_TEX_W, UI_TEX_H));
        }
    }

    void createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> sizes{};
        sizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT};
        sizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 2};

        VkDescriptorPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.poolSizeCount = (uint32_t)sizes.size();
        info.pPoolSizes = sizes.data();
        info.maxSets = MAX_FRAMES_IN_FLIGHT * 3;
        if (vkCreateDescriptorPool(device, &info, nullptr, &descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool");
    }

    std::vector<VkDescriptorSet> allocateSets(VkDescriptorSetLayout layout) {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, layout);
        VkDescriptorSetAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool = descriptorPool;
        info.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        info.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> sets(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &info, sets.data()) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor sets");
        return sets;
    }

    void createDescriptorSets() {
        uboSets = allocateSets(dslUbo);
        monitorSets = allocateSets(dslTex);
        uiSets = allocateSets(dslTex);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.range = sizeof(ViewProjUBO);

            VkWriteDescriptorSet writeUbo{};
            writeUbo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeUbo.dstSet = uboSets[i];
            writeUbo.dstBinding = 0;
            writeUbo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeUbo.descriptorCount = 1;
            writeUbo.pBufferInfo = &bufferInfo;

            VkDescriptorImageInfo monitorInfo{};
            monitorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            monitorInfo.imageView = monitorTextures[i].view;
            monitorInfo.sampler = sampler;

            VkDescriptorImageInfo uiInfo = monitorInfo;
            uiInfo.imageView = uiTextures[i].view;

            VkWriteDescriptorSet writeMonitor = writeUbo;
            writeMonitor.dstSet = monitorSets[i];
            writeMonitor.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeMonitor.pBufferInfo = nullptr;
            writeMonitor.pImageInfo = &monitorInfo;

            VkWriteDescriptorSet writeUi = writeMonitor;
            writeUi.dstSet = uiSets[i];
            writeUi.pImageInfo = &uiInfo;

            const std::array<VkWriteDescriptorSet, 3> writes = {writeUbo, writeMonitor, writeUi};
            vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
        }
    }

    void createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = commandPool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        info.commandBufferCount = (uint32_t)commandBuffers.size();
        if (vkAllocateCommandBuffers(device, &info, commandBuffers.data()) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate command buffers");
    }

    void createSyncObjects() {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create sync objects");
        }
    }

    // =====================================================================
    ViewProjUBO viewProj() {
        ViewProjUBO ubo{};
        ubo.view = glm::lookAt(scene.cameraPos(), scene.cameraTarget(), glm::vec3(0, 1, 0));
        const float aspect = swapChainExtent.height == 0
                                 ? 1.0f
                                 : swapChainExtent.width / (float)swapChainExtent.height;
        ubo.proj = glm::perspective(glm::radians(42.0f), aspect, 0.1f, 100.0f);
        ubo.proj[1][1] *= -1;  // GLM targets OpenGL; flip Y for Vulkan clip space
        return ubo;
    }

    void recordTextureUpload(VkCommandBuffer cmd, const DynamicTexture& tex, const Canvas& canvas) {
        std::memcpy(tex.mapped, canvas.data(), canvas.sizeBytes());

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = tex.image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {tex.width, tex.height, 1};
        vkCmdCopyBufferToImage(cmd, tex.staging, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &barrier);
    }

    void drawItems(VkCommandBuffer cmd, const std::vector<DrawItem>& items) {
        for (const DrawItem& item : items) {
            PushConstants pc{item.model, item.color};
            vkCmdPushConstants(cmd, layoutLit, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
            const MeshRange& range = meshes.range(item.mesh);
            vkCmdDrawIndexed(cmd, range.indexCount, 1, range.firstIndex, 0, 0);
        }
    }

    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS)
            throw std::runtime_error("failed to begin recording command buffer");

        // Texture uploads must happen outside the render pass.
        recordTextureUpload(cmd, monitorTextures[currentFrame], monitorCanvas);
        if (uiDirtyFrames > 0) {
            recordTextureUpload(cmd, uiTextures[currentFrame], uiCanvas);
            uiDirtyFrames--;
        }

        std::array<VkClearValue, 2> clears{};
        // Clear values are linear; the sRGB attachment encodes on write.
        clears[0].color = {{0.0033f, 0.0052f, 0.0080f, 1.0f}};
        clears[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo pass{};
        pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass.renderPass = renderPass;
        pass.framebuffer = swapChainFramebuffers[imageIndex];
        pass.renderArea.extent = swapChainExtent;
        pass.clearValueCount = (uint32_t)clears.size();
        pass.pClearValues = clears.data();
        vkCmdBeginRenderPass(cmd, &pass, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{0.0f, 0.0f, (float)swapChainExtent.width, (float)swapChainExtent.height, 0.0f, 1.0f};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor{{0, 0}, swapChainExtent};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        const VkBuffer buffers[] = {vertexBuffer};
        const VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
        vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // Opaque scene.
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLit);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layoutLit, 0, 1, &uboSets[currentFrame],
                                0, nullptr);
        drawItems(cmd, scene.opaque());

        // Monitor screen.
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineTex3d);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layoutTex3d, 0, 1, &uboSets[currentFrame],
                                0, nullptr);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layoutTex3d, 1, 1,
                                &monitorSets[currentFrame], 0, nullptr);
        {
            PushConstants pc{scene.monitorScreen(), glm::vec4(1.0f)};
            vkCmdPushConstants(cmd, layoutTex3d, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);
            const MeshRange& range = meshes.range(MESH_QUAD);
            vkCmdDrawIndexed(cmd, range.indexCount, 1, range.firstIndex, 0, 0);
        }

        // Transparent markers and highlights.
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineFlat);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layoutLit, 0, 1, &uboSets[currentFrame],
                                0, nullptr);
        if (showHotspots) {
            drawItems(cmd, scene.blended());
        } else {
            // Skip the marker draws at the tail of the list.
            const size_t markerCount = steps().size() * 2;
            std::vector<DrawItem> withoutMarkers(scene.blended().begin(),
                                                scene.blended().end() - (long)markerCount);
            drawItems(cmd, withoutMarkers);
        }

        // Teaching overlay on top of everything.
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineOverlay);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layoutOverlay, 0, 1,
                                &uiSets[currentFrame], 0, nullptr);
        vkCmdDraw(cmd, 6, 1, 0, 0);

        vkCmdEndRenderPass(cmd);
        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) throw std::runtime_error("failed to record command buffer");
    }

    void cleanupSwapChain() {
        vkDestroyImageView(device, depthImageView, nullptr);
        vkDestroyImage(device, depthImage, nullptr);
        vkFreeMemory(device, depthImageMemory, nullptr);
        for (VkFramebuffer fb : swapChainFramebuffers) vkDestroyFramebuffer(device, fb, nullptr);
        for (VkImageView view : swapChainImageViews) vkDestroyImageView(device, view, nullptr);
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    void recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }
        vkDeviceWaitIdle(device);
        cleanupSwapChain();
        createSwapChain();
        createImageViews();
        createDepthResources();
        createFramebuffers();
    }

    void drawFrame(float dt, float time) {
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX,
                                               imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE,
                                               &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image");
        }

        lastImageIndex = imageIndex;
        scene.update(dt, currentStep, time);
        paintMonitor(monitorCanvas, scene.shownVitals(), scene.ecgPhase());
        if (uiDirtyFrames > 0)
            paintPanel(uiCanvas, currentStep, (int)steps().size(), scene.following(), showHotspots);

        const ViewProjUBO ubo = viewProj();
        std::memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));

        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        const VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        const VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = waitSemaphores;
        submit.pWaitDstStageMask = waitStages;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffers[currentFrame];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = signalSemaphores;
        if (vkQueueSubmit(graphicsQueue, 1, &submit, inFlightFences[currentFrame]) != VK_SUCCESS)
            throw std::runtime_error("failed to submit draw command buffer");

        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = signalSemaphores;
        const VkSwapchainKHR chains[] = {swapChain};
        present.swapchainCount = 1;
        present.pSwapchains = chains;
        present.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(presentQueue, &present);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // Reads the last presented frame back and writes it as a PNG.
    void captureFrame(const std::string& path) {
        vkDeviceWaitIdle(device);

        const uint32_t w = swapChainExtent.width, h = swapChainExtent.height;
        const VkDeviceSize size = (VkDeviceSize)w * h * 4;
        VkBuffer readback;
        VkDeviceMemory readbackMemory;
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, readback,
                     readbackMemory);

        VkCommandBuffer cmd = beginSingleTime();
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = swapChainImages[lastImageIndex];
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {w, h, 1};
        vkCmdCopyImageToBuffer(cmd, swapChainImages[lastImageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               readback, 1, &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &barrier);
        endSingleTime(cmd);

        void* mapped;
        vkMapMemory(device, readbackMemory, 0, size, 0, &mapped);
        const uint8_t* src = (const uint8_t*)mapped;
        Canvas out((int)w, (int)h);
        const bool bgra = swapChainImageFormat == VK_FORMAT_B8G8R8A8_SRGB ||
                          swapChainImageFormat == VK_FORMAT_B8G8R8A8_UNORM;
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                const uint8_t* p = src + ((size_t)y * w + x) * 4;
                out.blend((int)x, (int)y, bgra ? rgb(p[2], p[1], p[0]) : rgb(p[0], p[1], p[2]));
            }
        }
        vkUnmapMemory(device, readbackMemory);
        vkDestroyBuffer(device, readback, nullptr);
        vkFreeMemory(device, readbackMemory, nullptr);

        out.writePng(path);
        std::cout << "captured " << path << std::endl;
    }

    // Renders every step to a PNG and exits; used to verify the scene.
    void captureAllSteps(const std::string& dir) {
        float time = 0.0f;
        for (size_t i = 0; i < steps().size(); i++) {
            currentStep = (int)i;
            scene.setFollowing(true);
            uiDirtyFrames = MAX_FRAMES_IN_FLIGHT;
            // Let the easing and idle motion settle before grabbing the frame.
            for (int f = 0; f < 150; f++) {
                glfwPollEvents();
                time += 1.0f / 60.0f;
                drawFrame(1.0f / 60.0f, time);
            }
            captureFrame(dir + "/step" + std::to_string(i + 1) + ".png");
        }
    }

    void mainLoop() {
        if (const char* dir = std::getenv("CLINICAL_CAPTURE")) {
            captureAllSteps(dir);
            vkDeviceWaitIdle(device);
            return;
        }

        using clock = std::chrono::high_resolution_clock;
        const auto start = clock::now();
        auto previous = start;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            const auto now = clock::now();
            const float dt = std::chrono::duration<float>(now - previous).count();
            const float time = std::chrono::duration<float>(now - start).count();
            previous = now;
            drawFrame(dt, time);
        }
        vkDeviceWaitIdle(device);
    }

    void destroyTexture(DynamicTexture& tex) {
        vkDestroyImageView(device, tex.view, nullptr);
        vkDestroyImage(device, tex.image, nullptr);
        vkFreeMemory(device, tex.memory, nullptr);
        vkUnmapMemory(device, tex.stagingMemory);
        vkDestroyBuffer(device, tex.staging, nullptr);
        vkFreeMemory(device, tex.stagingMemory, nullptr);
    }

    void cleanup() {
        cleanupSwapChain();

        for (auto& tex : monitorTextures) destroyTexture(tex);
        for (auto& tex : uiTextures) destroyTexture(tex);
        vkDestroySampler(device, sampler, nullptr);

        vkDestroyPipeline(device, pipelineOverlay, nullptr);
        vkDestroyPipeline(device, pipelineTex3d, nullptr);
        vkDestroyPipeline(device, pipelineFlat, nullptr);
        vkDestroyPipeline(device, pipelineLit, nullptr);
        vkDestroyPipelineLayout(device, layoutOverlay, nullptr);
        vkDestroyPipelineLayout(device, layoutTex3d, nullptr);
        vkDestroyPipelineLayout(device, layoutLit, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkUnmapMemory(device, uniformBuffersMemory[i]);
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        }
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, dslTex, nullptr);
        vkDestroyDescriptorSetLayout(device, dslUbo, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

// Paints the textures and writes them to PNG without opening a window, so the
// font and layout can be checked headlessly. Set CLINICAL_DUMP=<dir>.
static int dumpTextures(const char* dir) {
    Canvas monitor(MONITOR_TEX_W, MONITOR_TEX_H);
    Canvas ui(UI_TEX_W, UI_TEX_H);
    const std::string base = dir;

    paintMonitor(monitor, steps()[0].vitals, 0.15f);
    monitor.writePng(base + "/monitor.png");

    for (size_t i = 0; i < steps().size(); i++) {
        paintPanel(ui, (int)i, (int)steps().size(), i % 2 == 0, true);
        ui.writePng(base + "/panel" + std::to_string(i + 1) + ".png");
    }

    // Font sheet, for checking glyph legibility.
    Canvas sheet(760, 150);
    sheet.clear(rgb(11, 16, 22));
    sheet.text(10, 14, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", rgb(232, 238, 244), 2);
    sheet.text(10, 44, "abcdefghijklmnopqrstuvwxyz", rgb(232, 238, 244), 2);
    sheet.text(10, 74, "0123456789 .,-:;/()%!?'+", rgb(232, 238, 244), 2);
    sheet.text(10, 104, "Kneel at the head. Look, listen, feel.", rgb(143, 163, 181), 2);
    sheet.writePng(base + "/font.png");

    std::cout << "wrote texture dumps to " << base << std::endl;
    return EXIT_SUCCESS;
}

int main(int argc, char** argv) {
    if (argc > 0) {
        const std::string exe = argv[0];
        const size_t slash = exe.find_last_of('/');
        if (slash != std::string::npos) gExeDir = exe.substr(0, slash);
    }

    if (const char* dir = std::getenv("CLINICAL_DUMP")) return dumpTextures(dir);

    WalkthroughApp app;
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
