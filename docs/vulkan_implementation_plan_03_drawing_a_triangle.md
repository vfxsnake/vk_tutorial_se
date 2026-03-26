# Implementation Plan — Chapter 03: Drawing a Triangle

> **Status:** Agreed — this is the ground truth for all implementation and review sessions.
> **Style:** Enforced per `CLAUDE.md § Project-Wide Agreements`
> **Last updated:** 2026-03-19

---

## Dependency Graph

```
Application
 ├── owns ──► VulkanContext      (instance, debug messenger, surface, physical device, device, queue)
 ├── owns ──► SwapChain          (swap chain, image views)     ──reads──► VulkanContext
 ├── owns ──► GraphicsPipeline   (pipeline layout, pipeline)   ──reads──► VulkanContext
 └── owns ──► Renderer           (command pool, frame data)    ──reads──► VulkanContext

Renderer::drawFrame(SwapChain&, GraphicsPipeline&)
 └── calls ──► GraphicsPipeline::record(commandBuffer, extent, imageView)
```

**Construction order** (inside `Application::initVulkan`):
1. `VulkanContext`  — no Vulkan deps
2. `SwapChain`      — needs `VulkanContext`
3. `GraphicsPipeline` — needs `VulkanContext`, swap chain format
4. `Renderer`       — needs `VulkanContext`

**Implementation order (agreed 2026-03-23):** Build bottom-up, not top-down. Implement and verify each class independently using `main.cpp` as a temporary test harness before building the next layer. `Application` is written last, once all four classes are verified working.

**Destruction order** (reverse of declaration order — RAII):
`Renderer` → `GraphicsPipeline` → `SwapChain` → `VulkanContext`

---

## File List

```
src/
├── main.cpp
├── Application.h
├── Application.cpp
├── core/
│   ├── VulkanContext.h
│   ├── VulkanContext.cpp
│   ├── SwapChain.h
│   └── SwapChain.cpp
├── renderer/
│   ├── FrameData.h
│   ├── GraphicsPipeline.h
│   ├── GraphicsPipeline.cpp
│   ├── Renderer.h
│   └── Renderer.cpp
├── scene/
│   └── .gitkeep
└── utils/
    └── FileUtils.h

shaders/
└── triangle.slang
```

---

## `src/main.cpp`

Entry point only. No logic beyond constructing Application and catching exceptions.

**Responsibilities:** Create `Application`, call `run()`, catch `std::exception`.

---

## `src/Application.h` / `Application.cpp`

**Responsibilities:** Window lifecycle, object ownership, main loop, resize handling.

```cpp
#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <memory>

// Forward declarations — keep includes out of the header
class VulkanContext;
class SwapChain;
class GraphicsPipeline;
class Renderer;

class Application
{
public:
    Application();
    ~Application();

    void run();

private:
    // Initialisation (called in order from run())
    void initWindow();
    void initVulkan();
    void mainLoop();
    void onResize();

    // GLFW static callback — retrieves Application* via glfwGetWindowUserPointer
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    static constexpr uint32_t WIDTH  = 800;
    static constexpr uint32_t HEIGHT = 600;

    // Window — must be declared before Vulkan objects (destroyed last)
    GLFWwindow* window_             = nullptr;
    bool        framebufferResized_ = false;

    // Vulkan objects — declaration order controls destruction order (reverse)
    // VulkanContext must be destroyed last → declared first
    std::unique_ptr<VulkanContext>    context_;
    std::unique_ptr<SwapChain>        swapChain_;
    std::unique_ptr<GraphicsPipeline> pipeline_;
    std::unique_ptr<Renderer>         renderer_;
};
```

**Key implementation notes:**

- `run()` calls `initWindow()` → `initVulkan()` → `mainLoop()`. Wrapped in try/catch for `std::exception`.
- `initVulkan()` constructs all four unique_ptrs in dependency order using `std::make_unique`.
- `mainLoop()` polls events, calls `renderer_->drawFrame(*swapChain_, *pipeline_)`, checks the return value and `framebufferResized_` flag, calls `onResize()` if either is true, calls `context_->getDevice().waitIdle()` after the loop exits.
- `onResize()` calls `context_->getDevice().waitIdle()` then `swapChain_->recreate()`. Pipeline does not need recreation on resize (format is stable, extent is dynamic).
- `framebufferResizeCallback` retrieves `Application*` via `glfwGetWindowUserPointer` and sets `framebufferResized_ = true`.

---

## `src/core/VulkanContext.h` / `VulkanContext.cpp`

