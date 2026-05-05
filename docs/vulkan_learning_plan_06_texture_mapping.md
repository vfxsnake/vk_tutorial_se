# Learning Plan — Chapter 06: Texture Mapping

> **Estimated total time:** 4 sessions × ~1.5h avg = ~6h
> **Prerequisites:** Chapter 05 complete (rotating rectangle, silent validation layers). Implementation plan `vulkan_implementation_plan_06_texture_mapping.md` reviewed and accepted.

> **⚠️ Architecture discussion overlap.** The Ch06 architecture discussion (session 43) resolved all structural decisions: Texture class ownership, two-set design, `TextureDescriptorLayout` as sibling, `bindTextureToDescriptor` semantics, vertex wire-format cascade. These are marked ✅ — read your session log answers rather than re-deriving from scratch.

---

## Chapter Milestones Overview

| # | Milestone | Session Type | Est. Time |
|---|-----------|--------------|-----------|
| M1 | Texture Resource — image, sampler, GPU helpers | Theory + Implementation | ~3h total |
| M2 | Texture Sampling — descriptor wiring, vertex UV, shader | Theory (light) + Implementation | ~3h total |

---

## Milestone M1 — Texture Resource

> **Goal:** Understand image memory layouts and barrier synchronisation. Implement all GPU helpers, the `Texture` class, `Renderer::createTexture()`, and `VulkanContext` anisotropy support. At the end of M1 the texture exists on the GPU and the descriptor set points to it — the shader does not yet use it.
> **Source:** §06.00 Images, §06.01 Image View & Sampler

---

### Session A — Theory

> **Estimated time:** ~1h
> **Session Checklist:**
> - [ ] Read §06.00 and §06.01 in `docs/vulkan_chapter_06_texture_mapping.md` in full
> - [ ] Answer all fresh Comprehension Questions below
> - [ ] Sketch (or describe in words) the full upload sequence for a texture: which transitions happen, in what order, and what barrier masks are used at each step

#### Comprehension Questions

**Awareness — Why does this exist?**

1. ✅ *Why device-local images need staging and why `Texture` owns all four handles — covered in architecture session 43.*

2. **(fresh)** `VkImage` exists as a separate object type from `VkBuffer` even though both are backed by `VkDeviceMemory`. Read §06.00 "GPU Images vs CPU Pixel Arrays" and answer: what specifically about `eOptimal` tiling makes it impossible to write pixel data directly from the CPU, and why does the GPU use `eOptimal` at all despite that restriction?

3. **(fresh)** Image memory layouts represent the GPU's *intended use* of a texture at a given moment. Why does the GPU care about this at all — what does it actually do differently between `eTransferDstOptimal` and `eShaderReadOnlyOptimal` that justifies the mandatory transition between them?

**Conceptual — What is it and how does it work?**

4. **(fresh — the core barrier question)** A pipeline barrier needs four synchronisation fields: `srcStageMask`, `dstStageMask`, `srcAccessMask`, `dstAccessMask`. For the first transition (`eUndefined → eTransferDstOptimal`) the implementation plan specifies `srcStageMask = eTopOfPipe` and `srcAccessMask = {}`. These look wrong — shouldn't *some* prior stage need to finish? Read §06.00 "Image Memory Barriers" and explain why both are correct for this specific transition.
   *→ Hint: what is the image's state immediately after `createImage()`? Has any prior GPU command written to it?*

