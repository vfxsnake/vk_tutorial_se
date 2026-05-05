# Chapter 06: Texture Mapping

> **Source:** https://docs.vulkan.org/tutorial/latest/06_Texture_mapping/00_Images.html
> **Vulkan Version:** 1.4 | **Language:** C++20 | **Shading:** Slang | **Bindings:** Vulkan-Hpp RAII

---

## Overview

Chapter 06 closes the gap between a spinning coloured rectangle and a textured mesh. It introduces three new Vulkan object types — `VkImage`, `VkImageView` (for the texture specifically), and `VkSampler` — and a new descriptor type, `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`, which hands a texture and its sampling configuration to the fragment shader as a single descriptor.

The chapter is larger than it looks. Under the hood it also introduces the concept of **image memory layouts** and the **image memory barrier**, which are the same synchronisation primitive you already used in `transitionImageLayout()` during swap-chain rendering — but now applied to a completely different set of layout transitions. It also refactors the one-shot command-buffer pattern you wrote for `copyBuffer()` into a pair of reusable helpers (`beginSingleTimeCommands` / `endSingleTimeCommands`) that will appear again in depth buffering and mip generation.

After Chapter 06 the rectangle renders with a real image sampled from disk, the vertex struct gains a `texCoord` field, and the fragment shader discards the hardcoded vertex colour in favour of a texture sample.

---

## §06.00 — Images

### Concepts

#### GPU Images vs CPU Pixel Arrays

An image loaded by stb_image on the CPU is simply a flat array of bytes in heap memory. A Vulkan `VkImage` is not that. It is a GPU-side resource descriptor that records the image's dimensions, format, tiling, mip levels, array layers, and usage flags — and is backed by a separately allocated `VkDeviceMemory` block, exactly as `VkBuffer` is. The memory allocation and binding dance is identical to what you did for vertex and uniform buffers: `createImage()` → `getImageMemoryRequirements()` → `findMemoryType()` → `allocateMemory()` → `bindImageMemory()`.

The reason images get their own object type instead of just using buffers is **tiling**. A buffer stores elements linearly, left-to-right, row-by-row (`eLinear` tiling). GPU texture caches are optimised for 2D spatial locality, so the driver is allowed to rearrange the pixels into a hardware-specific swizzled layout called `eOptimal` tiling. `eOptimal` images cannot be mapped to CPU memory and cannot be directly written by the host. That is why images used as textures follow the same staging-buffer pattern as device-local vertex buffers: write the pixels into a host-visible staging buffer, then copy from that buffer into the `eOptimal` image with a transfer command.

#### Image Memory Layouts

Vulkan requires you to explicitly record the *intended use* of an image at every point in its life, because the GPU may reorganise the pixel data in memory to make that use fast. This is an **image memory layout**, and you must transition between layouts with a **pipeline barrier** whenever the intended use changes.

The layouts used in this chapter:

| Layout | Meaning |
|--------|---------|
| `eUndefined` | Initial state after creation. Contents may be discarded on first transition. Use when you do not care about preserving data. |
| `ePreinitialized` | Initial state that *preserves* host-written data on transition. Only useful for linearly-tiled images written by the CPU before the first barrier. |
| `eTransferDstOptimal` | Optimal layout for receiving a buffer-to-image copy. The image may not be sampled in this layout. |
| `eTransferSrcOptimal` | Optimal layout for being read as a copy source. |
| `eShaderReadOnlyOptimal` | Optimal layout for shader sampling. Cannot be written as a transfer destination in this layout. |
| `eColorAttachmentOptimal` | Optimal layout for writing as a colour attachment during rendering (swap-chain images). |
| `ePresentSrcKHR` | Required layout for presentation. |
| `eGeneral` | All operations supported, but may be slower than specialised layouts. |

The texture lifecycle in this chapter has two transitions:
1. `eUndefined → eTransferDstOptimal` — before the `copyBufferToImage` that uploads the pixels.
2. `eTransferDstOptimal → eShaderReadOnlyOptimal` — after the copy, before the shader can read it.

#### Image Memory Barriers