**Responsibilities:** Vulkan instance, debug messenger, window surface, physical device selection, logical device, queue.

```cpp
#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <string>

class VulkanContext
{
public:
    explicit VulkanContext(GLFWwindow* window);

    // Non-copyable, non-movable — passed by const ref everywhere
    VulkanContext(const VulkanContext&)            = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&)                 = delete;
    VulkanContext& operator=(VulkanContext&&)      = delete;

    // Accessors
    auto getDevice()           const -> const vk::raii::Device&;
    auto getPhysicalDevice()   const -> const vk::raii::PhysicalDevice&;
    auto getQueue()                  -> vk::raii::Queue&;
    auto getSurface()          const -> const vk::raii::SurfaceKHR&;
    uint32_t getQueueFamilyIndex()   const;

private:
    void createInstance(GLFWwindow* window);
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createLogicalDevice();

    // Device suitability helpers
    bool     isDeviceSuitable(const vk::raii::PhysicalDevice& physical_device) const;
    uint32_t findQueueFamily(const vk::raii::PhysicalDevice& physical_device)  const;
    bool     checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& physical_device) const;

    // Instance creation helpers
    static auto getRequiredInstanceExtensions() -> std::vector<const char*>;
    static bool checkValidationLayerSupport();

    // Debug messenger
    static auto makeDebugMessengerCreateInfo() -> vk::DebugUtilsMessengerCreateInfoEXT;
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
        VkDebugUtilsMessageTypeFlagsEXT             message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void*                                       user_data);

    // Required device extensions
    static const std::vector<const char*> REQUIRED_DEVICE_EXTENSIONS;

    // Validation layers (debug builds only)
#ifdef NDEBUG
    static constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
    static constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif
    static const std::vector<const char*> VALIDATION_LAYERS;

    // Members — destruction order is reverse of declaration
    // context_ must be first (destroyed last)
    vk::raii::Context              context_;
    vk::raii::Instance             instance_      = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger_ = nullptr;
    vk::raii::SurfaceKHR           surface_       = nullptr;
    vk::raii::PhysicalDevice       physicalDevice_ = nullptr;
    vk::raii::Device               device_        = nullptr;
    vk::raii::Queue                graphicsQueue_ = nullptr;
    uint32_t                       queueFamilyIndex_ = 0;
};
```

**Key implementation notes:**

- Constructor calls private init methods in order: `createInstance` → `setupDebugMessenger` → `createSurface` → `pickPhysicalDevice` → `createLogicalDevice`.
- `createInstance`: populates `vk::ApplicationInfo` + `vk::InstanceCreateInfo`. In debug builds, appends `VK_EXT_debug_utils` to extensions and passes `makeDebugMessengerCreateInfo()` in `pNext` for coverage during instance creation/destruction.
- `setupDebugMessenger`: creates `vk::raii::DebugUtilsMessengerEXT` (debug builds only).
- `createSurface`: calls `glfwCreateWindowSurface(*instance_, window, nullptr, &raw_surface)`, wraps result in `vk::raii::SurfaceKHR`.
- `pickPhysicalDevice`: enumerates via `instance_.enumeratePhysicalDevices()`, calls `isDeviceSuitable` on each, stores first passing device and its queue family index.
- `isDeviceSuitable`: checks API version ≥ 1.3, calls `findQueueFamily` for a valid index, calls `checkDeviceExtensionSupport`, checks `dynamicRendering` and `extendedDynamicState` via `getFeatures2` with chained feature structs.
- `findQueueFamily`: iterates queue family properties, returns index of first family where `eGraphics` is set AND `getSurfaceSupportKHR` returns true. Throws if none found.
- `createLogicalDevice`: builds `vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>` with `dynamicRendering = true` and `extendedDynamicState = true`. Single `DeviceQueueCreateInfo` with priority 1.0f. Retrieves queue via `device_.getQueue(queueFamilyIndex_, 0)`.
- `REQUIRED_DEVICE_EXTENSIONS` = `{ vk::KHRSwapchainExtensionName }`.
- `VALIDATION_LAYERS` = `{ "VK_LAYER_KHRONOS_validation" }`.

---

## `src/core/SwapChain.h` / `SwapChain.cpp`

**Responsibilities:** Swap chain creation, image view creation, swap extent/format selection, recreation on resize.

