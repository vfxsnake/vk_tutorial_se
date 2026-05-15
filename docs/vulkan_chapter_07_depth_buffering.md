# Chapter 07: Depth Buffering

> **Source:** https://docs.vulkan.org/tutorial/latest/07_Depth_buffering.html
> **Vulkan Version:** 1.4 | **Language:** C++20 | **Shading:** Slang | **Bindings:** Vulkan-Hpp RAII

---

## Overview

Depth buffering solves the fundamental rendering problem of **draw-order dependency**: without it, whichever geometry is submitted last wins, regardless of its actual distance from the camera. A depth buffer (also called a z-buffer) is an additional image attachment the same resolution as the colour attachment. Instead of storing colour, it stores the depth value of the closest fragment that has been written to each screen-space pixel so far. When a new fragment arrives, its depth is compared against the stored value — if it is behind what is already there, it is discarded; if it is in front, it overwrites both the colour and depth values.

This chapter adds the depth buffer as a resource owned alongside the swap chain, wires it into the dynamic rendering attachment, enables depth testing in the pipeline state, and teaches the GPU how to do comparisons. It also extends vertex positions from 2D to 3D, giving you geometry that actually occupies different depth planes.

---

## Section 1 — 3D Geometry Preparation

### Concepts

Before depth testing is meaningful you need vertices at genuinely different depths. The tutorial adds a second rectangle behind the first, with Z coordinates spread across the 0.0–1.0 range (Vulkan NDC depth). Without depth testing, the second rectangle would show through the first depending purely on draw order. With depth testing, the closer rectangle always wins regardless of submission order.

The changes required here are mechanical: `glm::vec2` positions become `glm::vec3`, the vertex format enum gains a third component, and the shader input struct gains a `float z` coordinate. The GLM compile definition `GLM_FORCE_DEPTH_ZERO_TO_ONE` — already present in our `CMakeLists.txt` since Chapter 05 — ensures that `glm::perspective` maps the near and far planes onto Vulkan's [0, 1] depth range, not OpenGL's [−1, +1].

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::Format::eR32G32B32Sfloat` | Enum value | Three-component 32-bit float format used for `vec3` position attribute |
| `GLM_FORCE_DEPTH_ZERO_TO_ONE` | Preprocessor define | Makes `glm::perspective` emit a [0,1] depth range clip matrix instead of [−1,1] |

### Code Walkthrough

In `Vertex.h`, the `pos` field type changes from `glm::vec2` to `glm::vec3`. The binding description stride automatically grows from 20 bytes to 24 bytes (two floats → three floats for position, still two floats for UV). The attribute description format for position changes to `eR32G32B32Sfloat` — the offset of `uv` (now 12 bytes into the struct) is handled by `offsetof` automatically.

In `Application.cpp`, the hardcoded vertex list gains Z values. The tutorial uses two overlapping quads at Z=0.0 and Z=−0.5. In our project we have one textured quad, so the change is just adding a Z=0.0 (or whatever depth makes sense) to each existing vertex. The shader position input changes from `float2` to `float3` — in Slang the projection matrix handles the clip-space W division, so no other shader change is needed beyond the input type.

### Common Pitfalls

- Forgetting `GLM_FORCE_DEPTH_ZERO_TO_ONE` causes the perspective matrix to produce values outside [0,1], making everything clipped or reversed. This is already set in our `CMakeLists.txt`.
- The `stride` in the binding description is computed by `sizeof(Vertex)` — it updates automatically when `pos` grows. No manual recalculation needed.
- If the format of the position attribute is left as `eR32G32Sfloat` (2-component) after adding a Z field, the GPU will read only X and Y from the buffer, silently ignoring Z.

---

## Section 2 — Depth Image Resources

### Concepts

A depth buffer is a regular `vk::Image` with a depth-compatible format and usage `eDepthStencilAttachment`. It must match the swap chain resolution because there is one depth value per pixel. Like any GPU image, it needs a `DeviceMemory` backing allocation and an `ImageView` for the pipeline to access it.

**Format selection** is not hardcoded because hardware support for specific depth formats varies. The tutorial queries format support via `vkGetPhysicalDeviceFormatProperties` in priority order:

1. `eD32Sfloat` — 32-bit float depth, no stencil. Sufficient for all depth testing.
2. `eD32SfloatS8Uint` — 32-bit float depth + 8-bit stencil integer.
3. `eD24UnormS8Uint` — 24-bit normalised depth + 8-bit stencil integer.

The GPU advertises what tiling modes and usage flags each format supports. We query `optimalTilingFeatures` (since the depth image uses `eOptimal` tiling) and check for `eDepthStencilAttachment`.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::PhysicalDevice::getFormatProperties(format)` | Method | Returns `VkFormatProperties` with `linearTilingFeatures`, `optimalTilingFeatures`, `bufferFeatures` bitmasks |
| `vk::FormatFeatureFlagBits::eDepthStencilAttachment` | Enum value | Feature bit required for a format to be usable as a depth/stencil attachment |
| `vk::ImageUsageFlagBits::eDepthStencilAttachment` | Enum value | Usage flag for depth (and optionally stencil) attachment images |
| `vk::ImageAspectFlagBits::eDepth` | Enum value | Aspect mask that selects the depth plane of an image (vs `eStencil` or `eColor`) |
| `vk::Format::eD32Sfloat` | Enum value | Preferred 32-bit float depth format |
| `vk::Format::eD32SfloatS8Uint` | Enum value | 32-bit float depth + 8-bit stencil |
| `vk::Format::eD24UnormS8Uint` | Enum value | 24-bit normalised depth + 8-bit stencil |