The pipeline barrier you already wrote for swap-chain images (`ImageMemoryBarrier2` + `pipelineBarrier2`) is the same mechanism used for texture layout transitions. The key fields are:

- `oldLayout` / `newLayout` — the transition direction.
- `srcStageMask` / `dstStageMask` — which pipeline stages must complete *before* the barrier (`src`) and which stages wait for it (`dst`).
- `srcAccessMask` / `dstAccessMask` — which memory accesses must be visible across the barrier.

For the two transitions in this chapter the correct masks are:

**`eUndefined → eTransferDstOptimal`**
- `srcStageMask = eTopOfPipe` — nothing precedes this transition; there are no prior operations whose results we need to wait for.
- `dstStageMask = eTransfer` — the transfer command must wait for the barrier.
- `srcAccessMask = {}` — no prior access to flush.
- `dstAccessMask = eTransferWrite` — the copy needs write access.

**`eTransferDstOptimal → eShaderReadOnlyOptimal`**
- `srcStageMask = eTransfer` — the copy must complete before the barrier.
- `dstStageMask = eFragmentShader` — the fragment shader must wait for the barrier.
- `srcAccessMask = eTransferWrite` — flush the transfer write.
- `dstAccessMask = eShaderRead` — the shader needs read access.

Getting these masks wrong will not always crash — `waitIdle()` hides many timing bugs — but it will trigger validation layer errors and may corrupt frames on hardware with less sequential execution.

#### One-Time Command Buffers

The copy and transition operations in this chapter are one-shot GPU commands: allocate a command buffer, record one operation, submit, wait. You already wrote that pattern inline in `copyBuffer()`. Chapter 06 extracts it into two helpers:

- `beginSingleTimeCommands()` — allocates a command buffer from the command pool with `eOneTimeSubmit` flag and calls `begin()`.
- `endSingleTimeCommands(commandBuffer)` — calls `end()`, submits to the graphics queue, and calls `queue.waitIdle()`.

