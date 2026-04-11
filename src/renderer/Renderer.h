#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <array>

#include "FrameData.h"

// Forward Declarations
class VulkanContext;
class SwapChain;
class GraphicsPipeline;


class Renderer
{
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    Renderer(const VulkanContext& context);

    // Non Copyable (removing copyable constructors)
    Renderer(const Renderer&) = delete;
    Renderer& operator =(const Renderer&) = delete;

    bool drawFrame(SwapChain& swap_chain, GraphicsPipeline& graphics_pipeline);

private:
    void createCommandPool();
    void createFrameData();

    // private member variables
    const VulkanContext& context_;
    vk::raii::CommandPool commandPool_ = nullptr;
    std::array<FrameData, MAX_FRAMES_IN_FLIGHT> frames_;
    uint32_t currentFrame_ = 0;
    
};