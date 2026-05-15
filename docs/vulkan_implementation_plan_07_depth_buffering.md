# Implementation Plan — Chapter 07: Depth Buffering

## Architecture Decisions

| # | Question | Decision |
|---|----------|----------|
| Q1 | Depth resource grouping | `DepthBuffer.h` — header-only move-only struct owning `Image`, `DeviceMemory`, `ImageView` + `getImageView()` accessor |
| Q2a | Format helper location | `findSupportedFormat(candidates, tiling, features)` + `findDepthFormat()` on `VulkanContext` (physical device queries) |
| Q2b | Resource factory location | `Renderer::createDepthResources(vk::Extent2D) → DepthBuffer` (needs GPU helpers) |
| Q3 | Pipeline format supply | `Application` calls `context_->findDepthFormat()` once; result passed as constructor parameter to `GraphicsPipeline` |
| Q4 | Depth view in `record()` | Additional parameter — `record(cmd, extent, colorImageView, depthImageView)` |
| Q5 | `transitionImageLayout` | Add `vk::ImageAspectFlags` parameter; existing call sites pass `eColor` explicitly |

---

## Folder / File Changes

### New files
```
src/renderer/DepthBuffer.h          ← header-only RAII struct
```

### Modified files
```
src/core/VulkanContext.h/.cpp       ← findSupportedFormat, findDepthFormat
src/renderer/Renderer.h/.cpp        ← createDepthResources, transitionImageLayout extended
src/renderer/GraphicsPipeline.h/.cpp ← depth format param, depth stencil state, depth attachment in record()
src/Application.h/.cpp              ← depthBuffer_ member, wiring, resize
src/renderer/buffers/Vertex.h       ← pos vec2 → vec3, attribute format update
shaders/triangle.slang              ← position float2 → float3
```

No `CMakeLists.txt` change — `DepthBuffer.h` is header-only.

---

## New / Changed Signatures

```cpp
// VulkanContext.h
auto findSupportedFormat(
    const std::vector<vk::Format>& candidates,
    vk::ImageTiling tiling,
    vk::FormatFeatureFlags features) const -> vk::Format;

auto findDepthFormat() const -> vk::Format;

// Renderer.h — extended signature (all call sites updated)
void transitionImageLayout(
    vk::Image image,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout,
    vk::AccessFlags2 src_access,
    vk::AccessFlags2 dst_access,
    vk::PipelineStageFlags2 src_stage,
    vk::PipelineStageFlags2 dst_stage,
    vk::ImageAspectFlags aspect);          // ← new param

auto createDepthResources(vk::Extent2D extent) -> DepthBuffer;

// GraphicsPipeline.h — constructor gains depth format
GraphicsPipeline(
    const VulkanContext& context,
    const FrameDescriptorLayout& frame_descriptor_layout,
    const TextureDescriptorLayout& texture_descriptor_layout,
    vk::Format depth_format);              // ← new param

// record() gains depth image view
void record(
    vk::CommandBuffer command_buffer,
    vk::Extent2D extent,
    vk::ImageView color_image_view,
    vk::ImageView depth_image_view,        // ← new param
    const vk::raii::DescriptorSet& frame_descriptor_set,
    const vk::raii::DescriptorSet& texture_descriptor_set,
    const Mesh& mesh);
```

---

## Build Order

Work through files in this order — each step should compile cleanly (or have known-broken call sites) before moving to the next.

### Step 1 — `Vertex.h`
- Change `pos` type from `glm::vec2` to `glm::vec3`
- Change position attribute description format to `vk::Format::eR32G32B32Sfloat`
- `offsetof(Vertex, uv)` and stride via `sizeof(Vertex)` update automatically

### Step 2 — `shaders/triangle.slang`
- Change `VertexInput.pos` from `float2` to `float3`
- The MVP multiply still works — `model * float4(input.pos, 1.0)` becomes correct with three-component input

### Step 3 — `Application.cpp` — vertex data
- Add Z coordinate (0.0f) to all four existing rectangle vertices
- No other Application changes yet

### Step 4 — `VulkanContext.h/.cpp`
- Add `findSupportedFormat(candidates, tiling, features) const -> vk::Format`
  - Iterate candidates; call `physicalDevice_.getFormatProperties(format)`
  - For `eOptimal` tiling: check `optimalTilingFeatures & features == features`
  - For `eLinear` tiling: check `linearTilingFeatures & features == features`
  - Throw `std::runtime_error` if no candidate passes
- Add `findDepthFormat() const -> vk::Format`
  - Calls `findSupportedFormat` with `{eD32Sfloat, eD32SfloatS8Uint, eD24UnormS8Uint}`, `eOptimal`, `eDepthStencilAttachment`