`copyBuffer()` is then refactored to use these helpers instead of repeating the allocation/submit sequence.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::raii::Image` | RAII handle | GPU image object |
| `vk::ImageCreateInfo` | Struct | Image dimensions, format, tiling, usage, sample count, mip/layer counts |
| `vk::Format::eR8G8B8A8Srgb` | Enum value | 8-bit RGBA with sRGB encoding — widely supported, used for colour textures |
| `vk::ImageTiling::eOptimal` | Enum value | Driver-chosen swizzled layout optimised for GPU access |
| `vk::ImageTiling::eLinear` | Enum value | Row-major CPU-readable layout — needed for staging images, rarely for final textures |
| `vk::ImageUsageFlagBits::eTransferDst` | Flag bit | Image will receive a copy from a buffer |
| `vk::ImageUsageFlagBits::eSampled` | Flag bit | Image will be sampled in a shader |
| `vk::ImageLayout` | Enum | Memory layout of an image at a given pipeline stage |
| `vk::ImageMemoryBarrier2` | Struct | Barrier that transitions image layout and synchronises access |
| `vk::BufferImageCopy` | Struct | Specifies a buffer-to-image copy region (offset, subresource, extent) |
| `vkCmdCopyBufferToImage` / `commandBuffer.copyBufferToImage()` | Command | Records a buffer → image copy |
| `stbi_load()` | Function (stb_image) | Loads a PNG/JPG/BMP from disk, returns `unsigned char*` pixel array |
| `stbi_image_free()` | Function (stb_image) | Frees pixel array returned by `stbi_load` |
| `STBI_rgb_alpha` | Constant | Forces stb_image to produce 4-channel RGBA output |

### Code Walkthrough

**Loading the image from disk**
The first step is reading the pixel data from a file on the CPU. `stbi_load()` takes the file path, output pointers for width, height, and actual channel count, and a `STBI_rgb_alpha` constant that forces the output to always be four channels regardless of what the file actually contains. It returns a raw `unsigned char*` pointer to a packed RGBA array of `width × height × 4` bytes. If it returns null, the file was not found or could not be decoded — throw immediately. The pixel pointer must be freed with `stbi_image_free()` after it has been copied into the staging buffer.

**Creating the staging buffer**
Compute `image_size = width × height × 4`. Allocate a staging buffer using the same `createBuffer()` helper from Chapter 04, with `eTransferSrc` usage and `eHostVisible | eHostCoherent` memory properties. Map it, `memcpy` the pixel array into it, unmap it, then call `stbi_image_free()`. The pixel data now lives in GPU-accessible staging memory.

**Creating the GPU image**
The `createImage()` helper takes the width, height, format, tiling, usage flags, and memory properties and returns a `(vk::raii::Image, vk::raii::DeviceMemory)` pair — exactly like `createBuffer()` returns a buffer/memory pair. Internally it fills a `vk::ImageCreateInfo` with `imageType = e2D`, the given extent, one mip level, one array layer, `eSampleCount1Bit`, the given tiling, and the given usage. It then calls `getImageMemoryRequirements()`, `findMemoryType()`, `allocateMemory()`, and `bindImageMemory()`. For the texture image the usage is `eTransferDst | eSampled` and the memory properties are `eDeviceLocal`.

**Transitioning and copying**
With both the staging buffer and the GPU image created, execute two GPU commands via single-time command buffers:
1. Call `transitionImageLayout(image, eUndefined, eTransferDstOptimal)` — this transitions the new image from its initial undefined layout to the layout the copy command requires.
2. Call `copyBufferToImage(staging_buffer, image, width, height)` — this records a `vk::BufferImageCopy` region (zero offset, zero row-length/image-height for tightly-packed data, `eColor` aspect, extent matching the image dimensions) and submits the copy.
3. Call `transitionImageLayout(image, eTransferDstOptimal, eShaderReadOnlyOptimal)` — transitions the image into the layout the fragment shader expects when sampling.

After the three commands finish (`waitIdle()` inside each `endSingleTimeCommands`), the staging buffer can be destroyed.

### Common Pitfalls

- **Wrong barrier masks** — Specifying `srcStageMask = eTopOfPipe` and `dstStageMask = eBottomOfPipe` with empty access masks compiles and often works in simple cases because `waitIdle()` covers the gap, but it is validation-layer incorrect and will fail on tighter GPU timelines. Always set the minimum-correct masks described above.
- **Forgetting `eTransferDst` on the image usage** — without it, `copyBufferToImage` is valid-layer invalid even though it may appear to work.
- **Forgetting `eSampled` on the image usage** — without it, binding the image to a sampler descriptor produces a validation error.
- **`stbi_image_free()` called before `memcpy` finishes** — impossible if `memcpy` is synchronous (it always is on the CPU), but worth keeping in mind if you later move the copy to async.
- **`imageSize` computed as `width * height * 3`** — stb_image with `STBI_rgb_alpha` always returns four bytes per pixel regardless of the source file's channel count. Using `3` here causes a short buffer and undefined behaviour.

---

## §06.01 — Image View & Sampler

### Concepts

#### The Texture Image View

Shaders never access `VkImage` handles directly — they go through `VkImageView`, which specifies the format to interpret the pixels with, the view type (`e2D`, `eCube`, etc.), and the subresource range (which mip levels and array layers are visible). You already created image views for the swap-chain images in `SwapChain::createImageViews()`. The texture gets the same treatment.

In the tutorial the texture view creation is similar enough to the swap-chain view creation that the code is immediately refactored into a shared `createImageView(image, format)` helper, and `SwapChain::createImageViews()` is updated to use it. In our modular layout we should follow the same principle — a `createImageView` utility that both `SwapChain` and the texture code can call.

#### The Sampler Object

The sampler is Vulkan's mechanism for controlling how texture reads are performed in shaders. It is a **separate object** from the image and the image view — the same sampler can be applied to any number of images, and the same image can be sampled with different samplers simultaneously. The sampler records:

**Filtering** — how the GPU interpolates between texels when a fragment maps to a position between two texels, or when one fragment covers many texels:
- *Magnification filter* (`magFilter`) — used when the texture is enlarged so that a texel covers multiple fragments (oversampling). `eLinear` produces smooth bilinear interpolation; `eNearest` produces blocky nearest-neighbour sampling.
- *Minification filter* (`minFilter`) — used when the texture is shrunk so that a fragment covers multiple texels (undersampling). Without mipmaps, high-frequency aliasing appears at small scales. `eLinear` reduces this.
- *Mipmap mode* (`mipmapMode`) — controls blending between mip levels. Set to `eLinear` for smooth mip transitions.

**Addressing modes** — what happens when texture coordinates fall outside the `[0, 1)` range:
- `eRepeat` — tiles the texture; coordinates wrap around.
- `eMirroredRepeat` — tiles with alternating mirrors.
- `eClampToEdge` — extends the border pixel.
- `eClampToBorder` — fills with a solid colour (set via `borderColor`).
- `eMirrorClampToEdge` — mirrors once, then clamps.

All three axes (U, V, W) are set independently. For a 2D texture only U and V matter.

**Anisotropic filtering** — when a surface is viewed at a steep angle, the sampling footprint of each fragment becomes elongated along one axis (anisotropic means "direction-dependent"). Isotropic filtering (plain `eLinear`) uses a circular footprint and produces blur in this case; anisotropic filtering uses an elongated footprint that follows the angle, dramatically improving texture quality on slanted surfaces. It is an optional device feature, must be enabled during `createLogicalDevice()`, and its maximum quality level is queried from `physicalDevice.getProperties().limits.maxSamplerAnisotropy`.

**Coordinate system** — `unnormalizedCoordinates = false` means the shader supplies coordinates in the `[0, 1)` range (normalised, resolution-independent). Setting it to `true` requires the shader to supply coordinates in `[0, width)` and `[0, height)` pixel space — rarely used.

**Compare operations** — `compareEnable` and `compareOp` are used for shadow map percentage-closer filtering. Leave them disabled for now.

**Mip LOD** — `minLod`, `maxLod`, `mipLodBias` control which mip level is selected. Set all to zero for now; the mipmapping chapter will revisit these.

One critically non-obvious aspect of the sampler: it contains **no reference to any image or image view**. It is a pure configuration object. The binding of a specific image to a specific sampler happens at descriptor-write time, not sampler-creation time.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::raii::Sampler` | RAII handle | Sampler configuration object |
| `vk::SamplerCreateInfo` | Struct | All sampler parameters |
| `vk::Filter` | Enum | `eNearest` or `eLinear` — for mag/min filtering |
| `vk::SamplerMipmapMode` | Enum | `eNearest` or `eLinear` — mip level blending |
| `vk::SamplerAddressMode` | Enum | Behaviour outside `[0,1)`: `eRepeat`, `eMirroredRepeat`, `eClampToEdge`, `eClampToBorder`, `eMirrorClampToEdge` |
| `vk::BorderColor` | Enum | Colour returned outside bounds when using `eClampToBorder` |
| `vk::PhysicalDeviceFeatures` | Struct | Contains `samplerAnisotropy` boolean — check this before enabling |
| `physicalDevice.getProperties().limits.maxSamplerAnisotropy` | Field | Maximum anisotropy samples supported by this GPU |

