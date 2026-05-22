# Chapters 09 & 10: Generating Mipmaps + Multisampling

> **Sources:**
> - https://docs.vulkan.org/tutorial/latest/09_Generating_Mipmaps.html
> - https://docs.vulkan.org/tutorial/latest/10_Multisampling.html
> **Vulkan Version:** 1.4 | **Language:** C++20 | **Shading:** Slang | **Bindings:** Vulkan-Hpp RAII

---

## Overview

These two chapters are combined because they address the same concern — image quality — and share a tight implementation coupling. Both require changes to `createImage()`, both affect the depth image, and together they complete the "quality pipeline" before you move on to more advanced features.

- **Chapter 09** teaches how to generate and use mipmaps: pre-downscaled image copies that eliminate Moiré aliasing on distant textures.
- **Chapter 10** teaches MSAA (Multisample Anti-Aliasing): multi-sample rendering that eliminates geometric jagged edges.

After both chapters your renderer renders at full quality using multi-sampled mipmapped textures.

---

## Chapter 09 — Generating Mipmaps

### Concepts

A mipmap chain is a sequence of pre-downscaled versions of a texture, where each level is exactly half the width and half the height of the previous one. The GPU automatically picks the right level when sampling based on how many screen pixels the texture footprint covers. At large distances (small screen footprint) it picks a small mip level — cheaper to sample, fewer cache misses, no Moiré aliasing. At close range it picks mip level 0 (the full-resolution image).

Mipmaps solve two problems:
- **Aliasing (Moiré patterns):** When a high-res texture is mapped onto a small screen area, many texels map to one pixel. Without mipmaps the GPU samples one arbitrary texel — nearby frames sample different texels, producing flickering patterns.
- **Performance:** Sampling from a smaller mip level has better cache hit rates. This is a meaningful GPU performance win for textured scenes.

The mip level count is calculated as:

```
mip_levels = floor(log2(max(width, height))) + 1
```

The `+1` accounts for the base image (level 0). A 512×512 texture has 10 levels (512 → 256 → 128 → ... → 1).

### How Vulkan generates mipmaps at runtime

Vulkan does not generate mip levels automatically. You must do it yourself using `vkCmdBlitImage`. A blit is a full image copy with optional scaling and filtering — using a linear filter on successive halvings gives a clean Lanczos-like result.

The generation loop works as follows (for each mip level `i` starting from 1):

1. Transition level `i-1` from `eTransferDstOptimal` → `eTransferSrcOptimal` (we finished writing to it; now read from it).
2. Record a `BlitImageInfo2` that copies level `i-1` → level `i`, scaling down by 2 in both dimensions, using `eLinear` filter.
3. Transition level `i-1` from `eTransferSrcOptimal` → `eShaderReadOnlyOptimal` (this level is done forever — hand it to the shader stage).
4. Halve the working width and height, clamped to 1 (never let dimensions reach 0).

