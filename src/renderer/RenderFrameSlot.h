#pragma once

#include <vulkan/vulkan_raii.hpp>

struct RenderFrameSlot
{

    vk::raii::CommandBuffer commandBuffer_ = nullptr;  //Recorded draw commands sent to the GPU (this frame).
    vk::raii::Semaphore imageAvailableSemaphore_ = nullptr;  // GPU waits on before writing, swap chain signals when a presentable image is ready.
    vk::raii::Fence inFlightFence_ = nullptr;  //CPU-side fence, drawFrame() waits on it so CPU doesn't overwrite this slot while the GPU is is still running it.

};
