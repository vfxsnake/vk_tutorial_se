# Implementation Plan — Chapters 09 & 10: Mipmaps + MSAA

> **Architecture decisions (all locked)**
> - Q1: `createImage()` takes both `mip_levels` and `num_samples` explicitly — no defaults
> - Q2: `VulkanContext` computes and caches `msaaSamples_` after `pickPhysicalDevice()`; exposes via `getMsaaSamples() const`
> - Q3: New `MsaaColorImage` struct in `src/renderer/image_resources/`, factory `Renderer::createColorResources(extent, format, samples) -> MsaaColorImage`
> - Q4: `record()` gains `vk::ImageView msaa_color_image_view` parameter; swap chain image view becomes the resolve target

---

## Affected files

### New files
| File | Purpose |
|------|---------|
| `src/renderer/image_resources/MsaaColorImage.h` | Move-only struct: Image + DeviceMemory + ImageView |
| `src/renderer/image_resources/MsaaColorImage.cpp` | Constructor implementation |

### Modified files
| File | Changes |
|------|---------|
| `src/core/VulkanContext.h` | `msaaSamples_` member, `getMsaaSamples()` accessor, private `getMaxUsableSampleCount()` helper |
| `src/core/VulkanContext.cpp` | `getMaxUsableSampleCount()` impl, `msaaSamples_` computed after `pickPhysicalDevice()` |
| `src/renderer/image_resources/Texture.h` | Add `mipLevels_` member |
| `src/renderer/Renderer.h` | `createImage` / `createImageView` / `transitionImageLayout` new params; new `generateMipmaps` and `createColorResources` declarations |
| `src/renderer/Renderer.cpp` | All implementations updated; `createDepthResources` and `createColorResources` query `context_.getMsaaSamples()` |
| `src/renderer/GraphicsPipeline.h` | `msaaSamples_` member; `record()` gains `msaa_color_image_view` |
| `src/renderer/GraphicsPipeline.cpp` | `rasterizationSamples` in `createPipeline()`; resolve fields in `record()` |
| `src/Application.h` | `msaaColorImage_` unique_ptr in correct destruction order |
| `src/Application.cpp` | Creates/recreates MSAA colour image; threads `msaa_color_image_view` into `drawFrame()` |
| `CMakeLists.txt` | Add `MsaaColorImage.cpp` |

---

## Folder structure after these chapters

```
src/renderer/image_resources/
├── Texture.h/.cpp
├── DepthImage.h/.cpp
└── MsaaColorImage.h/.cpp       ← new
```

---

## Build order

Do these steps in sequence. Build after each step to keep the error surface small.

### Step 1 — `VulkanContext`: add `msaaSamples_`

**`VulkanContext.h`**
- Add private method `auto getMaxUsableSampleCount() const -> vk::SampleCountFlagBits`
- Add private member `vk::SampleCountFlagBits msaaSamples_`
- Add public accessor `auto getMsaaSamples() const -> vk::SampleCountFlagBits`

**`VulkanContext.cpp`**
- Implement `getMaxUsableSampleCount()`:
  - Call `physicalDevice_.getProperties()` to get `VkPhysicalDeviceProperties`
  - Compute `counts = properties.limits.framebufferColorSampleCounts & properties.limits.framebufferDepthSampleCounts`
  - Walk down `e64 → e32 → e16 → e8 → e4 → e2`; return the first that `counts` contains
  - Return `e1` as fallback
- In the constructor body, after `pickPhysicalDevice()`, assign `msaaSamples_ = getMaxUsableSampleCount()`

**Build checkpoint:** existing tests still pass — no call sites changed.

---

### Step 2 — `Renderer`: update `createImage()` and `createImageView()`

Both Ch09 and Ch10 touch these helpers. Change both signatures in one pass.

**`Renderer.h`** — update declarations:
```
createImage(uint32_t width, uint32_t height, uint32_t mip_levels,
            vk::SampleCountFlagBits num_samples, vk::Format format,
            vk::ImageTiling tiling, vk::ImageUsageFlags usage,
            vk::MemoryPropertyFlags memory_properties)
    -> std::pair<vk::raii::Image, vk::raii::DeviceMemory>

createImageView(vk::Image image, vk::Format format,
                vk::ImageAspectFlags aspect_flags, uint32_t level_count)
    -> vk::raii::ImageView
```

**`Renderer.cpp`** — update bodies:
- `createImage()`: set `image_info.mipLevels = mip_levels` and `image_info.samples = num_samples`
- `createImageView()`: set `subresource_range.levelCount = level_count`