After the loop, the final level was never used as a blit source, so it is still in `eTransferDstOptimal`. It requires one final explicit transition to `eShaderReadOnlyOptimal`.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vkCmdBlitImage2` / `blitImage2` | Command | Records a scaled image copy between two image regions |
| `vk::BlitImageInfo2` | Struct | Wraps source image, dest image, regions, and filter |
| `vk::ImageBlit2` | Struct | Defines src/dst subresource + src/dst offset pairs (the two corners of the blit rectangle) |
| `vk::Filter::eLinear` | Enum | Bilinear filtering during the blit downsample |
| `vk::ImageSubresourceRange::levelCount` | Field | How many mip levels a barrier or view covers |
| `vkGetPhysicalDeviceFormatProperties` | Function | Query which features (linear tiling, optimal tiling) a format supports |
| `VkFormatFeatureFlagBits::eSampledImageFilterLinear` | Flag | Confirms the format supports linear filtering during blit |
| `vk::LodClampNone` | Constant | Sets `maxLod` to effectively unlimited, exposing the full mip chain to the sampler |

### Important constraints

**Blit requires a graphics queue.** `vkCmdBlitImage` is not part of the transfer-only queue family capability set. Your queue must have the graphics flag. In our architecture this is already satisfied — we use the single graphics queue for all commands.

**Format must support linear filtering.** Not every format supports linear filtering in `optimalTilingFeatures`. Before generating mipmaps, query `physicalDevice.getFormatProperties(format)` and check that `optimalTilingFeatures` includes `eSampledImageFilterLinear`. Throw if it does not — the alternative is a software resize fallback (out of scope here).

**`createImage()` must expose mip level count.** The `VkImage` is created with `mipLevels` set to the full chain count, and the usage flags must include both `eTransferSrc` and `eTransferDst` (in addition to `eSampled`): `eTransferSrc` is needed because each mip level will be used as the blit source when generating the next level.

**`createImageView()` must set `levelCount` correctly.** Views must cover the full mip chain — set `subresourceRange.levelCount = mip_levels`, not hardcoded 1.

**`transitionImageLayout()` initial transition must cover all levels.** The first `eUndefined → eTransferDstOptimal` transition (before the staging copy) must cover `levelCount = mip_levels`, not 1, so that every level starts in the correct layout for the generation loop.

### Sampler changes

The sampler needs updating to expose the mip chain to the shader:

| Field | Old value | New value | Why |
|-------|-----------|-----------|-----|
| `mipmapMode` | `eNearest` | `eLinear` | Blend between adjacent mip levels (trilinear filtering) |
| `minLod` | `0.0f` | `0.0f` | Allow access to the finest level |
| `maxLod` | `0.0f` | `vk::LodClampNone` | Allow access to all levels |
| `mipLodBias` | `0.0f` | `0.0f` | No artificial bias |

With `maxLod = 0.0f` the sampler is forced to always use mip level 0 regardless of distance — mipmapping is effectively disabled even if the chain exists. Setting it to `vk::LodClampNone` opens the full chain.

### Common Pitfalls

- **`eTransferSrc` missing from texture usage flags.** The generate loop blits *from* each mip level after writing it. If `eTransferSrc` is absent, the validation layer fires on the first blit.
- **`levelCount = 1` in the initial barrier.** The staging buffer copy only writes level 0. If the initial `eUndefined → eTransferDstOptimal` barrier only covers level 0, every other level stays in `eUndefined` when the generation loop tries to write to it — undefined behaviour / validation error.
- **Zero-dimension mip levels.** For non-square textures one dimension reaches 1 before the other. Always clamp: `mip_width = mip_width > 1 ? mip_width / 2 : 1`.
- **Forgetting the final level.** The last mip level exits the loop still in `eTransferDstOptimal`. A missing final transition causes a shader-read validation error.
- **Pre-generated mipmaps in DDS/KTX files.** Runtime blit generation is only needed when loading raw PNGs or JPEGs. If you later move to KTX2 format (Chapter 15), mip levels are baked into the file and you skip `generateMipmaps()` entirely.

---

## Chapter 10 — Multisampling (MSAA)

### Concepts

MSAA fixes geometric aliasing — the "jagged edges" (staircasing) visible on diagonal or curved polygon boundaries at pixel scale. The root cause: the rasterizer tests whether a pixel centre lies inside a triangle, producing a hard 0 or 1 coverage decision. On boundaries this looks stepped.

With MSAA, the rasterizer evaluates `N` sample points per pixel (arranged in a hardware-defined pattern). The fragment shader runs once per pixel (not once per sample — this is the efficiency win), but the coverage mask combines the N sample results. Pixels fully inside a triangle get full contribution; pixels on the boundary get a fractional contribution proportional to how many samples fall inside. The per-sample colour values are then resolved (averaged) into a single pixel colour.

MSAA improves geometric edge quality but does not fix aliasing *within* a polygon (texture aliasing, specular aliasing). Mipmaps address texture aliasing; specular aliasing requires more advanced techniques (TAA, SMAA).

**Sample shading** (`sampleShadingEnable = true`, `minSampleShading`) is an optional extension that also runs the fragment shader per sample instead of per pixel. This fixes intra-polygon aliasing (e.g., a fine-detail normal map flickering), at the cost of running the shader N times per pixel — a significant performance hit. Treat it as a quality knob for high-end configurations.

### Hardware sample count query

The GPU hardware has a maximum MSAA sample count for colour attachments and a separate maximum for depth attachments. You must use the lower of the two — a common result on desktop GPUs is 8× or 16×. The query is:

```
auto properties = physicalDevice.getProperties();
auto counts = properties.limits.framebufferColorSampleCounts
            & properties.limits.framebufferDepthSampleCounts;
