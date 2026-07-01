// GlobeVulkan.cpp – Vulkan globe viewer using 162 height tiles
// Compile: g++ -std=c++20 -static-libgcc -static-libstdc++ Code/GlobeVulkan.cpp -o Executables/GlobeVulkan.exe
//   -I /c/VulkanSDK/1.4.350.0/Include
//   -I /c/msys64/ucrt64/include
//   -I .dependencies
//   -L /c/msys64/ucrt64/lib
//   -lvulkan-1

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#define GLM_FORCE_PURE
#define GLM_FORCE_CTOR_INIT
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLFW_INCLUDE_VULKAN
#undef GLFW_DLL
#undef GLFWAPI
#define GLFWAPI
#include <GLFW/glfw3.h>

// glfw3.dll runtime loader — loaded manually from .dlls/ to avoid
// Windows-loader dependency resolution before main().
static HMODULE s_glfw = nullptr;

static void loadGlfw()
{
    if (!s_glfw)
        s_glfw = LoadLibraryA("glfw3.dll");
}

// Forwarding stubs — same names as GLFW declarations, so the C++ linker
// uses these instead of the glfw3.dll import library (which we don't link).
// GLFWAPI is cleared above so glfw3.h declares them without dllimport.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
extern "C" {
int glfwInit() {
    static auto fn = (int(*)())GetProcAddress(s_glfw, "glfwInit");
    return fn();
}
void glfwPollEvents() {
    static auto fn = (void(*)())GetProcAddress(s_glfw, "glfwPollEvents");
    fn();
}
void glfwTerminate() {
    static auto fn = (void(*)())GetProcAddress(s_glfw, "glfwTerminate");
    fn();
}
void glfwWindowHint(int a, int b) {
    static auto fn = (void(*)(int,int))GetProcAddress(s_glfw, "glfwWindowHint");
    fn(a, b);
}
GLFWwindow* glfwCreateWindow(int a, int b, const char* c, GLFWmonitor* d, GLFWwindow* e) {
    static auto fn = (GLFWwindow*(*)(int,int,const char*,GLFWmonitor*,GLFWwindow*))GetProcAddress(s_glfw, "glfwCreateWindow");
    return fn(a, b, c, d, e);
}
void glfwDestroyWindow(GLFWwindow* a) {
    static auto fn = (void(*)(GLFWwindow*))GetProcAddress(s_glfw, "glfwDestroyWindow");
    fn(a);
}
void glfwSetWindowUserPointer(GLFWwindow* a, void* b) {
    static auto fn = (void(*)(GLFWwindow*,void*))GetProcAddress(s_glfw, "glfwSetWindowUserPointer");
    fn(a, b);
}
GLFWkeyfun glfwSetKeyCallback(GLFWwindow* a, GLFWkeyfun b) {
    static auto fn = (GLFWkeyfun(*)(GLFWwindow*,GLFWkeyfun))GetProcAddress(s_glfw, "glfwSetKeyCallback");
    return fn(a, b);
}
GLFWmousebuttonfun glfwSetMouseButtonCallback(GLFWwindow* a, GLFWmousebuttonfun b) {
    static auto fn = (GLFWmousebuttonfun(*)(GLFWwindow*,GLFWmousebuttonfun))GetProcAddress(s_glfw, "glfwSetMouseButtonCallback");
    return fn(a, b);
}
GLFWcursorposfun glfwSetCursorPosCallback(GLFWwindow* a, GLFWcursorposfun b) {
    static auto fn = (GLFWcursorposfun(*)(GLFWwindow*,GLFWcursorposfun))GetProcAddress(s_glfw, "glfwSetCursorPosCallback");
    return fn(a, b);
}
GLFWscrollfun glfwSetScrollCallback(GLFWwindow* a, GLFWscrollfun b) {
    static auto fn = (GLFWscrollfun(*)(GLFWwindow*,GLFWscrollfun))GetProcAddress(s_glfw, "glfwSetScrollCallback");
    return fn(a, b);
}
void* glfwGetWindowUserPointer(GLFWwindow* a) {
    static auto fn = (void*(*)(GLFWwindow*))GetProcAddress(s_glfw, "glfwGetWindowUserPointer");
    return fn(a);
}
void glfwGetFramebufferSize(GLFWwindow* a, int* b, int* c) {
    static auto fn = (void(*)(GLFWwindow*,int*,int*))GetProcAddress(s_glfw, "glfwGetFramebufferSize");
    fn(a, b, c);
}
int glfwGetKey(GLFWwindow* a, int b) {
    static auto fn = (int(*)(GLFWwindow*,int))GetProcAddress(s_glfw, "glfwGetKey");
    return fn(a, b);
}
void glfwGetCursorPos(GLFWwindow* a, double* b, double* c) {
    static auto fn = (void(*)(GLFWwindow*,double*,double*))GetProcAddress(s_glfw, "glfwGetCursorPos");
    fn(a, b, c);
}
int glfwWindowShouldClose(GLFWwindow* a) {
    static auto fn = (int(*)(GLFWwindow*))GetProcAddress(s_glfw, "glfwWindowShouldClose");
    return fn(a);
}
void glfwSetWindowShouldClose(GLFWwindow* a, int b) {
    static auto fn = (void(*)(GLFWwindow*,int))GetProcAddress(s_glfw, "glfwSetWindowShouldClose");
    fn(a, b);
}
const char** glfwGetRequiredInstanceExtensions(uint32_t* a) {
    static auto fn = (const char**( *)(uint32_t*))GetProcAddress(s_glfw, "glfwGetRequiredInstanceExtensions");
    return fn(a);
}
VkResult glfwCreateWindowSurface(VkInstance a, GLFWwindow* b, const VkAllocationCallbacks* c, VkSurfaceKHR* d) {
    static auto fn = (VkResult(*)(VkInstance,GLFWwindow*,const VkAllocationCallbacks*,VkSurfaceKHR*))GetProcAddress(s_glfw, "glfwCreateWindowSurface");
    return fn(a, b, c, d);
}
}
#pragma GCC diagnostic pop

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;
constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