### Code Walkthrough

**Refactoring to `createImageView()`**
Before writing texture-specific code, extract the `ImageViewCreateInfo` setup from `SwapChain::createImageViews()` into a standalone helper that takes an image handle and a format and returns a `vk::raii::ImageView`. The texture image view then calls this helper with the texture image and `eR8G8B8A8Srgb`.

**Creating the sampler**
Fill `vk::SamplerCreateInfo` with the following choices for this chapter: both `magFilter` and `minFilter` as `eLinear`, `mipmapMode` as `eLinear`, all three address modes as `eRepeat`, `anisotropyEnable = vk::True`, `maxAnisotropy` queried from the physical device limits, `borderColor = eIntOpaqueBlack` (unused with `eRepeat`, but must be set), `unnormalizedCoordinates = vk::False`, `compareEnable = vk::False`, `compareOp = eAlways`, and all mip LOD fields at `0.0f`. Construct `vk::raii::Sampler(context_.getLogicalDevice(), samplerInfo)`.

**Enabling anisotropic filtering on the device**
Return to `VulkanContext::isDeviceSuitable()` and add a check that `physicalDevice.getFeatures().samplerAnisotropy` is `VK_TRUE`. Return to `VulkanContext::createLogicalDevice()` and add `samplerAnisotropy = vk::True` to the `PhysicalDeviceFeatures` in the feature chain.