```

Then walk down from `e64` to `e1` checking the bitmask. The first value present is the max usable count.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::SampleCountFlagBits` | Enum | Represents 1×, 2×, 4×, 8×, 16×, 32×, 64× sample counts |
| `VkImageCreateInfo::samples` | Field | Number of samples per texel for an image |
| `VkPipelineMultisampleStateCreateInfo` | Struct | Sets rasterization sample count + optional sample shading |
| `rasterizationSamples` | Field | Must match the sample count of the colour attachment |
| `sampleShadingEnable` | Field | Enables per-sample shading (optional quality upgrade) |
| `minSampleShading` | Field | Fraction of samples that run the shader (1.0 = all) |
| `resolveMode` | Field (RenderingAttachmentInfo) | How multi-sample data is resolved (use `eAverage`) |
| `resolveImageView` | Field (RenderingAttachmentInfo) | The single-sample image that receives the resolved result |
| `resolveImageLayout` | Field (RenderingAttachmentInfo) | Layout of the resolve target during rendering |

### How dynamic rendering handles MSAA

Because we use dynamic rendering (no render pass objects), MSAA is handled entirely through the attachments passed to `vk::RenderingInfo` at draw time:

**Colour attachment:**
- `imageView` = MSAA colour image view (the multi-sample render target — **not** the swap chain image)
- `imageLayout` = `eColorAttachmentOptimal`
- `loadOp` = `eClear`, `storeOp` = `eDontCare` (MSAA images are transient — GPU never needs to write them to memory)
- `resolveMode` = `eAverage`
- `resolveImageView` = swap chain image view (the single-sample resolve target)
- `resolveImageLayout` = `eColorAttachmentOptimal`

**Depth attachment:**
- `imageView` = MSAA depth image view (depth must use the same sample count as colour)
- Everything else unchanged

The MSAA colour image is a transient GPU-only resource: usage flags `eTransientAttachment | eColorAttachment`, single mip level, sample count = `msaaSamples`. It is never sampled from, never transferred — it lives entirely on the GPU tile memory during a render pass and is discarded after. This is why `storeOp = eDontCare` is correct and important for performance on mobile/tile-based GPUs.

The swap chain image is now the *resolve target*, not the direct colour output. The layout transitions in `record()` still apply to the swap chain image, because it still needs to be in the right layout before the GPU writes the resolved result into it.

### `PipelineRenderingCreateInfo` with MSAA

With dynamic rendering and MSAA, the pipeline's rendering info does **not** change — it still specifies the colour attachment format and depth format. The sample count is set in `VkPipelineMultisampleStateCreateInfo`. Both must be consistent:
- `PipelineMultisampleStateCreateInfo::rasterizationSamples` = `msaaSamples`
- The colour image passed at draw time must have the same sample count

### Common Pitfalls