### Code Walkthrough

Two helper functions are introduced: `findSupportedFormat(candidates, tiling, features)` and `findDepthFormat()`. `findSupportedFormat` iterates the candidate list, calls `getFormatProperties` for each, and returns the first format whose properties include all requested feature flags under the requested tiling mode. `findDepthFormat` calls it with the three depth format candidates, `eOptimal` tiling, and the `eDepthStencilAttachment` feature bit.

The depth image itself is created with `createImage()` (which we already have on `Renderer`) using the selected format, swap-chain extent dimensions, `eDepthStencilAttachment` usage, `eDeviceLocal` memory, and `eOptimal` tiling. The `ImageView` is created with `createImageView()` but with aspect `eDepth` instead of `eColor`.

There is also a helper `hasStencilComponent(format)` that checks whether a format contains a stencil plane — used when setting up barriers to ensure the `ImageAspectFlagBits` includes `eStencil` when appropriate.

### Common Pitfalls

- Using `eLinear` tiling for the depth image will compile but may silently fall back to a slower path or fail format feature checks. Always use `eOptimal` for render targets.
- Querying with `linearTilingFeatures` when the image will use `eOptimal` tiling gives wrong results — the feature sets are per-tiling-mode and can differ.
- An unsupported format in the priority list is silently skipped; if all three are unsupported (rare on desktop), `findSupportedFormat` should throw rather than return an invalid format.

---

## Section 3 — Layout Transition for the Depth Image

### Concepts

A freshly created `vk::Image` has layout `eUndefined`. Before it can be used as a depth attachment it must be transitioned to `eDepthAttachmentOptimal` (the modern Vulkan 1.2+ layout for depth-only images). This is done once at creation time via the `transitionImageLayout` helper already on `Renderer`.

The key difference from colour-image transitions is the pipeline stage and access mask values: depth testing happens in `eEarlyFragmentTests` and `eLateFragmentTests`, not in `eColorAttachmentOutput`. The access mask on the destination is `eDepthStencilAttachmentWrite` (and `eDepthStencilAttachmentRead` if the operation reads before writing, which is the general case).