**Update all existing call sites** (pass explicit values — no defaults):
| Call site | `mip_levels` | `num_samples` |
|-----------|-------------|--------------|
| `createTexture()` — image | calculated value (not yet, see Step 4) | `e1` |
| `createDepthResources()` — image | `1` | `context_.getMsaaSamples()` |
| `createTexture()` — image view | mip_levels (Step 4) | — |
| `createDepthResources()` — image view | `1` | — |

For now pass `1` / `e1` for the texture mip_levels — Step 4 wires the real value.

**Build checkpoint:** compiles; existing behaviour unchanged.

---

### Step 3 — `Renderer`: update `transitionImageLayout()`

**`Renderer.h`** — add `uint32_t level_count` parameter.

**`Renderer.cpp`** — update body: `subresource_range.levelCount = level_count`.

**Update all call sites:**
| Call site | `level_count` |
|-----------|--------------|
| `createTexture()` initial `eUndefined → eTransferDstOptimal` | mip_levels (Step 4 wires this) |
| `createTexture()` second `eTransferDstOptimal → eShaderReadOnlyOptimal` | removed — `generateMipmaps` handles this (Step 4) |
| `createDepthResources()` | `1` |

For now the texture call site temporarily passes `1` — corrected in Step 4.

**Build checkpoint:** compiles; behaviour unchanged.

---

### Step 4 — `Renderer`: implement `generateMipmaps()` and wire `createTexture()`

**`Renderer.h`** — declare:
```
void generateMipmaps(vk::Image image, vk::Format format,
                     uint32_t width, uint32_t height, uint32_t mip_levels)
```

**`Renderer.cpp`** — implement `generateMipmaps()`:
1. Format support check: `physicalDevice.getFormatProperties(format).optimalTilingFeatures` must include `eSampledImageFilterLinear`; throw if not.
2. Begin single-time commands.
3. Loop `i` from `1` to `mip_levels - 1`:
   - Barrier: level `i-1`, `eTransferDstOptimal → eTransferSrcOptimal`
   - `BlitImageInfo2` / `blitImage2`: src = level `i-1`, dst = level `i`, halved offsets, `eLinear` filter
   - Barrier: level `i-1`, `eTransferSrcOptimal → eShaderReadOnlyOptimal`
   - Halve `mip_width` and `mip_height` (clamp to 1)
4. Final barrier: level `mip_levels - 1`, `eTransferDstOptimal → eShaderReadOnlyOptimal`
5. End single-time commands.

**`Texture.h`** — add `uint32_t mipLevels_` member and expose it in the constructor.

**`Renderer.cpp` — update `createTexture()`:**
- After `stbi_load`, calculate: `mip_levels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1`
- Update `createImage()` call: pass `mip_levels`, `e1`, add `eTransferSrc` to usage flags
- Update initial `transitionImageLayout()` call: pass `mip_levels` as `level_count`
- Replace the second `transitionImageLayout()` call (the `eTransferDstOptimal → eShaderReadOnlyOptimal` one) with `generateMipmaps(image, format, width, height, mip_levels)` — the final transition is now inside `generateMipmaps`
- Update `createImageView()` call: pass `mip_levels` as `level_count`
- Update `Texture` construction: pass `mip_levels`

**Build checkpoint:** compiles; mipmaps generated at texture load. No visual change yet (sampler still clamps to level 0).

---

### Step 5 — `Renderer`: update `createSampler()`

**`Renderer.cpp`** — in `createSampler()`:
- `mipmapMode` = `vk::SamplerMipmapMode::eLinear`
- `minLod` = `0.0f`
- `maxLod` = `vk::LodClampNone`
- `mipLodBias` = `0.0f`

**Build checkpoint:** compiles; trilinear filtering active. Visually: mip-level transitions now smooth rather than popping.

---

### Step 6 — `MsaaColorImage`: new struct

**`src/renderer/image_resources/MsaaColorImage.h`**
- Same pattern as `DepthImage`: move-only, `= delete` copy, `= default` move ctor + move assign (Rule of Five)
- Three members: `vk::raii::Image image_`, `vk::raii::DeviceMemory memory_`, `vk::raii::ImageView imageView_`
- Constructor takes all three by move
- `auto getImageView() const -> const vk::raii::ImageView&`

**`src/renderer/image_resources/MsaaColorImage.cpp`**
- Constructor body: member initialiser list with `std::move` on all three

**`CMakeLists.txt`** — add `MsaaColorImage.cpp`

**Build checkpoint:** compiles (nothing calls it yet).

---

### Step 7 — `Renderer`: `createColorResources()`

**`Renderer.h`** — declare:
```
auto createColorResources(vk::Extent2D extent, vk::Format format) -> MsaaColorImage
```
(Sample count is queried from `context_.getMsaaSamples()` internally — not a parameter.)