struct Vertex
{
    glm::vec3 pos;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription b{};
        b.binding = 0;
        b.stride = sizeof(Vertex);
        b.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return b;
    }
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> a{};
        a[0].binding = 0;
        a[0].location = 0;
        a[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        a[0].offset = offsetof(Vertex, pos);
        a[1].binding = 0;
        a[1].location = 1;
        a[1].format = VK_FORMAT_R32G32_SFLOAT;
        a[1].offset = offsetof(Vertex, texCoord);
        return a;
    }
};

struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

// Push constants for tile drawing
struct TilePushConstants
{
    uint32_t waterOverlay; // 0 or 1
};

class GlobeApp
{
public:
    void setBasePath(const std::string &path) { basePath = path; }

    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow *window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    // Sphere mesh
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    VkBuffer vertexBuffer = VK_NULL_HANDLE, indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE, indexBufferMemory = VK_NULL_HANDLE;

    // Height tiles
    static constexpr int TILES_COLS = 18, TILES_ROWS = 9, TOTAL_TILES = 162;
    std::array<VkImage, TOTAL_TILES> tileImages;
    std::array<VkDeviceMemory, TOTAL_TILES> tileImageMemories;
    std::array<VkImageView, TOTAL_TILES> tileImageViews;
    std::array<VkSampler, TOTAL_TILES> tileSamplers;

    // Colour ramp (256x1 RGBA)
    VkImage colorRampImage = VK_NULL_HANDLE;
    VkDeviceMemory colorRampImageMemory = VK_NULL_HANDLE;
    VkImageView colorRampImageView = VK_NULL_HANDLE;
    VkSampler colorRampSampler = VK_NULL_HANDLE;

    // Height lookup table (256 floats)
    VkBuffer heightLUTBuffer = VK_NULL_HANDLE;
    VkDeviceMemory heightLUTMemory = VK_NULL_HANDLE;

    // Uniform buffers (camera)
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void *> uniformBuffersMapped;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet;

    // Sync
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;

    // Camera state
    float camDistance = 2.5f;
    float camPitch = 0.0f;
    float camYaw = 0.0f;
    float camRoll = 0.0f;
    glm::vec3 camTarget = glm::vec3(0.0f);
    bool waterOverlay = false;
    glm::vec2 lastMousePos;
    bool rightMouseDown = false, middleMouseDown = false;

    // Base path for assets (set from executable location)
    std::string basePath = ".";