For the source side of the `eUndefined → eDepthAttachmentOptimal` transition, the stage is `eTopOfPipe` and access mask is `{}` — the image has no prior owner, so nothing needs to be waited on.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::ImageLayout::eDepthAttachmentOptimal` | Enum value | Modern layout for depth-only images used as attachments (Vulkan 1.2+) |
| `vk::PipelineStageFlagBits2::eEarlyFragmentTests` | Enum value | Stage where depth/stencil testing happens before the fragment shader |
| `vk::PipelineStageFlagBits2::eLateFragmentTests` | Enum value | Stage where depth/stencil testing happens after the fragment shader (for `discard`) |
| `vk::AccessFlagBits2::eDepthStencilAttachmentWrite` | Enum value | Write access to a depth/stencil attachment |
| `vk::AccessFlagBits2::eDepthStencilAttachmentRead` | Enum value | Read access to a depth/stencil attachment |

### Code Walkthrough

Our existing `transitionImageLayout()` handles colour images and the two-stage texture upload transitions. For depth, we need to either extend it to accept an `ImageAspectFlags` parameter (so it can build the correct `subresourceRange`) or add a branch inside the function to detect depth formats and select the right aspect. The tutorial adds an `ImageAspectFlagBits` parameter to the function signature and passes `eDepth` explicitly at the call site.

The transition is called once from `createDepthResources()` immediately after the `ImageView` is created. Because this uses `beginSingleTimeCommands` / `endSingleTimeCommands` with a `waitIdle`, it completes before the main loop begins.

### Common Pitfalls

- Using `eColorAttachmentOutput` as the destination stage for a depth transition is incorrect and will cause validation errors. Depth testing is `eEarlyFragmentTests | eLateFragmentTests`.
- If `hasStencilComponent(format)` is true, the image aspect in the barrier should be `eDepth | eStencil`, not just `eDepth`. Omitting `eStencil` from the aspect mask on a stencil-capable format produces a validation warning.
- The `eUndefined` source layout means the barrier does not need a `srcAccessMask` — the driver is free to discard existing contents. This is correct and intentional.

---

## Section 4 — Dynamic Rendering Integration

### Concepts

In the dynamic rendering path (which we use, not legacy render passes), the depth attachment is declared per-frame inside `vkCmdBeginRendering` via a `vk::RenderingAttachmentInfo` pointing at the depth image view. This is symmetric with the colour attachment we already supply.

Two configuration decisions matter here:
- **Clear value**: `vk::ClearDepthStencilValue(1.0f, 0)` — clear to 1.0 (far plane), which is the maximum depth. Fragments that pass `eLess` comparison will write smaller depth values as geometry gets closer.
- **Store op**: `eDontCare` — the depth buffer result does not need to be preserved after the frame ends, unlike the colour attachment. This is a performance hint to the GPU that it can discard the depth attachment data after use (saves bandwidth on tile-based renderers).

The `vk::RenderingInfo` struct gains a `.pDepthAttachment` field pointing to this new `RenderingAttachmentInfo`.

We also need to tell the pipeline creation what depth format to expect, via `vk::PipelineRenderingCreateInfo::depthAttachmentFormat`. This is set at pipeline creation time, not at draw time — it must match whatever format `createDepthResources()` selects.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::RenderingAttachmentInfo` | Struct | Describes one attachment (colour or depth) for a dynamic rendering pass |
| `vk::ClearDepthStencilValue` | Struct | Holds `depth` (float) and `stencil` (uint32_t) clear values |
| `vk::AttachmentStoreOp::eDontCare` | Enum value | Tells the driver attachment contents are not needed after the pass |
| `vk::PipelineRenderingCreateInfo::depthAttachmentFormat` | Field | Depth format the pipeline expects — must match the actual depth image format |
| `vk::RenderingInfo::pDepthAttachment` | Field | Pointer to the depth `RenderingAttachmentInfo` |

### Code Walkthrough

In `GraphicsPipeline::record()`, the existing `vk::RenderingAttachmentInfo` for colour already has `loadOp = eClear`, `storeOp = eStore`. The new depth attachment is constructed the same way: `imageView` from the depth `ImageView`, layout `eDepthAttachmentOptimal`, `loadOp = eClear`, `storeOp = eDontCare`, clear value `ClearDepthStencilValue(1.0f, 0)`.

The `vk::RenderingInfo` struct gains `.pDepthAttachment = &depth_attachment_info`.

In `GraphicsPipeline::createPipeline()`, the `vk::PipelineRenderingCreateInfo` that is chained into `GraphicsPipelineCreateInfo` via `pNext` already has `colorAttachmentCount = 1` and `pColorAttachmentFormats`. We add `depthAttachmentFormat = depthFormat_` where `depthFormat_` is a member set from the selected depth format, passed in at construction.

The depth attachment image view must be passed into `record()` each frame so it can be placed in the `RenderingAttachmentInfo`. On swap chain recreation, the depth image and view are also recreated, so the new view handle must reach `record()` without being stale.

### Common Pitfalls