```cpp
#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>
#include <vector>

class VulkanContext;

class SwapChain
{
public:
    SwapChain(const VulkanContext& ctx, GLFWwindow* window);

    // Non-copyable
    SwapChain(const SwapChain&)            = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    void recreate();

    // Accessors
    vk::Format   getFormat()     const;
    vk::Extent2D getExtent()     const;
    uint32_t     imageCount()    const;

    auto get()          const -> const vk::raii::SwapchainKHR&;
    auto getImageViews() const -> const std::vector<vk::raii::ImageView>&;

private:
    void create();
    void createImageViews();
    void cleanup();

    vk::SurfaceFormatKHR chooseFormat(
        const std::vector<vk::SurfaceFormatKHR>& formats)        const;
    vk::PresentModeKHR   choosePresentMode(
        const std::vector<vk::PresentModeKHR>&   modes)          const;
    vk::Extent2D         chooseExtent(
        const vk::SurfaceCapabilitiesKHR&         capabilities)  const;
    uint32_t             chooseImageCount(
        const vk::SurfaceCapabilitiesKHR&         capabilities)  const;

    const VulkanContext&              ctx_;
    GLFWwindow*                       window_;

    vk::raii::SwapchainKHR            swapChain_     = nullptr;
    std::vector<vk::Image>            images_;
    std::vector<vk::raii::ImageView>  imageViews_;
    vk::SurfaceFormatKHR              surfaceFormat_ = {};
    vk::Extent2D                      extent_        = {};
};
```

**Key implementation notes:**

- Constructor stores refs, then calls `create()` and `createImageViews()`.
- `create()`: queries `getSurfaceCapabilitiesKHR`, `getSurfaceFormatsKHR`, `getSurfacePresentModesKHR`. Calls the four chooser helpers. Builds `vk::SwapchainCreateInfoKHR` with `imageArrayLayers = 1`, `imageUsage = eColorAttachment`, `imageSharingMode = eExclusive`, `clipped = true`, `compositeAlpha = eOpaque`, `oldSwapchain = nullptr` (first creation) or `*swapChain_` (on recreate).
- `createImageViews()`: iterates `swapChain_.getImages()`, creates one `vk::raii::ImageView` per image with `viewType = e2D`, `eColor` aspect, mip 0, layer 0.
- `cleanup()`: resets `imageViews_` vector and `swapChain_` to null RAII handles.
- `recreate()`: calls `cleanup()` then `create()` then `createImageViews()`.
- `chooseFormat`: prefers `eB8G8R8A8Srgb` + `eSrgbNonlinear`, falls back to `availableFormats[0]`.
- `choosePresentMode`: prefers `eMailbox`, falls back to `eFifo` (guaranteed).
- `chooseExtent`: if `currentExtent.width != UINT32_MAX` return it directly; else query `glfwGetFramebufferSize` and clamp to `[minImageExtent, maxImageExtent]`.
- `chooseImageCount`: requests `max(3, minImageCount)`, clamped to `maxImageCount` if non-zero.

---

## `src/renderer/FrameData.h`

**Responsibilities:** POD struct grouping per-frame GPU synchronisation resources.

```cpp
#pragma once
#include <vulkan/vulkan_raii.hpp>

struct FrameData
{
    vk::raii::CommandBuffer commandBuffer  = nullptr;
    vk::raii::Semaphore     imageAvailable = nullptr;
    vk::raii::Semaphore     renderFinished = nullptr;
    vk::raii::Fence         inFlightFence  = nullptr;
};
```

**Key implementation notes:**

- No constructor, no methods. Pure data grouping.
- Populated by `Renderer::createFrameData()`.
- All four handles default to null RAII — safe to default-construct the array.

---

## `src/renderer/GraphicsPipeline.h` / `GraphicsPipeline.cpp`

**Responsibilities:** Shader loading, all fixed-function pipeline state, pipeline layout, pipeline object creation, command recording.

```cpp
#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <string>

class VulkanContext;

class GraphicsPipeline
{
public:
    GraphicsPipeline(const VulkanContext& ctx, vk::Format color_format);

    // Non-copyable
    GraphicsPipeline(const GraphicsPipeline&)            = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;

    // Called by Renderer each frame
    void record(
        vk::CommandBuffer command_buffer,
        vk::Extent2D      extent,
        vk::ImageView     image_view);

    auto getPipeline() const -> const vk::raii::Pipeline&;

private:
    void createPipelineLayout();
    void createPipeline(vk::Format color_format);

    auto createShaderModule(const std::string& spirv_path) const
        -> vk::raii::ShaderModule;

    // Image layout transition helper (synchronization2)
    static void transitionImageLayout(
        vk::CommandBuffer     command_buffer,
        vk::Image             image,
        vk::ImageLayout       old_layout,
        vk::ImageLayout       new_layout,
        vk::PipelineStageFlags2 src_stage,
        vk::AccessFlags2        src_access,
        vk::PipelineStageFlags2 dst_stage,
        vk::AccessFlags2        dst_access);

    const VulkanContext&    ctx_;
    vk::raii::PipelineLayout layout_   = nullptr;
    vk::raii::Pipeline       pipeline_ = nullptr;
};
```