### Step 5 — `DepthBuffer.h`
- Header-only move-only struct
- Constructor takes `vk::raii::Image`, `vk::raii::DeviceMemory`, `vk::raii::ImageView` by move
- `auto getImageView() const -> vk::ImageView` — returns `*imageView_`
- Members: `image_`, `memory_`, `imageView_` (destruction order: view → memory → image, so declare image first)

### Step 6 — `Renderer.h/.cpp` — extend `transitionImageLayout`
- Add `vk::ImageAspectFlags aspect` parameter to declaration and definition
- Replace hardcoded `eColor` aspect in the barrier `subresourceRange` with the parameter
- Update all existing call sites in `Renderer.cpp` to pass `vk::ImageAspectFlagBits::eColor`

### Step 7 — `Renderer.h/.cpp` — `createDepthResources`
- Add `#include "DepthBuffer.h"` to `Renderer.h`
- Declare `auto createDepthResources(vk::Extent2D extent) -> DepthBuffer`
- Implement:
  - Call `context_.findDepthFormat()` to get format
  - Call `createImage(extent.width, extent.height, format, eOptimal, eDepthStencilAttachment, eDeviceLocal)`
  - Call `createImageView(image, format, eDepth)`
  - Call `transitionImageLayout(image, eUndefined, eDepthAttachmentOptimal, {}, eDepthStencilAttachmentWrite, eTopOfPipe, eEarlyFragmentTests | eLateFragmentTests, eDepth)`
  - Return `DepthBuffer(std::move(image), std::move(memory), std::move(view))`

### Step 8 — `GraphicsPipeline.h/.cpp`
- Constructor: add `vk::Format depth_format` parameter; store as `depthFormat_` member
- `createPipeline()`:
  - Add `vk::PipelineDepthStencilStateCreateInfo` with `depthTestEnable`, `depthWriteEnable`, `depthCompareOp = eLess`
  - Add `.pDepthStencilState = &depth_stencil` to `GraphicsPipelineCreateInfo`
  - Add `depthAttachmentFormat = depthFormat_` to `PipelineRenderingCreateInfo`
- `record()`:
  - Add `vk::ImageView depth_image_view` parameter
  - Construct `vk::RenderingAttachmentInfo` for depth: `imageView = depth_image_view`, `imageLayout = eDepthAttachmentOptimal`, `loadOp = eClear`, `storeOp = eDontCare`, `clearValue = ClearDepthStencilValue(1.0f, 0)`
  - Add `.pDepthAttachment = &depth_attachment_info` to `vk::RenderingInfo`

### Step 9 — `Application.h/.cpp` — wiring
- `Application.h`:
  - Forward declare `class DepthBuffer`
  - Add `std::unique_ptr<DepthBuffer> depthBuffer_` member — destruction order: `mesh_` → `texture_` → `depthBuffer_` → `renderer_` → `graphicsPipeline_` → ...
- `Application.cpp`:
  - Add `#include "renderer/DepthBuffer.h"`
  - In `initVulkan()`:
    - Call `context_->findDepthFormat()` → store in local `depth_format`
    - Pass `depth_format` to `GraphicsPipeline` constructor
    - After pipeline: `depthBuffer_ = std::make_unique<DepthBuffer>(renderer_->createDepthResources(swapChain_->getExtent()))`
  - In `onResize()`: after `swapChain_->recreate()`, reassign `*depthBuffer_ = renderer_->createDepthResources(swapChain_->getExtent())`
  - Thread `depthBuffer_->getImageView()` through `drawFrame()` → `record()`

---

## Destruction Order in `Application.h`

```
mesh_             (unique_ptr) — destroyed first
texture_          (unique_ptr)
depthBuffer_      (unique_ptr) ← new
renderer_         (unique_ptr)
graphicsPipeline_ (unique_ptr)
frameDescriptorLayout_   (unique_ptr)
textureDescriptorLayout_ (unique_ptr)
swapChain_        (unique_ptr)
context_          (unique_ptr) — destroyed last
window_           (GLFWwindow*, manual cleanup in destructor)
```

---

## Smoke Test Bar

Before marking the chapter done, all of the following must be true:

1. Rectangle still renders and rotates with correct aspect ratio
2. Geometry at Z=0 correctly occludes geometry at Z=−0.5 regardless of draw order
3. Resize: depth buffer recreated, no validation errors, rendering resumes correctly
4. Minimise + restore: no crash
5. Validation layers silent (OBS_HOOK warning permitted)