- Passing `eDepthStencilAttachmentOptimal` (the legacy combined layout) instead of `eDepthAttachmentOptimal` works but triggers a validation note on Vulkan 1.2+ drivers that prefer the more specific layout.
- If `depthAttachmentFormat` in `PipelineRenderingCreateInfo` is left as `eUndefined` when a depth attachment is actually provided at draw time, validation will warn that the pipeline and the rendering info disagree.
- `storeOp = eDontCare` is correct for depth but would be catastrophic for the colour attachment. Don't conflate the two.

---

## Section 5 — Pipeline Depth Stencil State

### Concepts

Depth testing is disabled by default in a Vulkan pipeline. It must be explicitly enabled via `vk::PipelineDepthStencilStateCreateInfo`. The three most important fields are:

- **`depthTestEnable`**: whether fragments arriving at the rasteriser are tested against the depth buffer before being written. If false, all fragments pass regardless of depth.
- **`depthWriteEnable`**: whether a fragment that passes the depth test writes its depth value back to the buffer. You would set this to false for transparent geometry — you want transparent fragments to be depth-tested (don't draw through walls) but not to occlude subsequent transparent layers.
- **`depthCompareOp`**: the comparison operator. `eLess` means "pass if the new fragment's depth is strictly less than the stored depth" — i.e., closer to the camera wins. Other options exist (e.g., `eLessOrEqual` for skybox tricks, `eAlways` to disable testing while keeping writes).

Stencil testing (`stencilTestEnable`) and depth bounds testing (`depthBoundsTestEnable`) are both disabled for this chapter.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::PipelineDepthStencilStateCreateInfo` | Struct | Full depth and stencil test configuration for a pipeline |
| `depthTestEnable` | Field | Enables the depth comparison test |
| `depthWriteEnable` | Field | Enables writing the fragment's depth to the depth buffer on pass |
| `depthCompareOp` | Field | The comparison function — `vk::CompareOp::eLess` for standard closer-wins |
| `depthBoundsTestEnable` | Field | Optional range cull — discard fragments outside [minDepthBounds, maxDepthBounds] |
| `stencilTestEnable` | Field | Enables stencil testing (not used here) |
| `vk::GraphicsPipelineCreateInfo::pDepthStencilState` | Pointer field | Wires the depth stencil struct into pipeline creation |

### Code Walkthrough

In `GraphicsPipeline::createPipeline()`, a `vk::PipelineDepthStencilStateCreateInfo` is declared with `depthTestEnable = vk::True`, `depthWriteEnable = vk::True`, `depthCompareOp = vk::CompareOp::eLess`, and all other fields at their zero/false defaults. The address of this struct is passed to `vk::GraphicsPipelineCreateInfo::pDepthStencilState`.

This is a pure pipeline-creation change. No changes to the frame recording path are needed to enable the test itself — the test runs automatically on every fragment once the pipeline is bound and a depth attachment is declared in the rendering info.

### Common Pitfalls

- Leaving `pDepthStencilState = nullptr` in `GraphicsPipelineCreateInfo` while also providing a `depthAttachmentFormat` in `PipelineRenderingCreateInfo` will produce a validation error. The two must be consistent.
- `depthCompareOp = eGreater` combined with a clear value of 0.0 (near plane) implements a "reverse depth" buffer for improved floating-point precision at range — a legitimate technique, but only valid when the clear value and compare op are changed as a pair. Using `eGreater` with a 1.0 clear means nothing ever passes.
- `depthWriteEnable = false` on opaque geometry is a correctness bug: later fragments can incorrectly overwrite closer ones because the buffer is never updated.

---

## Section 6 — Swap Chain Recreation

### Concepts

The depth buffer must match the colour attachment in resolution. When the window is resized and the swap chain is recreated, the depth image (and its `DeviceMemory` and `ImageView`) must also be destroyed and recreated at the new size.

In the tutorial's monolithic approach, `cleanupSwapChain()` destroys all three depth resources and `recreateSwapChain()` calls `createDepthResources()` immediately after `createImageViews()`. In our modular architecture the equivalent is: wherever depth resources live (they should be recreated as part of the same lifecycle as swap chain image views), the `onResize()` / `recreate()` path must call the depth recreation logic.

### Code Walkthrough

If depth resources live on the `Renderer`, `Application::onResize()` can call a method like `renderer_->recreateDepthResources(swapChain_->getExtent())` immediately after `swapChain_->recreate()`. If depth resources are attached to the `SwapChain` class instead (since they share the same recreation trigger and the same resolution), `SwapChain::recreate()` can handle them directly.

The key invariant: by the time `Renderer::drawFrame()` begins its next call, the depth image view handle passed into `record()` must point to a live, correctly-sized image at `eDepthAttachmentOptimal` layout.

### Common Pitfalls

- Forgetting to recreate the depth image on resize causes the depth buffer to remain at the old resolution, producing GPU validation errors (`VkImageView` extent mismatch) or silent corruption.
- The depth image must be transitioned back to `eDepthAttachmentOptimal` after recreation. A newly created image starts in `eUndefined`; without the transition, `beginRendering` will see a mismatched layout.
- Destroying the old depth image *before* the GPU has finished using it (i.e., before `waitIdle()` on resize) produces use-after-free validation errors. Our `Application::onResize()` already calls `context_->getLogicalDevice().waitIdle()` first, so this is already handled.

---

## Summary

- **Problem solved:** depth buffering eliminates draw-order dependency by hardware-testing each fragment's depth against a persistent depth attachment.
- **Resources added:** one depth `vk::Image` + `DeviceMemory` + `ImageView`, selected format from a priority list via `findSupportedFormat()`.
- **Layout transition:** `eUndefined → eDepthAttachmentOptimal` once at creation, using `eEarlyFragmentTests | eLateFragmentTests` stage masks.
- **Dynamic rendering:** depth attachment added to `vk::RenderingInfo` with `loadOp = eClear` (clear to 1.0), `storeOp = eDontCare`.
- **Pipeline state:** `PipelineDepthStencilStateCreateInfo` with `depthTestEnable`, `depthWriteEnable`, `depthCompareOp = eLess`; `depthAttachmentFormat` added to `PipelineRenderingCreateInfo`.
- **Vertex change:** `pos` becomes `glm::vec3` / `float3`, attribute format becomes `eR32G32B32Sfloat`.
- **Resize:** depth resources must be destroyed and recreated alongside swap chain image views.

## Implementation Checklist

- [ ] `Vertex.h` — change `pos` to `glm::vec3`, update attribute description format to `eR32G32B32Sfloat`, verify stride via `sizeof(Vertex)`
- [ ] `Application.cpp` — add Z coordinates to all vertices
- [ ] `triangle.slang` — change position input from `float2` to `float3`
- [ ] `findSupportedFormat(candidates, tiling, features)` — queries `getFormatProperties`, returns first matching format
- [ ] `findDepthFormat()` — calls `findSupportedFormat` with three depth candidates and `eDepthStencilAttachment` feature
- [ ] `hasStencilComponent(format)` — returns true for `eD32SfloatS8Uint` and `eD24UnormS8Uint`
- [ ] `createDepthResources()` — calls `createImage`, `createImageView` (aspect `eDepth`), `transitionImageLayout`
- [ ] Extend `transitionImageLayout()` to accept `vk::ImageAspectFlags` parameter
- [ ] `GraphicsPipeline` constructor — accept depth format, store as member
- [ ] `GraphicsPipeline::createPipeline()` — add `PipelineDepthStencilStateCreateInfo`, add `depthAttachmentFormat` to `PipelineRenderingCreateInfo`
- [ ] `GraphicsPipeline::record()` — accept depth `ImageView`, build depth `RenderingAttachmentInfo`, add to `RenderingInfo`
- [ ] Depth resources recreated on swap chain resize (layout re-transitioned after recreate)

## Further Reading

- [Vulkan Spec — `VkPipelineDepthStencilStateCreateInfo`](https://docs.vulkan.org/spec/latest/chapters/fragops.html#fragops-depth)
- [Vulkan Spec — `VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL`](https://docs.vulkan.org/spec/latest/chapters/resources.html#resources-image-layouts)
- [Vulkan Spec — `vkGetPhysicalDeviceFormatProperties`](https://docs.vulkan.org/spec/latest/chapters/formats.html)
- [Vulkan Guide — Depth Buffering](https://vkguide.dev/docs/chapter-3/depth_buffer/)