    // Depth buffer
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    // Input callbacks (same as before)
    static void keyCallback(GLFWwindow *w, int key, int, int action, int mods)
    {
        auto *app = reinterpret_cast<GlobeApp *>(glfwGetWindowUserPointer(w));
        if (action == GLFW_PRESS)
        {
            if (key == GLFW_KEY_V)
                app->waterOverlay = !app->waterOverlay;
            if (key == GLFW_KEY_R)
            {
                app->camPitch = app->camYaw = app->camRoll = 0.0f;
                app->camDistance = 2.5f;
            }
            if (key == GLFW_KEY_ESCAPE)
                glfwSetWindowShouldClose(w, GLFW_TRUE);
        }
        if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            float mult = (mods & GLFW_MOD_SHIFT) ? 3.0f : 1.0f;
            float rotSpeed = 0.02f * mult;
            float zoomSpeed = 0.1f * mult;
            if (key == GLFW_KEY_W)
                app->camPitch -= rotSpeed;
            if (key == GLFW_KEY_S)
                app->camPitch += rotSpeed;
            if (key == GLFW_KEY_A)
                app->camYaw -= rotSpeed;
            if (key == GLFW_KEY_D)
                app->camYaw += rotSpeed;
            if (key == GLFW_KEY_Q)
                app->camRoll -= rotSpeed;
            if (key == GLFW_KEY_E)
                app->camRoll += rotSpeed;
            if (key == GLFW_KEY_Z)
                app->camDistance -= zoomSpeed;
            if (key == GLFW_KEY_X)
                app->camDistance += zoomSpeed;
            app->camPitch = glm::clamp(app->camPitch, -glm::half_pi<float>(), glm::half_pi<float>());
            app->camDistance = glm::clamp(app->camDistance, 1.2f, 10.0f);
        }
    }
    static void mouseButtonCallback(GLFWwindow *w, int button, int action, int)
    {
        auto *app = reinterpret_cast<GlobeApp *>(glfwGetWindowUserPointer(w));
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
            app->rightMouseDown = (action == GLFW_PRESS);
        if (button == GLFW_MOUSE_BUTTON_MIDDLE)
            app->middleMouseDown = (action == GLFW_PRESS);
        double x, y;
        glfwGetCursorPos(w, &x, &y);
        app->lastMousePos = glm::vec2(x, y);
    }
    static void cursorPosCallback(GLFWwindow *w, double xpos, double ypos)
    {
        auto *app = reinterpret_cast<GlobeApp *>(glfwGetWindowUserPointer(w));
        glm::vec2 newPos(xpos, ypos);
        glm::vec2 delta = newPos - app->lastMousePos;
        app->lastMousePos = newPos;
        if (app->rightMouseDown)
        {
            if (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
                app->camRoll += delta.x * 0.005f;
            else
            {
                app->camYaw -= delta.x * 0.005f;
                app->camPitch += delta.y * 0.005f;
                app->camPitch = glm::clamp(app->camPitch, -glm::half_pi<float>(), glm::half_pi<float>());
            }
        }
    }
    static void scrollCallback(GLFWwindow *w, double, double yoffset)
    {
        auto *app = reinterpret_cast<GlobeApp *>(glfwGetWindowUserPointer(w));
        app->camDistance -= static_cast<float>(yoffset) * 0.2f;
        app->camDistance = glm::clamp(app->camDistance, 1.2f, 10.0f);
    }

    // ---------- Vulkan helpers (same as before) ----------
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        throw std::runtime_error("failed to find suitable memory type!");
    }

    std::vector<char> readFile(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error("failed to open file: " + filename);
        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
        return buffer;
    }

    VkShaderModule createShaderModule(const std::vector<char> &code)
    {
        VkShaderModuleCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = code.size();
        ci.pCode = reinterpret_cast<const uint32_t *>(code.data());
        VkShaderModule mod;
        if (vkCreateShaderModule(device, &ci, nullptr, &mod) != VK_SUCCESS)
            throw std::runtime_error("failed to create shader module!");
        return mod;
    }

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer &buffer, VkDeviceMemory &bufferMemory)
    {
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = size;
        bi.usage = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bi, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("failed to create buffer!");
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, buffer, &memReqs);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = memReqs.size;
        ai.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);
        if (vkAllocateMemory(device, &ai, nullptr, &bufferMemory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate buffer memory!");
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size)
    {
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandPool = commandPool;
        ai.commandBufferCount = 1;
        VkCommandBuffer cmdbuf;
        vkAllocateCommandBuffers(device, &ai, &cmdbuf);
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdbuf, &bi);
        VkBufferCopy region{};
        region.size = size;
        vkCmdCopyBuffer(cmdbuf, src, dst, 1, &region);
        vkEndCommandBuffer(cmdbuf);
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmdbuf;
        vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &cmdbuf);
    }

    void createImage(uint32_t w, uint32_t h, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                     VkImage &image, VkDeviceMemory &memory)
    {
        VkImageCreateInfo ii{};
        ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.extent = {w, h, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.format = format;
        ii.tiling = tiling;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ii.usage = usage;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device, &ii, nullptr, &image) != VK_SUCCESS)
            throw std::runtime_error("failed to create image!");
        VkMemoryRequirements mr;
        vkGetImageMemoryRequirements(device, image, &mr);
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = mr.size;
        ai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits, props);
        if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate image memory!");
        vkBindImageMemory(device, image, memory, 0);
    }

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT)
    {
        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = image;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = format;
        vi.subresourceRange.aspectMask = aspectFlags;
        vi.subresourceRange.baseMipLevel = 0;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.baseArrayLayer = 0;
        vi.subresourceRange.layerCount = 1;
        VkImageView iv;
        if (vkCreateImageView(device, &vi, nullptr, &iv) != VK_SUCCESS)
            throw std::runtime_error("failed to create image view!");
        return iv;
    }

    void transitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandPool = commandPool;
        ai.commandBufferCount = 1;
        VkCommandBuffer cmdbuf;
        vkAllocateCommandBuffers(device, &ai, &cmdbuf);
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdbuf, &bi);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage, dstStage;
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }
        vkCmdPipelineBarrier(cmdbuf, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        vkEndCommandBuffer(cmdbuf);

        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmdbuf;
        vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &cmdbuf);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t w, uint32_t h)
    {
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandPool = commandPool;
        ai.commandBufferCount = 1;
        VkCommandBuffer cmdbuf;
        vkAllocateCommandBuffers(device, &ai, &cmdbuf);
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdbuf, &bi);
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {w, h, 1};
        vkCmdCopyBufferToImage(cmdbuf, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        vkEndCommandBuffer(cmdbuf);
        VkSubmitInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cmdbuf;
        vkQueueSubmit(graphicsQueue, 1, &si, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &cmdbuf);
    }

    VkSampler createSampler()
    {
        VkSamplerCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        si.magFilter = VK_FILTER_LINEAR;
        si.minFilter = VK_FILTER_LINEAR;
        si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        si.anisotropyEnable = VK_FALSE;
        si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        si.unnormalizedCoordinates = VK_FALSE;
        si.compareEnable = VK_FALSE;
        si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        si.mipLodBias = 0.0f;
        si.minLod = 0.0f;
        si.maxLod = 0.0f;
        VkSampler sampler;
        if (vkCreateSampler(device, &si, nullptr, &sampler) != VK_SUCCESS)
            throw std::runtime_error("failed to create sampler!");
        return sampler;
    }

    // ---------- Initialization (same as before, but no validation layers) ----------
    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "GlobeVulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetCursorPosCallback(window, cursorPosCallback);
        glfwSetScrollCallback(window, scrollCallback);
    }

    void initVulkan()
    {
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createDepthResources();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        prepareSphere();
        loadTiles();
        createColorRamp();
        createHeightLUT();
        createUniformBuffers();
        createDescriptorSet();
        createCommandBuffers();
        createSyncObjects();
    }

    void createInstance()
    {
        VkApplicationInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        ai.pApplicationName = "GlobeVulkan";
        ai.applicationVersion = (1u << 22) | (0u << 12) | 0u;
        ai.engineVersion = (1u << 22) | (0u << 12) | 0u;
        ai.apiVersion = (0u << 29) | (1u << 22) | (0u << 12) | 0u;

        uint32_t glfwCnt = 0;
        const char **glfwExts = glfwGetRequiredInstanceExtensions(&glfwCnt);
        std::vector<const char *> exts(glfwExts, glfwExts + glfwCnt);

        VkInstanceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo = &ai;
        ci.enabledExtensionCount = static_cast<uint32_t>(exts.size());
        ci.ppEnabledExtensionNames = exts.data();
        if (vkCreateInstance(&ci, nullptr, &instance) != VK_SUCCESS)
            throw std::runtime_error("failed to create instance!");
    }

    void createSurface()
    {
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
            throw std::runtime_error("failed to create surface!");
    }

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphics, present;
        bool isComplete() { return graphics && present; }
    };
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev)
    {
        QueueFamilyIndices idx;
        uint32_t cnt;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &cnt, nullptr);
        std::vector<VkQueueFamilyProperties> qfs(cnt);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &cnt, qfs.data());
        for (uint32_t i = 0; i < cnt; ++i)
        {
            if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                idx.graphics = i;
            VkBool32 present = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &present);
            if (present)
                idx.present = i;
            if (idx.isComplete())
                break;
        }
        return idx;
    }

    struct SwapChainSupport
    {
        VkSurfaceCapabilitiesKHR caps;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> modes;
    };
    SwapChainSupport querySwapChainSupport(VkPhysicalDevice dev)
    {
        SwapChainSupport s;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface, &s.caps);
        uint32_t cnt;
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &cnt, nullptr);
        if (cnt)
        {
            s.formats.resize(cnt);
            vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &cnt, s.formats.data());
        }
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &cnt, nullptr);
        if (cnt)
        {
            s.modes.resize(cnt);
            vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &cnt, s.modes.data());
        }
        return s;
    }

    void pickPhysicalDevice()
    {
        uint32_t cnt;
        vkEnumeratePhysicalDevices(instance, &cnt, nullptr);
        if (!cnt)
            throw std::runtime_error("no Vulkan GPUs");
        std::vector<VkPhysicalDevice> devs(cnt);
        vkEnumeratePhysicalDevices(instance, &cnt, devs.data());
        for (auto &d : devs)
        {
            if (isDeviceSuitable(d))
            {
                physicalDevice = d;
                break;
            }
        }
        if (!physicalDevice)
            throw std::runtime_error("no suitable GPU");
    }

    bool isDeviceSuitable(VkPhysicalDevice dev)
    {
        QueueFamilyIndices idx = findQueueFamilies(dev);
        bool extOk = checkDeviceExtensionSupport(dev);
        bool swapOk = false;
        if (extOk)
        {
            auto ss = querySwapChainSupport(dev);
            swapOk = !ss.formats.empty() && !ss.modes.empty();
        }
        return idx.isComplete() && extOk && swapOk;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice dev)
    {
        uint32_t cnt;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &cnt, nullptr);
        std::vector<VkExtensionProperties> available(cnt);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &cnt, available.data());
        std::set<std::string> required = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        for (auto &e : available)
            required.erase(e.extensionName);
        return required.empty();
    }

    void createLogicalDevice()
    {
        auto idx = findQueueFamilies(physicalDevice);
        std::set<uint32_t> uniq = {idx.graphics.value(), idx.present.value()};
        std::vector<VkDeviceQueueCreateInfo> qcis;
        float prio = 1.0f;
        for (uint32_t fam : uniq)
        {
            VkDeviceQueueCreateInfo qi{};
            qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qi.queueFamilyIndex = fam;
            qi.queueCount = 1;
            qi.pQueuePriorities = &prio;
            qcis.push_back(qi);
        }
        VkPhysicalDeviceFeatures feats{};
        VkDeviceCreateInfo dci{};
        dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
        dci.pQueueCreateInfos = qcis.data();
        dci.pEnabledFeatures = &feats;
        const char *swapExt = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        dci.enabledExtensionCount = 1;
        dci.ppEnabledExtensionNames = &swapExt;
        // no device layers (must be 0)
        dci.enabledLayerCount = 0;
        if (vkCreateDevice(physicalDevice, &dci, nullptr, &device) != VK_SUCCESS)
            throw std::runtime_error("failed to create device!");
        vkGetDeviceQueue(device, idx.graphics.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, idx.present.value(), 0, &presentQueue);
    }

    void createSwapChain()
    {
        auto ss = querySwapChainSupport(physicalDevice);
        VkSurfaceFormatKHR fmt = chooseSwapSurfaceFormat(ss.formats);
        VkPresentModeKHR mode = chooseSwapPresentMode(ss.modes);
        VkExtent2D ext = chooseSwapExtent(ss.caps);
        uint32_t imgCount = ss.caps.minImageCount + 1;
        if (ss.caps.maxImageCount > 0 && imgCount > ss.caps.maxImageCount)
            imgCount = ss.caps.maxImageCount;

        VkSwapchainCreateInfoKHR sci{};
        sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        sci.surface = surface;
        sci.minImageCount = imgCount;
        sci.imageFormat = fmt.format;
        sci.imageColorSpace = fmt.colorSpace;
        sci.imageExtent = ext;
        sci.imageArrayLayers = 1;
        sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        auto idx = findQueueFamilies(physicalDevice);
        uint32_t fams[] = {idx.graphics.value(), idx.present.value()};
        if (idx.graphics != idx.present)
        {
            sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            sci.queueFamilyIndexCount = 2;
            sci.pQueueFamilyIndices = fams;
        }
        else
            sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        sci.preTransform = ss.caps.currentTransform;
        sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sci.presentMode = mode;
        sci.clipped = VK_TRUE;
        if (vkCreateSwapchainKHR(device, &sci, nullptr, &swapChain) != VK_SUCCESS)
            throw std::runtime_error("failed to create swap chain!");

        vkGetSwapchainImagesKHR(device, swapChain, &imgCount, nullptr);
        swapChainImages.resize(imgCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imgCount, swapChainImages.data());
        swapChainImageFormat = fmt.format;
        swapChainExtent = ext;
    }

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &avail)
    {
        for (auto &f : avail)
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return f;
        return avail[0];
    }
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &avail)
    {
        for (auto &m : avail)
            if (m == VK_PRESENT_MODE_MAILBOX_KHR)
                return m;
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &caps)
    {
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max())
            return caps.currentExtent;
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    }

    void createImageViews()
    {
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); ++i)
            swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat);
    }

    void createDepthResources()
    {
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
        createImage(swapChainExtent.width, swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    depthImage, depthImageMemory);
        depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    void createRenderPass()
    {
        VkAttachmentDescription colorAtt{}, depthAtt{};
        colorAtt.format = swapChainImageFormat;
        colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAtt.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        depthAtt.format = VK_FORMAT_D32_SFLOAT;
        depthAtt.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;

        std::array<VkAttachmentDescription, 2> atts = {colorAtt, depthAtt};
        VkRenderPassCreateInfo rpi{};
        rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpi.attachmentCount = static_cast<uint32_t>(atts.size());
        rpi.pAttachments = atts.data();
        rpi.subpassCount = 1;
        rpi.pSubpasses = &subpass;
        if (vkCreateRenderPass(device, &rpi, nullptr, &renderPass) != VK_SUCCESS)
            throw std::runtime_error("failed to create render pass!");
    }

    void createDescriptorSetLayout()
    {
        std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
        // tile textures array (162)
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].descriptorCount = TOTAL_TILES;
        bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // camera UBO
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // height LUT
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // colour ramp
        bindings[3].binding = 3;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[3].descriptorCount = 1;
        bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo li{};
        li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        li.bindingCount = static_cast<uint32_t>(bindings.size());
        li.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(device, &li, nullptr, &descriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor set layout!");
    }

    void createGraphicsPipeline()
    {
        auto vertCode = readFile(basePath + "/Shaders/vert.spv");
        auto fragCode = readFile(basePath + "/Shaders/frag.spv");
        VkShaderModule vertMod = createShaderModule(vertCode);
        VkShaderModule fragMod = createShaderModule(fragCode);

        VkPipelineShaderStageCreateInfo vertStage{};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertMod;
        vertStage.pName = "main";
        VkPipelineShaderStageCreateInfo fragStage{};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragMod;
        fragStage.pName = "main";
        VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

        auto bindingDesc = Vertex::getBindingDescription();
        auto attrDescs = Vertex::getAttributeDescriptions();
        VkPipelineVertexInputStateCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &bindingDesc;
        vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
        vi.pVertexAttributeDescriptions = attrDescs.data();

        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        ia.primitiveRestartEnable = VK_FALSE;

        VkViewport vp{0, 0, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0, 1};
        VkRect2D sc{{0, 0}, swapChainExtent};
        VkPipelineViewportStateCreateInfo vs{};
        vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vs.viewportCount = 1;
        vs.pViewports = &vp;
        vs.scissorCount = 1;
        vs.pScissors = &sc;

        VkPipelineRasterizationStateCreateInfo rs{};
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.lineWidth = 1.0f;
        rs.cullMode = VK_CULL_MODE_NONE;
        rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo ds{};
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.depthTestEnable = VK_TRUE;
        ds.depthWriteEnable = VK_TRUE;
        ds.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState cba{};
        cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        cba.blendEnable = VK_FALSE;
        VkPipelineColorBlendStateCreateInfo cb{};
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.attachmentCount = 1;
        cb.pAttachments = &cba;

        VkPushConstantRange pcr{};
        pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pcr.offset = 0;
        pcr.size = sizeof(TilePushConstants);

        VkPipelineLayoutCreateInfo pl{};
        pl.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pl.setLayoutCount = 1;
        pl.pSetLayouts = &descriptorSetLayout;
        pl.pushConstantRangeCount = 1;
        pl.pPushConstantRanges = &pcr;
        if (vkCreatePipelineLayout(device, &pl, nullptr, &pipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create pipeline layout!");

        VkGraphicsPipelineCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pi.stageCount = 2;
        pi.pStages = stages;
        pi.pVertexInputState = &vi;
        pi.pInputAssemblyState = &ia;
        pi.pViewportState = &vs;
        pi.pRasterizationState = &rs;
        pi.pMultisampleState = &ms;
        pi.pDepthStencilState = &ds;
        pi.pColorBlendState = &cb;
        pi.layout = pipelineLayout;
        pi.renderPass = renderPass;
        pi.subpass = 0;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pi, nullptr, &graphicsPipeline) != VK_SUCCESS)
            throw std::runtime_error("failed to create pipeline!");

        vkDestroyShaderModule(device, vertMod, nullptr);
        vkDestroyShaderModule(device, fragMod, nullptr);
    }

    void createFramebuffers()
    {
        swapChainFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); ++i)
        {
            VkImageView attachments[] = {swapChainImageViews[i], depthImageView};
            VkFramebufferCreateInfo fi{};
            fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fi.renderPass = renderPass;
            fi.attachmentCount = 2;
            fi.pAttachments = attachments;
            fi.width = swapChainExtent.width;
            fi.height = swapChainExtent.height;
            fi.layers = 1;
            if (vkCreateFramebuffer(device, &fi, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create framebuffer!");
        }
    }

    void createCommandPool()
    {
        auto idx = findQueueFamilies(physicalDevice);
        VkCommandPoolCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pi.queueFamilyIndex = idx.graphics.value();
        if (vkCreateCommandPool(device, &pi, nullptr, &commandPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create command pool!");
    }

    // ---------- Sphere mesh (same as before) ----------
    void prepareSphere()
    {
        constexpr size_t lonSegs = 256, latSegs = 128;
        vertices.clear();
        indices.clear();
        for (size_t j = 0; j <= latSegs; ++j)
        {
            float theta = static_cast<float>(j) * glm::pi<float>() / static_cast<float>(latSegs);
            float y = glm::cos(theta), sinTheta = glm::sin(theta);
            for (size_t i = 0; i <= lonSegs; ++i)
            {
                float phi = static_cast<float>(i) * 2.0f * glm::pi<float>() / static_cast<float>(lonSegs);
                float x = glm::cos(phi) * sinTheta;
                float z = glm::sin(phi) * sinTheta;
                vertices.push_back({{x, y, z}, {static_cast<float>(i) / static_cast<float>(lonSegs), static_cast<float>(j) / static_cast<float>(latSegs)}});
            }
        }
        for (size_t j = 0; j < latSegs; ++j)
            for (size_t i = 0; i < lonSegs; ++i)
            {
                uint32_t first = static_cast<uint32_t>(j * (lonSegs + 1) + i);
                uint32_t second = first + static_cast<uint32_t>(lonSegs + 1);
                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);
                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        // vertex buffer
        VkDeviceSize vbSize = sizeof(Vertex) * vertices.size();
        VkBuffer staging;
        VkDeviceMemory stagingMem;
        createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
        void *data;
        vkMapMemory(device, stagingMem, 0, vbSize, 0, &data);
        memcpy(data, vertices.data(), vbSize);
        vkUnmapMemory(device, stagingMem);
        createBuffer(vbSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
        copyBuffer(staging, vertexBuffer, vbSize);
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
        // index buffer
        VkDeviceSize ibSize = sizeof(uint32_t) * indices.size();
        createBuffer(ibSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
        vkMapMemory(device, stagingMem, 0, ibSize, 0, &data);
        memcpy(data, indices.data(), ibSize);
        vkUnmapMemory(device, stagingMem);
        createBuffer(ibSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);
        copyBuffer(staging, indexBuffer, ibSize);
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
    }

    // ---------- Load 162 height tiles ----------
    void loadTiles()
    {
        const char colNames[] = "ABCDEFGHIJKLMNOPQR";
        // Precompute tile dimensions (same as DivideHeightMap)
        int colWidths[TILES_COLS], rowHeights[TILES_ROWS];
        for (int c = 0; c < TILES_COLS; ++c)
            colWidths[c] = (c < 16) ? 3641 : 3640;
        for (int r = 0; r < TILES_ROWS; ++r)
            rowHeights[r] = (r < 8) ? 3641 : 3640;

        for (int row = 0; row < TILES_ROWS; ++row)
        {
            for (int col = 0; col < TILES_COLS; ++col)
            {
                int tileIdx = row * TILES_COLS + col;
                size_t uTileIdx = static_cast<size_t>(tileIdx);
                // Build filename: XY_AB.bin
                std::string fname = basePath + "/Output/DividedHeightMap/";
                fname += colNames[col];
                fname += std::to_string(row);
                fname += "_";
                int oldX = (col < 9) ? (col - 9) : (col - 8);
                int oldY = 4 - row;
                if (oldX == -1)
                    fname += "[-1]";
                else if (oldX == 1)
                    fname += "[+1]";
                else
                    fname += std::to_string(oldX);
                if (oldY == 0)
                    fname += "[0]";
                else
                    fname += std::to_string(oldY);
                fname += ".bin";

                std::ifstream file(fname, std::ios::binary | std::ios::ate);
                if (!file.is_open())
                    throw std::runtime_error("Cannot open " + fname);
                auto fsize = file.tellg();
                size_t size = (fsize > 0) ? static_cast<size_t>(fsize) : 0;
                file.seekg(0);
                std::vector<uint8_t> pixels(size);
                file.read(reinterpret_cast<char *>(pixels.data()), static_cast<std::streamsize>(size));
                file.close();

                int w = colWidths[col];
                int h = rowHeights[row];

                // Create staging buffer and upload
                VkDeviceSize imageSize = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h);
                VkBuffer staging;
                VkDeviceMemory stagingMem;
                createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
                void *data;
                vkMapMemory(device, stagingMem, 0, imageSize, 0, &data);
                memcpy(data, pixels.data(), imageSize);
                vkUnmapMemory(device, stagingMem);

                uint32_t uw = static_cast<uint32_t>(w);
                uint32_t uh = static_cast<uint32_t>(h);
                createImage(uw, uh, VK_FORMAT_R8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            tileImages[uTileIdx], tileImageMemories[uTileIdx]);
                transitionImageLayout(tileImages[uTileIdx], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                copyBufferToImage(staging, tileImages[uTileIdx], uw, uh);
                transitionImageLayout(tileImages[uTileIdx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                vkDestroyBuffer(device, staging, nullptr);
                vkFreeMemory(device, stagingMem, nullptr);

                tileImageViews[uTileIdx] = createImageView(tileImages[uTileIdx], VK_FORMAT_R8_UNORM);
                tileSamplers[uTileIdx] = createSampler();
            }
        }
    }

    // ---------- Colour ramp (same) ----------
    void createColorRamp()
    {
        std::array<uint8_t, 256 * 4> ramp;
        for (size_t i = 0; i < 256; ++i)
        {
            if (i == 0)
            {
                ramp[4 * i + 0] = 0;
                ramp[4 * i + 1] = 0;
                ramp[4 * i + 2] = 255;
                ramp[4 * i + 3] = 255;
                continue;
            }
            float t = static_cast<float>(i - 1) / 254.0f;
            struct Stop
            {
                float pos;
                uint8_t r, g, b;
            };
            constexpr Stop stops[] = {
                {0.00f, 0, 200, 100}, {0.08f, 0, 100, 0}, {0.16f, 128, 128, 0}, {0.25f, 0, 255, 0}, {0.33f, 127, 255, 0}, {0.41f, 255, 255, 0}, {0.50f, 255, 165, 0}, {0.58f, 255, 0, 0}, {0.66f, 255, 192, 203}, {0.75f, 128, 0, 128}, {0.83f, 0, 0, 0}, {0.91f, 128, 128, 128}, {1.00f, 255, 255, 255}};
            int idx = 0;
            while (idx < 12 && t > stops[idx + 1].pos)
                ++idx;
            float seg = stops[idx + 1].pos - stops[idx].pos;
            float local = std::clamp((t - stops[idx].pos) / seg, 0.0f, 1.0f);
            ramp[4 * i + 0] = uint8_t(stops[idx].r + (stops[idx + 1].r - stops[idx].r) * local);
            ramp[4 * i + 1] = uint8_t(stops[idx].g + (stops[idx + 1].g - stops[idx].g) * local);
            ramp[4 * i + 2] = uint8_t(stops[idx].b + (stops[idx + 1].b - stops[idx].b) * local);
            ramp[4 * i + 3] = 255;
        }
        VkDeviceSize size = ramp.size();
        VkBuffer staging;
        VkDeviceMemory stagingMem;
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMem);
        void *data;
        vkMapMemory(device, stagingMem, 0, size, 0, &data);
        memcpy(data, ramp.data(), size);
        vkUnmapMemory(device, stagingMem);
        createImage(256, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorRampImage, colorRampImageMemory);
        transitionImageLayout(colorRampImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(staging, colorRampImage, 256, 1);
        transitionImageLayout(colorRampImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkDestroyBuffer(device, staging, nullptr);
        vkFreeMemory(device, stagingMem, nullptr);
        colorRampImageView = createImageView(colorRampImage, VK_FORMAT_R8G8B8A8_UNORM);
        colorRampSampler = createSampler();
    }

    // ---------- Height LUT (same) ----------
    void createHeightLUT()
    {
        // ... (identical to previous version) ...
        constexpr std::array<double, 10> subsea = {-400, -300, -250, -200, -150, -100, -50, -20, -7.8, -1.0 / 32768};
        constexpr std::array<double, 245> positive = {
            7.8, 15.5, 25.8, 36.2, 46.5, 54.3, 62.0, 69.8, 77.5, 85.3,
            93.0, 100.8, 108.5, 113.7, 118.8, 124.0, 129.2, 134.3, 139.5, 144.7,
            149.8, 155.0, 160.2, 165.3, 170.5, 174.9, 179.4, 183.8, 188.2, 192.6,
            197.1, 201.5, 205.9, 210.4, 214.8, 219.2, 223.6, 228.1, 232.5, 236.9,
            241.4, 245.8, 250.2, 254.6, 259.1, 263.5, 267.9, 272.4, 276.8, 281.2,
            285.6, 290.1, 294.5, 298.9, 303.4, 307.8, 312.2, 316.6, 321.1, 325.5,
            331.7, 337.9, 344.1, 350.3, 356.5, 362.7, 368.9, 375.1, 381.3, 387.5,
            393.7, 399.9, 406.1, 412.3, 418.5, 426.3, 434.0, 441.8, 449.5, 459.8,
            470.2, 480.5, 490.8, 501.2, 511.5, 521.8, 532.2, 542.5, 552.8, 563.2,
            573.5, 589.0, 604.5, 620.0, 635.5, 651.0, 666.5, 682.0, 697.5, 713.0,
            728.5, 744.0, 759.5, 790.5, 806.0, 821.5, 852.5, 868.0, 883.5, 914.5,
            945.5, 961.0, 976.5, 1007.5, 1038.5, 1069.5, 1085.0, 1100.5, 1131.5, 1162.5,
            1193.5, 1224.5, 1255.5, 1271.0, 1286.5, 1317.5, 1348.5, 1379.5, 1410.5, 1441.5,
            1472.5, 1503.5, 1534.5, 1565.5, 1596.5, 1627.5, 1658.5, 1689.5, 1720.5, 1751.5,
            1782.5, 1813.5, 1844.5, 1875.5, 1906.5, 1937.5, 1968.5, 1999.5, 2030.5, 2061.5,
            2108.0, 2170.0, 2232.0, 2294.0, 2356.0, 2418.0, 2480.0, 2542.0, 2604.0, 2666.0,
            2728.0, 2790.0, 2852.0, 2914.0, 2976.0, 3038.0, 3100.0, 3162.0, 3224.0, 3286.0,
            3348.0, 3410.0, 3472.0, 3534.0, 3596.0, 3658.0, 3720.0, 3782.0, 3844.0, 3906.0,
            3968.0, 4030.0, 4092.0, 4154.0, 4216.0, 4278.0, 4340.0, 4402.0, 4464.0, 4526.0,
            4588.0, 4650.0, 4712.0, 4774.0, 4836.0, 4898.0, 4960.0, 5022.0, 5084.0, 5146.0,
            5208.0, 5270.0, 5332.0, 5394.0, 5456.0, 5518.0, 5580.0, 5642.0, 5704.0, 5766.0,
            5828.0, 5890.0, 5952.0, 6014.0, 6076.0, 6138.0, 6200.0, 6262.0, 6324.0, 6386.0,
            6448.0, 6510.0, 6572.0, 6634.0, 6696.0, 6758.0, 6820.0, 6882.0, 6944.0, 7006.0,
            7068.0, 7130.0, 7192.0, 7254.0, 7316.0, 7386.0, 7466.0, 7556.0, 7666.0, 7796.0,
            7946.0, 8126.0, 8336.0, 8576.0, 8846.0};
        std::array<float, 256> heights;
        heights[0] = 0.0f;
        for (size_t i = 1; i <= 10; ++i)
            heights[i] = static_cast<float>(subsea[i - 1]);
        for (size_t i = 11; i <= 255; ++i)
            heights[i] = static_cast<float>(positive[i - 11]);
        VkDeviceSize size = sizeof(heights);
        createBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, heightLUTBuffer, heightLUTMemory);
        void *data;
        vkMapMemory(device, heightLUTMemory, 0, size, 0, &data);
        memcpy(data, heights.data(), size);
        vkUnmapMemory(device, heightLUTMemory);
    }

    // ---------- Uniform buffers (camera) ----------
    void createUniformBuffers()
    {
        VkDeviceSize size = sizeof(UniformBufferObject);
        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            createBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
            vkMapMemory(device, uniformBuffersMemory[i], 0, size, 0, &uniformBuffersMapped[i]);
        }
    }

    // ---------- Descriptor set (array of 162 tile samplers) ----------
    void createDescriptorSet()
    {
        std::array<VkDescriptorPoolSize, 3> poolSizes = {{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, TOTAL_TILES + 1}, // tiles + ramp
                                                           {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                                           {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}}};
        VkDescriptorPoolCreateInfo pi{};
        pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pi.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        pi.pPoolSizes = poolSizes.data();
        pi.maxSets = 1;
        if (vkCreateDescriptorPool(device, &pi, nullptr, &descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool!");

        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = descriptorPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &descriptorSetLayout;
        if (vkAllocateDescriptorSets(device, &ai, &descriptorSet) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor set!");

        // Prepare array of image info for all tiles
        std::vector<VkDescriptorImageInfo> tileInfos(TOTAL_TILES);
        for (size_t i = 0; i < TOTAL_TILES; ++i)
        {
            tileInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            tileInfos[i].imageView = tileImageViews[i];
            tileInfos[i].sampler = tileSamplers[i];
        }
        VkWriteDescriptorSet tileWrite{};
        tileWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        tileWrite.dstSet = descriptorSet;
        tileWrite.dstBinding = 0;
        tileWrite.dstArrayElement = 0;
        tileWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        tileWrite.descriptorCount = TOTAL_TILES;
        tileWrite.pImageInfo = tileInfos.data();

        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = uniformBuffers[0];
        uboInfo.offset = 0;
        uboInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet uboWrite{};
        uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet = descriptorSet;
        uboWrite.dstBinding = 1;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &uboInfo;

        VkDescriptorBufferInfo lutInfo{};
        lutInfo.buffer = heightLUTBuffer;
        lutInfo.offset = 0;
        lutInfo.range = sizeof(float) * 256;
        VkWriteDescriptorSet lutWrite{};
        lutWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lutWrite.dstSet = descriptorSet;
        lutWrite.dstBinding = 2;
        lutWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        lutWrite.descriptorCount = 1;
        lutWrite.pBufferInfo = &lutInfo;

        VkDescriptorImageInfo rampInfo{};
        rampInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        rampInfo.imageView = colorRampImageView;
        rampInfo.sampler = colorRampSampler;
        VkWriteDescriptorSet rampWrite{};
        rampWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        rampWrite.dstSet = descriptorSet;
        rampWrite.dstBinding = 3;
        rampWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        rampWrite.descriptorCount = 1;
        rampWrite.pImageInfo = &rampInfo;

        std::array<VkWriteDescriptorSet, 4> writes = {tileWrite, uboWrite, lutWrite, rampWrite};
        // Ensure pNext is null (avoid the validation error)
        for (auto &w : writes)
            w.pNext = nullptr;
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    void createCommandBuffers()
    {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = commandPool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        vkAllocateCommandBuffers(device, &ai, commandBuffers.data());
    }

    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex)
    {
        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &bi);

        VkRenderPassBeginInfo rpi{};
        rpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpi.renderPass = renderPass;
        rpi.framebuffer = swapChainFramebuffers[imageIndex];
        rpi.renderArea = {{0, 0}, swapChainExtent};

        VkClearValue colorClear{}, depthClear{};
        colorClear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        depthClear.depthStencil = {1.0f, 0};
        std::array<VkClearValue, 2> clears = {colorClear, depthClear};
        rpi.clearValueCount = 2;
        rpi.pClearValues = clears.data();

        vkCmdBeginRenderPass(cmd, &rpi, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, offsets);
        vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        // Single draw call — tile index is computed from UV in the vertex shader
        TilePushConstants pc{};
        pc.waterOverlay = waterOverlay ? 1 : 0;

        vkCmdPushConstants(cmd, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(TilePushConstants), &pc);
        vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);
    }

    void createSyncObjects()
    {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        VkSemaphoreCreateInfo si{};
        si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkCreateSemaphore(device, &si, nullptr, &imageAvailableSemaphores[i]);
            vkCreateSemaphore(device, &si, nullptr, &renderFinishedSemaphores[i]);
            vkCreateFence(device, &fi, nullptr, &inFlightFences[i]);
        }
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            updateCamera();
            drawFrame();
        }
        vkDeviceWaitIdle(device);
    }

    void updateCamera()
    {
        float cosPitch = glm::cos(camPitch), sinPitch = glm::sin(camPitch);
        float cosYaw = glm::cos(camYaw), sinYaw = glm::sin(camYaw);
        glm::vec3 camPos;
        camPos.x = camDistance * cosPitch * sinYaw;
        camPos.y = camDistance * sinPitch;
        camPos.z = camDistance * cosPitch * cosYaw;

        glm::mat4 view = glm::lookAt(camPos, camTarget, glm::vec3(0, 1, 0));
        view = glm::rotate(view, camRoll, glm::vec3(0, 0, 1));
        float aspect = static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100000.0f);
        proj[1][1] *= -1;
        proj[2][2] = proj[2][2] * 0.5f + proj[3][2] * 0.5f;
        proj[2][3] = proj[2][3] * 0.5f + proj[3][3] * 0.5f;

        UniformBufferObject ubo{};
        ubo.model = glm::mat4(1.0f);
        ubo.view = view;
        ubo.proj = proj;
        memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
    }

    void drawFrame()
    {
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = uniformBuffers[currentFrame];
        uboInfo.offset = 0;
        uboInfo.range = sizeof(UniformBufferObject);
        VkWriteDescriptorSet uboWrite{};
        uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet = descriptorSet;
        uboWrite.dstBinding = 1;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.descriptorCount = 1;
        uboWrite.pBufferInfo = &uboInfo;
        vkUpdateDescriptorSets(device, 1, &uboWrite, 0, nullptr);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
        submit.pWaitDstStageMask = waitStages;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffers[currentFrame];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];
        vkQueueSubmit(graphicsQueue, 1, &submit, inFlightFences[currentFrame]);

        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
        present.swapchainCount = 1;
        present.pSwapchains = &swapChain;
        present.pImageIndices = &imageIndex;
        vkQueuePresentKHR(presentQueue, &present);
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void cleanup()
    {
        vkDeviceWaitIdle(device);
        // Destroy resources (abbreviated)
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main()
{
#ifdef _WIN32
    // Set DLL directory to .dlls/ (absolute path)
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir(exePath);
    exeDir = exeDir.substr(0, exeDir.find_last_of("\\/"));
    std::string dllDir = exeDir + "\\..\\.dlls";
    char absPath[MAX_PATH];
    if (GetFullPathNameA(dllDir.c_str(), MAX_PATH, absPath, nullptr))
        SetDllDirectoryA(absPath);
    loadGlfw();
    if (!s_glfw)
    {
        std::cerr << "Failed to load glfw3.dll from " << dllDir << std::endl;
        return EXIT_FAILURE;
    }
#endif

    GlobeApp app;
#ifdef _WIN32
    app.setBasePath(exeDir + "/..");
#endif
    try
    {
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}