**Key implementation notes:**

- Constructor calls `createPipelineLayout()` then `createPipeline(color_format)`.
- `createPipelineLayout()`: empty layout for now — no push constants, no descriptor sets. `vk::PipelineLayoutCreateInfo{}` with zero counts.
- `createPipeline()`: builds all fixed-function structs in order:
  - `vk::PipelineVertexInputStateCreateInfo` — zero bindings, zero attributes (data in shader)
  - `vk::PipelineInputAssemblyStateCreateInfo` — `eTriangleList`, no restart
  - `vk::PipelineViewportStateCreateInfo` — 1 viewport, 1 scissor (both dynamic)
  - `vk::PipelineDynamicStateCreateInfo` — `eViewport`, `eScissor`
  - `vk::PipelineRasterizationStateCreateInfo` — `eFill`, back-face cull, CCW front face, no depth bias
  - `vk::PipelineMultisampleStateCreateInfo` — `e1`, disabled
  - `vk::PipelineColorBlendAttachmentState` — blend disabled, all channels written
  - `vk::PipelineColorBlendStateCreateInfo` — references single attachment, no logic op
  - `vk::PipelineRenderingCreateInfo` (in `pNext`) — 1 color attachment, `color_format`
  - `vk::GraphicsPipelineCreateInfo` — assembles all above, `renderPass = nullptr`
- `record()`: called by `Renderer` each frame. Performs:
  1. `transitionImageLayout` from `eUndefined`→`eColorAttachmentOptimal`
  2. Fill `vk::RenderingAttachmentInfo` — `image_view`, layout `eColorAttachmentOptimal`, load `eClear` (black), store `eStore`
  3. `commandBuffer.beginRendering(renderingInfo)` — render area = full `extent`
  4. `commandBuffer.bindPipeline(eGraphics, *pipeline_)`
  5. Set dynamic viewport: `{0, 0, extent.width, extent.height, 0.0f, 1.0f}`
  6. Set dynamic scissor: `{{0,0}, extent}`
  7. `commandBuffer.draw(3, 1, 0, 0)`
  8. `commandBuffer.endRendering()`
  9. `transitionImageLayout` from `eColorAttachmentOptimal`→`ePresentSrcKHR`
- `transitionImageLayout`: uses `vk::ImageMemoryBarrier2` + `vk::DependencyInfo` + `commandBuffer.pipelineBarrier2()` (synchronization2 path).
- Shader path convention: `shaders/triangle.vert.spv` and `shaders/triangle.frag.spv` (relative to executable).

---

## `src/renderer/Renderer.h` / `Renderer.cpp`

**Responsibilities:** Command pool, per-frame resource ring buffer, frame orchestration (acquire → record → submit → present).

```cpp
#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <array>
#include "FrameData.h"

class VulkanContext;
class SwapChain;
class GraphicsPipeline;

class Renderer
{
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    explicit Renderer(const VulkanContext& ctx);

    // Non-copyable
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Returns true if swap chain recreation is needed
    bool drawFrame(SwapChain& swap_chain, GraphicsPipeline& pipeline);

private:
    void createCommandPool();
    void createFrameData();

    const VulkanContext& ctx_;

    vk::raii::CommandPool                         commandPool_  = nullptr;
    std::array<FrameData, MAX_FRAMES_IN_FLIGHT>   frames_;
    uint32_t                                      currentFrame_ = 0;
};
```

**Key implementation notes:**

- Constructor calls `createCommandPool()` then `createFrameData()`.
- `createCommandPool()`: `eResetCommandBuffer` flag, queue family index from `ctx_.getQueueFamilyIndex()`.
- `createFrameData()`: for each frame in `frames_`:
  - Allocate one primary `vk::raii::CommandBuffer` from the pool
  - Create `imageAvailable` semaphore (no flags)
  - Create `renderFinished` semaphore (no flags)
  - Create `inFlightFence` with `eFenceCreateFlagBits::eSignaled` (first frame safety)