**`Renderer.cpp`** — implement:
- `createImage(extent.width, extent.height, 1, context_.getMsaaSamples(), format, eOptimal, eTransientAttachment | eColorAttachment, eDeviceLocal)`
- `createImageView(image, format, eColor, 1)`
- Return `MsaaColorImage(std::move(image), std::move(memory), std::move(view))`

**Build checkpoint:** compiles.

---

### Step 8 — `GraphicsPipeline`: MSAA pipeline state + `record()` changes

**`GraphicsPipeline.h`**
- Add `vk::SampleCountFlagBits msaaSamples_` member
- Constructor gains `vk::SampleCountFlagBits msaa_samples` parameter
- `record()` signature gains `vk::ImageView msaa_color_image_view` (placed between `swap_chain_image_view` and `depth_image_view`)

**`GraphicsPipeline.cpp`**
- Constructor initialiser list: store `msaaSamples_`
- `createPipeline()`: set `multisampling.rasterizationSamples = msaaSamples_`
- `record()` — update colour `RenderingAttachmentInfo`:
  - `imageView` = `msaa_color_image_view` (the MSAA render target, not the swap chain)
  - `storeOp` = `eDontCare` (MSAA image is transient — never stored)
  - Add resolve fields:
    - `resolveMode` = `vk::ResolveModeFlagBits::eAverage`
    - `resolveImageView` = `swap_chain_image_view` (the resolve target — what we present)
    - `resolveImageLayout` = `vk::ImageLayout::eColorAttachmentOptimal`

**Build checkpoint:** compile error expected in Application (constructor signature changed) — fix in Step 9.

---

### Step 9 — `Application`: wire everything up

**`Application.h`**
- Add `#include "renderer/image_resources/MsaaColorImage.h"`
- Add `std::unique_ptr<MsaaColorImage> msaaColorImage_` — declare **before** `graphicsPipeline_` and `renderer_` (destroyed after them; must outlive the per-frame resources that reference its image view)

**`Application.cpp` — `initVulkan()`**
- After `swapChain_` construction, create the MSAA colour image:
  ```
  msaaColorImage_ = std::make_unique<MsaaColorImage>(
      renderer_->createColorResources(swapChain_->getExtent(), swapChain_->getFormat()));
  ```
  Wait — `renderer_` is constructed after `graphicsPipeline_`. But `createColorResources` only needs the context (via Renderer internals), not the pipeline. Order:
  1. `swapChain_`
  2. `frameDescriptorLayout_`
  3. `textureDescriptorLayout_`
  4. `graphicsPipeline_` — now takes `context_.getMsaaSamples()` as constructor arg
  5. `renderer_`
  6. `msaaColorImage_` (calls `renderer_->createColorResources(...)`)
  7. `depthImage_` (already calls `renderer_->createDepthResources(...)`)
  8. `texture_` / `mesh_`

- Update `GraphicsPipeline` construction to pass `context_->getMsaaSamples()`

**`Application.cpp` — `onResize()`**
- After `swapChain_->recreate()`, add:
  ```
  msaaColorImage_ = std::make_unique<MsaaColorImage>(
      renderer_->createColorResources(swapChain_->getExtent(), swapChain_->getFormat()));
  ```
- Keep the existing `depthImage_` recreation line immediately after

**`Application.cpp` — `drawFrame()` / `mainLoop()`**
- Thread `msaaColorImage_->getImageView()` through to `renderer_->drawFrame()`, then into `graphics_pipeline.record()` as `msaa_color_image_view`

**Build checkpoint:** full compile and link.

---

## Smoke tests

Run on Windows with validation layers enabled.

| # | What to test | How | Expected |
|---|-------------|-----|---------|
| T1 | Mipmaps generated | Run app, check stdout — no validation errors on texture load | Clean |
| T2 | Trilinear filtering active | Move camera far from model; texture should fade smoothly without Moiré or hard pops | Smooth falloff |
| T3 | MSAA active | Inspect geometric edges (model silhouette) — should be noticeably smoother than before | Anti-aliased edges |
| T4 | Depth occlusion still correct | Two overlapping quads from Ch07 are gone (replaced by viking room) — model self-occlusion correct | No depth artefacts |
| T5 | Resize clean | Drag window; MSAA colour image and depth image both recreated | No crash, no validation errors |
| T6 | Minimise/restore clean | Minimise and restore | No crash |
| T7 | Validation layers silent | Full session with OBS closed | No errors (OBS_HOOK warning harmless) |

---

## Destruction order in `Application`

Declare members in `Application.h` in this order (destroyed in reverse):

```
mesh_                     ← destroyed first
texture_
depthImage_
msaaColorImage_           ← destroyed before renderer (holds image views used by renderer)
renderer_
graphicsPipeline_
textureDescriptorLayout_
frameDescriptorLayout_
swapChain_
context_                  ← destroyed last
```
