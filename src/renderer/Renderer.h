#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <array>

#include "RenderFrameSlot.h"

// Forward Declarations
class VulkanContext;
class SwapChain;
class GraphicsPipeline;


class Renderer
{
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    Renderer(VulkanContext& context);

    // Non Copyable (removing copyable constructors)
    Renderer(const Renderer&) = delete;
    Renderer& operator =(const Renderer&) = delete;

    bool drawFrame(SwapChain& swap_chain, GraphicsPipeline& graphics_pipeline);

private:
    void createCommandPool();
    void initializeFrameData();

    // private member variables
    VulkanContext& context_;
    vk::raii::CommandPool commandPool_ = nullptr;
    std::array<RenderFrameSlot, MAX_FRAMES_IN_FLIGHT> renderFrameSlots_;
    uint32_t currentFrame_ = 0;
    
};