- **MSAA image used as texture source.** Multi-sampled images cannot be used with `eSampled` — do not add `eSampled` to the MSAA colour image usage flags.
- **Mipmap on MSAA image.** MSAA images always have `mipLevels = 1`. There is no mipmap chain on a transient render target.
- **Depth image sample count mismatch.** If the depth image is created with `e1` while the colour image is created with `e4`, the validation layer fires at draw time. The depth image must match the colour sample count.
- **`storeOp = eStore` on MSAA image.** Storing a multi-sample image wastes memory bandwidth and breaks tile-based GPU optimisations. Always use `eDontCare`.
- **`rasterizationSamples` mismatch.** The pipeline must be created with the same `rasterizationSamples` that will be used at draw time. If you recreate the swap chain but not the pipeline, and the sample count changed (it won't in practice, but if you query a new device…), you get a mismatch.
- **`sampleShadingEnable` without device feature.** `sampleRateShading` must be enabled in the logical device features before enabling sample shading in the pipeline. Check `physicalDeviceFeatures.sampleRateShading` during device selection.

---

## Interaction Between the Two Chapters

Both chapters touch `createImage()`. Implementing them together means one clean final signature:

```
createImage(width, height, mip_levels, num_samples, format, tiling, usage, memory_properties)
             ^Ch09            ^Ch10
```

Every call site must pass both. The combinations are:

| Image | `mip_levels` | `num_samples` |
|-------|-------------|--------------|
| Texture | calculated from dimensions | `e1` |
| MSAA colour | `1` | `msaaSamples` |
| Depth | `1` | `msaaSamples` |

Similarly `createImageView()` needs to know `level_count` for the subresource range, and for `transitionImageLayout()` the subresource range must cover the right number of levels.

---

## Summary

**After Chapter 09:**
- Textures have a full mip chain generated on the GPU at load time
- The sampler uses trilinear filtering across mip levels
- Distant geometry samples smaller mip levels — no Moiré, better cache performance

**After Chapter 10:**
- Rendering uses an intermediate MSAA colour image (and MSAA depth image)
- The pipeline rasterises with N samples per pixel
- At the end of each frame the multi-sample data is resolved into the swap chain image
- Geometric edges are smooth

## Implementation Checklist (combined)

**Chapter 09 — Mipmaps:**
- [ ] `Renderer::createImage()` gains `mip_levels` parameter
- [ ] `Renderer::createImageView()` gains `level_count` parameter
- [ ] `Renderer::transitionImageLayout()` updated to use `level_count` in subresource range
- [ ] `Renderer::createTexture()` calculates `mip_levels`, passes to `createImage`/`createImageView`, updates usage flags to include `eTransferSrc`
- [ ] `Renderer::generateMipmaps(image, format, width, height, mip_levels)` implemented — format check, blit loop, final transition
- [ ] `Renderer::createSampler()` updated — `mipmapMode = eLinear`, `maxLod = vk::LodClampNone`
- [ ] `Texture` stores `mipLevels_` member

**Chapter 10 — MSAA:**
- [ ] `VulkanContext::getMaxUsableSampleCount()` implemented
- [ ] `Renderer::createImage()` gains `num_samples` parameter
- [ ] `MsaaColorImage` (or similar) move-only struct — `Image`, `Memory`, `ImageView`
- [ ] `Renderer::createColorResources(extent, format, samples)` factory
- [ ] `GraphicsPipeline` constructor gains `vk::SampleCountFlagBits msaa_samples` parameter
- [ ] `GraphicsPipeline::createPipeline()` sets `rasterizationSamples = msaaSamples_`
- [ ] `GraphicsPipeline::record()` gains MSAA colour image view param; colour `RenderingAttachmentInfo` sets resolve fields
- [ ] `Application` queries `msaaSamples`, owns `msaaColorImage_`, recreates on resize
- [ ] Depth image creation updated — passes `msaaSamples` to `createDepthResources()`

## Further Reading
- [Vulkan Spec — VkImageBlit2](https://registry.khronos.org/vulkan/specs/latest/man/html/VkImageBlit2.html)
- [Vulkan Spec — VkPipelineMultisampleStateCreateInfo](https://registry.khronos.org/vulkan/specs/latest/man/html/VkPipelineMultisampleStateCreateInfo.html)
- [Vulkan Spec — VkRenderingAttachmentInfo (resolveMode)](https://registry.khronos.org/vulkan/specs/latest/man/html/VkRenderingAttachmentInfo.html)
- [Sascha Willems — MSAA sample](https://github.com/SaschaWillems/Vulkan/blob/master/examples/multisampling)