### Common Pitfalls

- **Anisotropy enabled in the sampler but not requested in `createLogicalDevice()`** — the validation layer will catch this at sampler creation time with an error about `VkPhysicalDeviceFeatures::samplerAnisotropy`.
- **Anisotropy enabled but device does not support it** — `isDeviceSuitable()` must check the feature before selecting the device, otherwise `maxSamplerAnisotropy` returns `1` and enabling anisotropy is a spec violation.
- **`SamplerCreateInfo` fields not initialised** — unlike buffer creation, sampler creation has many fields and no "zero is a sensible default" guarantee. Every field should be set explicitly with designated initialisers.

---

## §06.02 — Combined Image Sampler

### Concepts

#### The Combined Image Sampler Descriptor

Until now the descriptor set has held one binding: binding 0, a `eUniformBuffer` descriptor consumed by the vertex shader. Chapter 06 adds a second binding: binding 1, a `eCombinedImageSampler` descriptor consumed by the fragment shader.

The combined image sampler packages a `VkImageView` and a `VkSampler` into a single descriptor. The alternative is to use separate `eStoredImage` and `eSampler` descriptors at two different bindings, which gives more flexibility (one sampler with many images, or one image with many samplers). For a single texture the combined form is simpler and may be faster due to cache locality.

Every time you add a new descriptor type to the layout, you must update three places:
1. **`DescriptorSetLayout`** — add a new `DescriptorSetLayoutBinding` at the new binding index.
2. **`DescriptorPool`** — add a new `DescriptorPoolSize` for the new type.
3. **`updateDescriptorSets`** — add a new `WriteDescriptorSet` referencing a `DescriptorImageInfo`.

The `DescriptorImageInfo` struct (analogous to `DescriptorBufferInfo` for buffers) takes three fields: the `sampler` handle, the `imageView` handle, and the `imageLayout` at the time the descriptor will be used. For shader sampling the layout must be `eShaderReadOnlyOptimal` — this is the layout you transitioned the image to at the end of §06.00.

#### Texture Coordinates

The existing `Vertex` struct has two fields: `glm::vec2 pos` and `glm::vec3 color`. A third field `glm::vec2 texCoord` must be added. This requires updating:
- The `Vertex` struct definition.
- `getAttributeDescriptions()` — a third `VertexInputAttributeDescription` at location 2, format `eR32G32Sfloat`, offset `offsetof(Vertex, texCoord)`.
- The hardcoded vertex array in `Application` — each vertex gains a UV pair.

Texture coordinates (UVs) are normalised: `(0, 0)` is conventionally the top-left of the image, `(1, 1)` is the bottom-right. In Vulkan's framebuffer space, Y increases downward — so `(0, 0)` in UV space maps to the top-left of the texture and `(0, 1)` maps to the bottom-left. For a full-rectangle mapping the four corners are assigned:
- Top-left vertex: `(1, 0)`
- Top-right vertex: `(0, 0)`
- Bottom-right vertex: `(0, 1)`
- Bottom-left vertex: `(1, 1)`

The tutorial's specific UV assignments place a texture corner at each rectangle corner in a way that makes the gradient visible and correctly oriented.

#### Shader Changes

**Vertex shader** gains a `float2 inTexCoord` input at location 2, adds `float2 fragTexCoord` to its output struct, and passes the coordinate through unchanged. The GPU interpolates `fragTexCoord` across the triangle surface between vertices.

