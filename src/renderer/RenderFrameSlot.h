#pragma once

#include <vulkan/vulkan_raii.hpp>
/*
    Per-frame GPU synchronisation resources.
    The renderer keeps MAX_FRAMES_IN_FLIGHT (2 instances of this struct)
    and cycles through them.
    * refer to the Vulkan Tutorial for create Command buffer for the command buffer creation.
    * refer to the vulkan Tutorial as part of the CreteSyncObjects(), for the semaphores and fence creation

*/
struct RenderFrameSlot{

    vk::raii::CommandBuffer commandBuffer_ = nullptr;  //Recorded draw commands sent to the GPU (this frame).
    vk::raii::Semaphore imageAvailableSemaphore_ = nullptr;  // GPU waits on before writing, swap chain signals when a presentable image is ready.
    vk::raii::Semaphore renderFinishedSemaphore_ = nullptr;  // GPU signals when rendering is done. Presenter waits on it before showing the image.
    vk::raii::Fence inFlightFence_ = nullptr;  //CPU-side fence, drawFrame() waits on it so CPU doesn't overwrite this slot while the GPU is is still running it.
};
