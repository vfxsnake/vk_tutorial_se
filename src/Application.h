#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <memory>
#include <cstdint>
#include <chrono>

#include "renderer/buffers/UniformBufferObject.h"
#include "renderer/buffers/Particle.h"


// forward declarations
class VulkanContext;
class SwapChain;
class FrameDescriptorLayout;
class GraphicsPipeline;
class Renderer;
class Mesh;
class TextureDescriptorLayout;
class Texture;
class DepthImage;
class MsaaColorImage;
class ParticleDescriptorLayout;
class ComputePipeline;
class ParticleGraphicsPipeline;

class Application
{
public:
    Application();
    ~Application();

    void run();

private:
    // Initializing functions
    void initWindow();
    void initVulkan();
    void mainLoop();
    void onResize();

    auto computeUniformBufferObject(vk::Extent2D extent, float time_seconds) const -> UniformBufferObject;

    auto generateParticles() const -> std::vector<Particle>;

    // GLFW static callback - retrieves Application* via glfwGetWindowUserPointer
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;
    static constexpr uint32_t PARTICLE_COUNT = 256 * 500;

    std::chrono::high_resolution_clock::time_point startTime_;
    std::chrono::high_resolution_clock::time_point lastFrameTime_;

    // Window - must be declared before Vulkan objects (destroyed last)
    GLFWwindow* window_ = nullptr;
    bool framebufferResized_ = false;

    // Vulkan objects order controls destruction order (reversed)
    std::unique_ptr<VulkanContext> context_;
    std::unique_ptr<SwapChain> swapChain_;
    std::unique_ptr<FrameDescriptorLayout> frameDescriptorLayout_;
    std::unique_ptr<TextureDescriptorLayout> textureDescriptorLayout_;
    std::unique_ptr<GraphicsPipeline> graphicsPipeline_;
    std::unique_ptr<ParticleDescriptorLayout> particleDescriptorLayout_;
    std::unique_ptr<ComputePipeline> computePipeline_;
    std::unique_ptr<ParticleGraphicsPipeline> particleGraphicsPipeline_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<MsaaColorImage> msaaColorImage_;
    std::unique_ptr<DepthImage> depthImage_;
    std::unique_ptr<Texture> texture_;
    std::unique_ptr<Mesh> mesh_;
};