**Fragment shader** gains a `Sampler2D` resource declaration bound at binding 1 and calls `.Sample(fragTexCoord)` on it. In Slang, `Sampler2D` is a combined image sampler type — it represents both the `VkImageView` and the `VkSampler` packed together. The return type of `.Sample()` is `float4` (RGBA). Multiply the sampled colour by the vertex colour to produce a tinted texture, or use the sample directly to display the raw texture.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::DescriptorType::eCombinedImageSampler` | Enum value | Combined image+sampler descriptor type |
| `vk::DescriptorImageInfo` | Struct | Binds a sampler + image view + layout into a descriptor write |
| `vk::WriteDescriptorSet::pImageInfo` | Field | Pointer to `DescriptorImageInfo` — used instead of `pBufferInfo` for image descriptors |
| `Sampler2D` | Slang type | Combined 2D image sampler; maps to `eCombinedImageSampler` at binding N |
| `.Sample(uv)` | Slang method | Samples the texture at normalised UV coordinates, returns `float4` |

### Code Walkthrough

**Updating the descriptor set layout**
The `FrameDescriptorLayout` class currently creates a single binding for the UBO. It now needs a second binding for the combined image sampler. The two bindings are passed to `DescriptorSetLayoutCreateInfo` as an array (or two-element `std::array`). Binding 1 sets `descriptorType = eCombinedImageSampler`, `stageFlags = eFragment`, `descriptorCount = 1`, and `pImmutableSamplers = nullptr`.

**Updating the descriptor pool**
The pool currently has one `DescriptorPoolSize`: `{eUniformBuffer, MAX_FRAMES_IN_FLIGHT}`. Add a second entry: `{eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT}`. Update `DescriptorPoolCreateInfo::poolSizeCount` and `pPoolSizes` accordingly.

**Updating the descriptor set writes**
In `initializeFrameData()` (or wherever descriptor sets are updated), add a second `WriteDescriptorSet` for each slot. Fill a `DescriptorImageInfo` with the sampler handle, the texture image view handle, and layout `eShaderReadOnlyOptimal`. Set `WriteDescriptorSet::dstBinding = 1`, `descriptorType = eCombinedImageSampler`, `descriptorCount = 1`, and point `pImageInfo` at the info struct. Pass both writes (UBO and image sampler) to `updateDescriptorSets` in one call.

**Updating the Vertex struct**
Add `glm::vec2 texCoord` as a third member after `glm::vec3 color`. Add a third `VertexInputAttributeDescription` at location 2, format `eR32G32Sfloat`, offset `offsetof(Vertex, texCoord)`.

**Updating the vertex data**
Replace the four rectangle vertices with new definitions that include UV pairs alongside position and colour.

**Updating the shaders**
In `triangle.slang`: add `float2 inTexCoord` at location 2 to `VSInput`, add `float2 fragTexCoord` to `VSOutput`, pass through in the vertex shader body. In the fragment shader, declare a `Sampler2D` resource at `[[vk::binding(1)]]` and call `.Sample(input.fragTexCoord)` on it. Replace the colour return with the sample result (or multiply by `fragColor` for a tinted effect).

### Common Pitfalls

- **Pool size not updated for the new descriptor type** — `vkAllocateDescriptorSets` returns `VK_ERROR_POOL_OUT_OF_MEMORY` (or worse, succeeds silently on some hardware) if you allocate a descriptor type not listed in any `DescriptorPoolSize`. The validation layer will catch this.
- **`DescriptorImageInfo.imageLayout` not set to `eShaderReadOnlyOptimal`** — the layout field must match the actual layout the image is in at draw time. If you forgot to transition the image after the copy (§06.00 step 3), the image is still `eTransferDstOptimal` and sampling it is undefined behaviour.
- **`pImageInfo` vs `pBufferInfo`** — `WriteDescriptorSet` has separate pointer fields for different descriptor categories. Filling `pBufferInfo` for an image descriptor compiles but produces a validation error and a black texture.
- **Vertex attribute count mismatch** — if `getAttributeDescriptions()` returns two entries but the shader declares three input locations, vertex fetch is implementation-defined on the missing attribute (usually zero, but validation layers will warn).
- **UV coordinates assigned in the wrong order** — if the texture appears flipped horizontally or vertically, the UV assignments at the vertices are swapped. The fix is to adjust the UV values in the vertex array, not to change the image or the sampler.
- **`Sampler2D` not bound** — if the descriptor set layout has binding 1 but the shader uses `[[vk::binding(0)]]` for it, the shader reads from the UBO binding, producing garbage colour values. Match binding indices carefully between the layout and the shader annotations.

---

## Summary

- Vulkan images are GPU resource descriptors backed by separately-allocated `VkDeviceMemory`, just like buffers.
- `eOptimal` tiling makes GPU sampling fast but prevents CPU-side mapping; use a staging buffer to upload pixel data.
- Image memory layouts must be explicitly managed via pipeline barriers (`ImageMemoryBarrier2`). The texture lifecycle requires two transitions: `eUndefined → eTransferDstOptimal` before the copy, and `eTransferDstOptimal → eShaderReadOnlyOptimal` after it.
- `beginSingleTimeCommands` / `endSingleTimeCommands` extract the one-shot command pattern into reusable helpers; `copyBuffer` is refactored to use them.
- A `VkSampler` is a pure configuration object — no image reference — that controls filtering, addressing, and anisotropy. It is separate from the image and image view.
- The combined image sampler descriptor (`eCombinedImageSampler`) bundles a sampler and image view at a single binding index, consumed in the fragment shader as `Sampler2D`.
- Adding the combined image sampler requires updating the descriptor set layout, the descriptor pool, and the descriptor set write operations — all three must be in sync.
- The `Vertex` struct gains a `glm::vec2 texCoord` field; `getAttributeDescriptions()` gains a third entry at location 2.
- The fragment shader switches from vertex colour to `texture.Sample(uv)` using the Slang `Sampler2D` type.

## Implementation Checklist

- [ ] Add `stb_image.h` include; add texture image path to the project (`textures/texture.jpg`)
- [ ] Implement `beginSingleTimeCommands()` helper
- [ ] Implement `endSingleTimeCommands(commandBuffer)` helper
- [ ] Refactor `copyBuffer()` to use the two helpers
- [ ] Implement `createImage(width, height, format, tiling, usage, memProps)` helper → returns `(Image, DeviceMemory)` pair
- [ ] Implement `transitionImageLayout(image, oldLayout, newLayout)` — correct barrier masks for both transitions
- [ ] Implement `copyBufferToImage(buffer, image, width, height)`
- [ ] Implement `createTextureImage()` — load pixels, staging buffer, create GPU image, transition, copy, transition, destroy staging
- [ ] Extract `createImageView(image, format)` helper; refactor `SwapChain::createImageViews()` to use it
- [ ] Implement `createTextureImageView()`
- [ ] Enable `samplerAnisotropy` in `VulkanContext::isDeviceSuitable()` and `createLogicalDevice()`
- [ ] Implement `createTextureSampler()` — linear filtering, repeat addressing, anisotropy enabled
- [ ] Update `FrameDescriptorLayout` — add binding 1 (`eCombinedImageSampler`, `eFragment`)
- [ ] Update `createDescriptorPool()` — add `DescriptorPoolSize` for `eCombinedImageSampler`
- [ ] Update descriptor set writes — add `DescriptorImageInfo` + `WriteDescriptorSet` at binding 1
- [ ] Update `Vertex` struct — add `glm::vec2 texCoord`; update `getAttributeDescriptions()` with location 2
- [ ] Update vertex data in `Application` — four vertices with UV pairs
- [ ] Update `shaders/triangle.slang` — `VSInput` location 2, `VSOutput` `fragTexCoord`, `Sampler2D` at binding 1, `.Sample()` in fragment shader
- [ ] Recompile shader
- [ ] Full build and run — texture visible on spinning rectangle, validation layers silent

## Further Reading

- [Vulkan Spec — VkImage](https://docs.vulkan.org/spec/latest/chapters/resources.html#VkImage)
- [Vulkan Spec — Image Layouts](https://docs.vulkan.org/spec/latest/chapters/resources.html#resources-image-layouts)
- [Vulkan Spec — Pipeline Barriers](https://docs.vulkan.org/spec/latest/chapters/synchronization.html#synchronization-pipeline-barriers)
- [Vulkan Spec — VkSampler](https://docs.vulkan.org/spec/latest/chapters/samplers.html)
- [Vulkan Spec — Descriptor Types](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html#descriptorsets-types)
- [stb_image library](https://github.com/nothings/stb/blob/master/stb_image.h)