5. **(fresh)** For the second transition (`eTransferDstOptimal → eShaderReadOnlyOptimal`), identify the four mask values from the implementation plan and explain each one in plain language: what is waiting for what, and why would leaving any one mask wrong produce incorrect rendering (even if it doesn't always crash)?

6. **(fresh)** `transitionImageLayout()` is designed to throw `std::invalid_argument` for any transition not in its explicit list. An alternative would be to accept any `(oldLayout, newLayout)` pair and let the caller supply the barrier masks. Argue for the design in the implementation plan: why is the explicit list safer, and what class of bug does it prevent?

7. **(fresh)** The `VkSampler` object contains no reference to any `VkImage` or `VkImageView`. Explain what this separation enables — give one concrete scenario from this project (not a hypothetical) where you could exploit it.
   *→ Hint: look at the vertex data — the rectangle has four vertices with different colours.*

8. **(fresh)** Anisotropic filtering requires the feature to be declared in both `isDeviceSuitable()` and `createLogicalDevice()`. If you enable `anisotropyEnable = vk::True` in the sampler but omit `samplerAnisotropy = vk::True` from the feature chain in `createLogicalDevice()`, what does the validation layer report, and at which point in the program's execution does it fire?
   *→ Hint: not at physical device selection — the feature check in `isDeviceSuitable` is a prerequisite filter, not an enforcer.*

**Dependency / flow — How does information arrives here from earlier work?**

9. **(fresh, implementation prep)** `createImage()` allocates `DeviceMemory` using the same `findMemoryType()` helper from Ch04. Trace the argument values for the texture image specifically: what `usage` flags does the image need (two of them), what `memoryProperties` flags does device-local memory use, and what is the `tiling` argument — and why is it different from what a staging buffer would use?
   *→ Refers back to: Session 27 — `findMemoryType()` and `createBuffer()`.*

10. **(fresh, implementation prep)** `beginSingleTimeCommands()` and `endSingleTimeCommands()` extract a pattern already duplicated in `copyBuffer()`. Read the current `copyBuffer()` implementation mentally and answer: what three operations does `beginSingleTimeCommands()` encapsulate, and what two operations does `endSingleTimeCommands()` encapsulate?
    *→ Refers back to: Session 29 — `copyBuffer()` implementation.*

---

### Session B — Implementation

> **Depends on:** Session A complete and all questions answered.
> **Estimated time:** ~2h

#### Session Checklist
- [ ] Can explain the two transitions (with correct masks) before writing any code
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests below pass

#### Implementation Checklist

- [ ] `src/renderer/textures/` folder created; `Texture.h/.cpp` written — four RAII members, move-only, two accessors
- [ ] `src/renderer/descriptors/TextureDescriptorLayout.h/.cpp` — single binding (eCombinedImageSampler, eFragment, binding 0); same structure as `FrameDescriptorLayout`
- [ ] `VulkanContext::isDeviceSuitable()` — `samplerAnisotropy` check added
- [ ] `VulkanContext::createLogicalDevice()` — `samplerAnisotropy = vk::True` in feature chain
- [ ] `Renderer::beginSingleTimeCommands()` implemented
- [ ] `Renderer::endSingleTimeCommands()` implemented
- [ ] `Renderer::copyBuffer()` refactored to use the two helpers (same behaviour, shorter body)
- [ ] `Renderer::createImage()` implemented — returns `(Image, DeviceMemory)` pair
- [ ] `Renderer::createImageView()` implemented
- [ ] `Renderer::transitionImageLayout()` implemented — two explicit transitions + throw on unknown
- [ ] `Renderer::copyBufferToImage()` implemented
- [ ] `Renderer::createSampler()` (private) implemented — linear filtering, repeat, anisotropy
- [ ] `Renderer::createTexture(path)` implemented — full 10-step upload sequence per implementation plan
- [ ] `Renderer::bindTextureToDescriptor(const Texture&)` implemented — writes `DescriptorImageInfo` into texture descriptor set
- [ ] `Renderer` constructor updated — gains `const TextureDescriptorLayout&` parameter; creates texture descriptor pool (maxSets=1) and allocates one descriptor set
- [ ] `CMakeLists.txt` — `Texture.cpp` and `TextureDescriptorLayout.cpp` added to source list
- [ ] `Application::initVulkan()` — `textureDescriptorLayout_` constructed; `texture_` created via `renderer_->createTexture()`; `renderer_->bindTextureToDescriptor(*texture_)` called
- [ ] `Application.h` — `textureDescriptorLayout_` and `texture_` declared in correct destruction order

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Texture loaded and transferred | Add temporary `std::cout` of texture dimensions inside `createTexture()` | Width and height printed; match actual image file | [ ] |
| T2 | Texture descriptor set populated | Run app | No validation error about uninitialized descriptor | [ ] |
| T3 | Rectangle still renders (Ch05 behaviour) | Run app | Spinning coloured rectangle visible — vertex colours unchanged | [ ] |
| T4 | No resource leaks on shutdown | Close window | Validation layers silent on destruction | [ ] |
| T5 | Validation layers fully silent | Run with `VK_LAYER_KHRONOS_validation` | No errors or warnings on init, frames, or shutdown (OBS_HOOK exempt) | [ ] |

---

## Milestone M2 — Texture Sampling

> **Goal:** Wire the texture into the pipeline. Update the pipeline layout to two sets, extend `record()`, update `Vertex` and the shader, and confirm the texture is visible on the spinning rectangle.
> **Source:** §06.02 Combined Image Sampler

---

### Session A — Theory (light)

> **Estimated time:** ~30–45 min
> **Session Checklist:**
> - [ ] Read §06.02 in `docs/vulkan_chapter_06_texture_mapping.md`
> - [ ] Re-read your architecture session 43 answers on the two-set design and `TextureDescriptorLayout` before continuing
> - [ ] Answer the fresh Comprehension Questions below
> - [ ] Sketch the full data path of one frame: from `texture_` in `Application` → through `drawFrame` → into the fragment shader

#### Comprehension Questions

**Awareness — Why does this exist?**

1. ✅ *Why two descriptor sets (set 0 = UBO, set 1 = texture) rather than adding binding 1 to `FrameDescriptorLayout` — covered in architecture session 43.*

2. ✅ *`TextureDescriptorLayout` as a sibling class, not an extension — covered in architecture session 43.*

**Conceptual — What is it and how does it work?**

3. **(fresh)** `WriteDescriptorSet` for the UBO uses `pBufferInfo` pointing to a `DescriptorBufferInfo`. The texture write uses `pImageInfo` pointing to a `DescriptorImageInfo`. What three fields does `DescriptorImageInfo` have, and what value must `imageLayout` be set to — and why must it match the actual layout the image is in at draw time?
   *→ Hint: the layout was set at the end of `createTexture()` in M1.*

4. **(fresh)** In Slang, a combined image sampler at set 1, binding 0 is declared as `[[vk::binding(0, 1)]] Sampler2D texture_sampler`. The two-argument form `(binding, set)` is different from Ch05's one-argument `[[vk::binding(0)]]`. Why does Ch05 not need the second argument, and what would happen if you declared the texture sampler with `[[vk::binding(1)]]` (one argument) instead of `[[vk::binding(0, 1)]]`?

5. **(fresh)** UV coordinates range from `(0,0)` to `(1,1)`. In the vertex data the top-left vertex gets `{1.0f, 0.0f}` and the top-right gets `{0.0f, 0.0f}`. This seems counterintuitive — why is U=1 at the left and U=0 at the right? Relate your answer to the rectangle's position coordinates and the direction the camera is looking.
   *→ Hint: look at the vertex positions `{-0.5, -0.5}` through `{0.5, 0.5}` and think about which way the texture should appear.*

**Dependency / flow — How does information arrive here from earlier work?**

6. ✅ *Two-set pipeline layout plumbing (`pSetLayouts` array, `GraphicsPipeline` gains second layout ref) — covered in architecture session 43.*

7. **(fresh, implementation prep)** `bindDescriptorSets` in `record()` will bind both sets in one call: `{*frame_descriptor_set, *texture_descriptor_set}`. The first set (index 0) is already passed as a parameter to `record()`. The second set (index 1) is `textureDescriptorSet_` stored on `Renderer`. Trace how `record()` gets access to `textureDescriptorSet_` — is it passed as a parameter, or does `Renderer` forward it differently? Check the implementation plan to confirm.
   *→ Refers back to: implementation plan — `Application` section note about `textureDescriptorSet_`.*

8. **(fresh)** ✅ *Vertex wire-format change cascade — covered in architecture session 43 (Vertex.h, GraphicsPipeline.cpp:64, Application vertex data, shader) are already known.*
   **Fresh angle:** `GraphicsPipeline.cpp` line 64 will change from `std::array<..., 2>` to `auto`. Argue for using `auto` here rather than updating the hardcoded `3`: what future maintenance problem does `auto` prevent that `3` does not?

---

### Session B — Implementation

> **Depends on:** Session A complete. M1 Session B complete.
> **Estimated time:** ~2h

#### Session Checklist
- [ ] Can explain what `DescriptorImageInfo` needs and why `imageLayout` must be `eShaderReadOnlyOptimal` before writing any code
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests below pass

#### Implementation Checklist

- [ ] `GraphicsPipeline` constructor — gains `const TextureDescriptorLayout&` parameter; stored as member
- [ ] `GraphicsPipeline::createPipelineLayout()` — `setLayoutCount=2`; `pSetLayouts` points to `std::array{*frameLayout, *textureLayout}`
- [ ] `GraphicsPipeline::record()` — gains `const vk::raii::DescriptorSet& texture_descriptor_set` parameter; `bindDescriptorSets` updated to bind both sets in one call
- [ ] `GraphicsPipeline.cpp` line 64 — `std::array<..., 2>` → `auto`
- [ ] `Vertex.h` — `glm::vec2 texCoord` member added; `getAttributeDescriptions()` return type updated to `array<..., 3>`; third attribute description at location 2 (`eR32G32Sfloat`, `offsetof(Vertex, texCoord)`)
- [ ] `Application::initVulkan()` — `graphicsPipeline_` and `renderer_` constructors updated with `*textureDescriptorLayout_`
- [ ] `Application::initVulkan()` vertex data — four vertices updated with UV pairs
- [ ] `Application::mainLoop()` — `drawFrame()` call updated (texture descriptor set forwarded internally by Renderer)
- [ ] `shaders/triangle.slang` — `VertexInput` gains `float2 texCoord` at location 2; `VertexOutput` gains `float2 texCoord`; vertex shader passes through; fragment shader gains `[[vk::binding(0, 1)]] Sampler2D texture_sampler` and returns `texture_sampler.Sample(vertex_in.texCoord)`
- [ ] Shader recompiled to `build/shaders/triangle.spv`
- [ ] `CMakeLists.txt` — `TextureDescriptorLayout.cpp` already added in M1; verify no missing source files
- [ ] Add texture image file to project — `textures/texture.jpg` (or any JPEG/PNG — stb_image supports both)

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Texture visible | Run app | Rectangle shows image content — not a solid colour | [ ] |
| T2 | Rectangle rotates | Watch for ~4 seconds | Full rotation in ~4 seconds; texture rotates with geometry | [ ] |
| T3 | Aspect ratio correct | Resize window | Texture stays proportioned correctly on resize | [ ] |
| T4 | UV orientation | Pause visually at 0° rotation | Texture is right-side up, not mirrored or flipped | [ ] |
| T5 | Minimise / maximise | Minimise, then restore | No validation errors; texture reappears correctly | [ ] |
| T6 | Validation layers silent | Run with `VK_LAYER_KHRONOS_validation` | No errors or warnings (OBS_HOOK exempt) | [ ] |
| T7 | Clean shutdown | Close window | No leaked image / sampler / descriptor set / pool errors | [ ] |

---

## Chapter Progress Tracker

| Milestone | Theory ✓ | Impl ✓ | Tests Pass ✓ |
|-----------|----------|--------|--------------|
| M1 — Texture Resource | [ ] | [ ] | [ ] |
| M2 — Texture Sampling | [ ] | [ ] | [ ] |

**Chapter complete when all rows are fully ticked.**

---

## Session Log

| Session # | Date | Milestone(s) | What was covered | Blockers / open questions |
|-----------|------|--------------|------------------|--------------------------|
| 43 | 2026-05-05 | Arch discussion | Ch06 markdown, full architecture discussion, implementation plan, learning plan | |
| 44 | | M1-A | | |
| 45 | | M1-B | | |
| 46 | | M2-A | | |
| 47 | | M2-B | | |