- `drawFrame()` sequence:
  1. `ctx_.getDevice().waitForFences(*frames_[currentFrame_].inFlightFence, true, UINT64_MAX)`
  2. `auto [result, image_index] = swap_chain.get().acquireNextImage(UINT64_MAX, *frames_[currentFrame_].imageAvailable, nullptr)` — if `eErrorOutOfDateKHR` return `true` immediately (before fence reset)
  3. `ctx_.getDevice().resetFences(*frames_[currentFrame_].inFlightFence)` — only after confirmed acquisition
  4. Reset and begin command buffer
  5. Call `pipeline.record(commandBuffer, swap_chain.getExtent(), *swap_chain.getImageViews()[image_index])`
  6. End command buffer
  7. Submit via `vk::SubmitInfo2` — wait on `imageAvailable` at `eColorAttachmentOutput`, signal `renderFinished`, signal `inFlightFence`
  8. Present via `vk::PresentInfoKHR` — wait on `renderFinished`, present `image_index`
  9. If present returns `eErrorOutOfDateKHR` or `eSuboptimalKHR` → return `true`
  10. `currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT`
  11. Return `false`

---

## `src/utils/FileUtils.h`

**Responsibilities:** SPIR-V binary loading.

```cpp
#pragma once
#include <vector>
#include <string>
#include <cstdint>

// Reads a SPIR-V binary file and returns its contents as uint32_t words.
// Throws std::runtime_error if the file cannot be opened.
auto readSpirv(const std::string& file_path) -> std::vector<uint32_t>;
```

**Key implementation notes:**

- Opens file with `std::ios::binary | std::ios::ate`.
- Asserts file size is a multiple of 4 (SPIR-V alignment requirement).
- Returns `std::vector<uint32_t>` — `vk::ShaderModuleCreateInfo` expects this type directly.

---

## `shaders/triangle.slang`

Two entry points in a single file.

**Vertex entry point (`vertexMain`):**
- No input buffers — positions and colours are compile-time arrays in the shader
- Hardcoded positions: `{0.0, -0.5}`, `{0.5, 0.5}`, `{-0.5, 0.5}` (NDC, will appear as an upright triangle)
- Hardcoded colours: red, green, blue per vertex
- Outputs `SV_Position` (clip space) and interpolated `COLOR`

**Fragment entry point (`fragmentMain`):**
- Input: interpolated `COLOR` from vertex stage
- Output: `SV_Target` — writes colour directly to the attachment

**Compilation (add to CMakeLists.txt):**
```cmake
add_custom_command(
    OUTPUT  ${CMAKE_BINARY_DIR}/shaders/triangle.vert.spv
            ${CMAKE_BINARY_DIR}/shaders/triangle.frag.spv
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
    COMMAND $ENV{VULKAN_SDK}/Bin/slangc.exe
            ${CMAKE_SOURCE_DIR}/shaders/triangle.slang
            -entry vertexMain   -stage vertex   -o ${CMAKE_BINARY_DIR}/shaders/triangle.vert.spv
    COMMAND $ENV{VULKAN_SDK}/Bin/slangc.exe
            ${CMAKE_SOURCE_DIR}/shaders/triangle.slang
            -entry fragmentMain -stage fragment -o ${CMAKE_BINARY_DIR}/shaders/triangle.frag.spv
    DEPENDS ${CMAKE_SOURCE_DIR}/shaders/triangle.slang
    COMMENT "Compiling triangle shaders"
)
add_custom_target(Shaders DEPENDS
    ${CMAKE_BINARY_DIR}/shaders/triangle.vert.spv
    ${CMAKE_BINARY_DIR}/shaders/triangle.frag.spv
)
add_dependencies(VulkanTutorial Shaders)
```

---

## `CMakeLists.txt` Changes Required

- Add all new `.cpp` files to `add_executable` (or use `GLOB` — explicit preferred)
- Add `target_include_directories` for `src/` so `#include "core/VulkanContext.h"` works from anywhere
- Add shader compilation block (above)
- Add `VULKAN_HPP_NO_STRUCT_CONSTRUCTORS` to compile definitions (already present)

---

## Verification Targets

When implementation is complete, the following must hold before chapter is marked done:

| # | Test | Expected result |
|---|------|-----------------|
| V1 | Application launches | Window opens, no crash |
| V2 | Validation layers | Zero errors or warnings in stderr |
| V3 | Triangle visible | Smooth RGB-shaded triangle on black background |
| V4 | Window resize | Triangle redraws correctly at new size, no crash |
| V5 | Window minimise | Application pauses, resumes correctly on restore |
| V6 | Clean exit | Window closes, no validation errors on shutdown |
| V7 | GPU name printed | Physical device name logged to stdout on startup |
