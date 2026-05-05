# Learning Plan — Chapter 05: Uniform Buffers

> **Estimated total time:** 8 sessions × ~1.5h avg = ~14h
> **Calibration basis:** Ch04 implementation sessions averaged 1h45m–3h37m. Ch05 architecture (sessions 35–37) is already complete; implementation starts at session 38.
> **Prerequisites:** Chapter 04 complete (indexed rectangle visible, resize clean). Implementation plan `vulkan_implementation_plan_05_uniform_buffers.md` reviewed and accepted.

> **⚠️ Overlap with architecture discussion.** Sessions 35–37 worked through substantial Ch05 theory while making design decisions (descriptor layout vs set, pool sizing, Y-flip approach, encapsulation patterns). Theory questions already answered there are marked ✅ with a session reference — re-read your answers from the session log rather than re-deriving from scratch. Theory sessions M1-A, M3-A, M4-A are correspondingly shorter than a typical fresh-territory theory session.

---

## Chapter Milestones Overview

| # | Milestone | Session Type | Est. Sessions | Est. Time |
|---|-----------|--------------|---------------|-----------|
| M1 | Descriptor Set Layout & Pipeline Integration | Theory (light) + Implementation | 2 | ~2.75h |
| M2 | Per-Frame Uniform Buffers & Persistent Mapping | Theory + Implementation | 2 | ~4h |
| M3 | Descriptor Pool & Per-Frame Sets | Theory (light) + Implementation | 2 | ~3.5h |
| M4 | MVP Math, Y-flip, and End-to-End Spinning Rectangle | Theory (light) + Implementation | 2 | ~3.75h |

---

## Milestone M1 — Descriptor Set Layout & Pipeline Integration

> **Session type:** Theory (light, Session A) + Implementation (Session B)
> **Estimated time:** Session A ~45min · Session B ~2h
> **Goal:** Understand the descriptor-layout vs descriptor-set distinction. Implement `FrameDescriptorLayout` as a standalone class and wire it into `GraphicsPipeline`'s pipeline-layout creation.
> **Source:** §05.00 Descriptor Set Layout & Buffer (layout half) + Vulkan Spec §14 Descriptor Sets

---

### Session A — Theory (light)

#### Session Checklist
- [ ] Re-read §05.00 (layout half) — most concepts already touched in arch sessions 35–36
- [ ] Re-read your answers to architecture Q4 (session 36) before continuing
- [ ] Answer the *fresh* Comprehension Questions below (the ✅ ones are recap pointers, no writing required)
- [ ] Write a one-paragraph summary: what role does `DescriptorSetLayout` play in the lifetime of a pipeline?

#### Comprehension Questions

**Awareness — Why does this exist?**

1. ✅ *Why descriptors exist; categories of non-per-vertex data — covered in arch session 36 (Q4 discussion).*

2. ✅ *Layout vs set utility / flexibility argument — covered in arch session 36. The "framebuffer ≈ set, render pass ≈ layout" analogy was the framing we landed on.*

**Conceptual — What is it and how does it work?**

3. ✅ *DescriptorSetLayoutBinding's `binding`, `descriptorType`, `descriptorCount`, `stageFlags` fields — covered when we hardcoded them in `FrameDescriptorLayout`'s design.* **One fresh angle:** the fifth field `pImmutableSamplers` was not discussed. Read the spec entry and answer: what does it do, and why is `nullptr` the correct value for our UBO binding?

4. **(fresh)** We chose `descriptorCount = 1` for our UBO. In what concrete shader scenario would `descriptorCount > 1` be correct? Give a specific example (e.g. skeletal animation, light arrays).

5. **(fresh, deepens arch Q4)** We agreed `stageFlags = eVertex` is correct. Beyond "the fragment shader doesn't read it" — what does the validation layer / driver actually do differently with a narrow stage flag vs `eAll`? Is there a runtime cost difference, or only a correctness signal?

6. **(fresh)** The tutorial creates the descriptor set layout **before** `createGraphicsPipeline()`. Read §05.00 — *"Pipeline layout"* paragraph — and answer: what does the pipeline layout *bake in* about the descriptor set layout, and is the resulting `vk::raii::Pipeline` still tied to the original `vk::raii::DescriptorSetLayout` handle's lifetime, or only to its *contents*?

**Dependency / flow — How does information arrive here from earlier work?**

7. **(fresh, implementation prep)** In Chapter 03 the `PipelineLayoutCreateInfo` was created with `setLayoutCount = 0` and `pSetLayouts = nullptr`. Trace exactly what changes in `GraphicsPipeline::createPipelineLayout()` to wire in the new layout: which two fields, and where does the layout pointer come from given the `const FrameDescriptorLayout&` constructor parameter?
   *→ Refers back to: Session 18 — `createPipelineLayout()` implementation.*

8. **(fresh, implementation prep)** The plan declares `frameDescriptorLayout_` between `swapChain_` and `graphicsPipeline_` in `Application`'s member list. Walk through destruction order (last-declared destroyed first): in what order do `frameDescriptorLayout_`, `graphicsPipeline_`, and `renderer_` actually get destroyed, and why does *this specific order* matter for the underlying `vk::raii::DescriptorSetLayout` handle?

---

### Session B — Implementation

> **Depends on:** Session A complete and all questions answered.

#### Session Checklist
- [ ] Can explain the layout-vs-set distinction in plain language before writing any code
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests below pass

#### Implementation Checklist

- [ ] Create `src/renderer/descriptors/` folder; add `FrameDescriptorLayout.h/.cpp`
- [ ] `FrameDescriptorLayout` class — `vk::raii::DescriptorSetLayout layout_` member, constructor takes `const VulkanContext&`, copy deleted, `getLayout() const` accessor returning `const vk::raii::DescriptorSetLayout&`
- [ ] Constructor body — single `DescriptorSetLayoutBinding{binding=0, type=eUniformBuffer, count=1, stage=eVertex}`; wrap in `DescriptorSetLayoutCreateInfo` with explicit type (no bare `{}` — see `feedback_vk_raii_construction.md`)
- [ ] `CMakeLists.txt` — add `src/renderer/descriptors/FrameDescriptorLayout.cpp` to source list
- [ ] `GraphicsPipeline` constructor — adds `const FrameDescriptorLayout& frame_layout` parameter, stores as member
- [ ] `GraphicsPipeline::createPipelineLayout()` — `setLayoutCount = 1`, `pSetLayouts = &*frameLayout_.getLayout()`
- [ ] `Renderer` constructor — adds `const FrameDescriptorLayout& frame_layout` parameter, stores as member (still unused inside Renderer this milestone — wired up in M3)
- [ ] `Application` — declares `std::unique_ptr<FrameDescriptorLayout> frameDescriptorLayout_` (declared between `swapChain_` and `graphicsPipeline_`)
- [ ] `Application::initVulkan()` — constructs `frameDescriptorLayout_` after `swapChain_`, passes `*frameDescriptorLayout_` into both `GraphicsPipeline` and `Renderer` constructors
- [ ] Project compiles cleanly; rectangle still renders unchanged

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Descriptor set layout creates without error | Run app | No `vk::raii` exceptions during init | [ ] |
| T2 | Pipeline accepts the layout | Build cleanly | No compile errors; no validation errors at pipeline creation | [ ] |
| T3 | Rectangle still renders | Run application | Static rectangle visible, exactly as in Ch04 | [ ] |
| T4 | Validation layers silent | Run with `VK_LAYER_KHRONOS_validation` | No errors or warnings (OBS_HOOK exempt) | [ ] |

---

## Milestone M2 — Per-Frame Uniform Buffers & Persistent Mapping

> **Session type:** Theory (Session A) + Implementation (Session B)
> **Estimated time:** Session A ~1.5h · Session B ~2.5h
> **Goal:** Understand persistent mapping and std140 alignment. Add UBO members to `RenderFrameSlot`, extend `Renderer::initializeFrameData()` to create + map a uniform buffer per slot, and add `RenderFrameSlot::updateUniformBuffer()`.
> **Source:** §05.00 (uniform buffer half) + Vulkan Spec §15.6.4 std140 layout rules

---

### Session A — Theory

#### Session Checklist
- [ ] Read §05.00 (uniform buffer + persistent mapping sections) in full
- [ ] Answer all Comprehension Questions below
- [ ] Sketch (or describe in words) the lifetime of `uniformMapped_` inside one `RenderFrameSlot`: when is it set, when is it valid, what destroys it
- [ ] Write one paragraph: why is the staging-buffer pattern (Ch04) **wrong** for uniform buffers, and what's the rule of thumb for choosing between staged-device-local vs persistent-host-visible?

#### Comprehension Questions

**Awareness — Why does this exist?**

1. Why do we need `MAX_FRAMES_IN_FLIGHT` separate uniform buffers, not one shared buffer? Walk through the failure mode if a single buffer were used: what is the CPU doing, what is the GPU doing, and exactly when do they collide?
   *→ Hint: §05.00 — "We don't want to update the buffer in preparation of the next frame..."*

2. Vertex buffers (Ch04) are uploaded once via staging into device-local memory. Uniform buffers are kept host-visible and persistently mapped. State the **trade-off** that drove this choice — and identify the data-update frequency at which the choice flips.
   *→ Hint: §05.00 — "It doesn't really make any sense to have a staging buffer."*

**Conceptual — What is it and how does it work?**

3. `mapMemory(0, sizeof(UBO))` returns a `void*`. The tutorial explicitly says **never call `unmapMemory()`** on uniform buffers. What invariant of the mapped pointer would `unmapMemory` break, and what cleanup mechanism replaces it for persistent maps?

4. `eHostCoherent` is paired with `eHostVisible` in the memory property flags. Re-read the Vulkan Spec memory model section: what guarantee does `eHostCoherent` add, and what *additional* code would be needed if you used `eHostVisible` alone?
   *→ Hint: `vkFlushMappedMemoryRanges` / `vkInvalidateMappedMemoryRanges`.*

5. The chapter doc warns about **std140 alignment rules**. Re-read the "Common Pitfalls" section of the chapter markdown. Suppose your UBO struct were `{ float t; glm::mat4 model; }`. What alignment problem arises, and what are the two ways to fix it?

6. `RenderFrameSlot::updateUniformBuffer()` is a one-line `memcpy`. Argue against the alternative of exposing `uniformMapped_` publicly and letting `Renderer::drawFrame()` do the `memcpy` inline. What invariant of `RenderFrameSlot` would the public exposure leak?
   *→ Refers back to: Architecture discussion Q6 — sessions 35 and 37.*

**Dependency / flow — How does information arrive here from earlier work?**

7. The implementation plan extends `Renderer::initializeFrameData()` to create the UBO + memory + mapped pointer per slot. Trace exactly which existing helper from Chapter 04 is reused for the buffer allocation, and what arguments it receives in this case (size, usage, memory properties).
   *→ Refers back to: Session 29 — `Renderer::createBuffer()`.*

8. `RenderFrameSlot` declares its members in a specific order. Look at the `.h` and the destruction-order rule for class members (last-declared destroyed first). Why must `uniformMemory_` be declared **after** `uniformBuffer_`, and why must `uniformMapped_` (a raw `void*`) be declared **after** `uniformMemory_`?

---

### Session B — Implementation

> **Depends on:** Session A complete and all questions answered. M1 Session B complete.

#### Session Checklist
- [ ] Can explain why uniform buffers don't use staging before writing any code
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests below pass

#### Implementation Checklist

- [ ] `src/renderer/buffers/UniformBufferObject.h` — header-only struct with `glm::mat4 model, view, proj`
- [ ] `RenderFrameSlot.h` — adds members `uniformBuffer_`, `uniformMemory_`, `uniformMapped_ = nullptr`, `descriptorSet_ = nullptr`
- [ ] `RenderFrameSlot.h` — adds `updateUniformBuffer(const UniformBufferObject&) → void` method (one-line `memcpy`)
- [ ] Include `<cstring>` somewhere reachable from `RenderFrameSlot.h` (or do the `memcpy` in a new `RenderFrameSlot.cpp` if you'd prefer to keep the header light — call it now if you choose this)
- [ ] `Renderer::initializeFrameData()` — for each slot: call `createBuffer(sizeof(UBO), eUniformBuffer, eHostVisible | eHostCoherent)`, move the pair into `slot.uniformBuffer_`/`slot.uniformMemory_`, then `slot.uniformMapped_ = slot.uniformMemory_.mapMemory(0, sizeof(UBO))` (no unmap)
- [ ] Add `CMakeLists.txt` compile definitions: `GLM_FORCE_DEPTH_ZERO_TO_ONE` and `GLM_FORCE_RADIANS` (project-wide, before any GLM include)
- [ ] Project compiles cleanly; rectangle still renders unchanged (no shader change yet — UBO bytes exist but aren't read yet)

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Buffers created, memory mapped | Add a temporary `std::cout` of `slot.uniformMapped_` after init; verify two distinct non-null pointers | Two pointers printed, both non-null, different from each other | [ ] |
| T2 | `sizeof(UniformBufferObject)` | `static_assert(sizeof(UniformBufferObject) == 192, ...)` somewhere | Compiles — three `mat4` × 64B = 192B | [ ] |
| T3 | Rectangle still renders | Run app | Static rectangle visible | [ ] |
| T4 | No leaks on shutdown | Run + close window | Validation layer silent — no "buffer destroyed while memory still bound" errors | [ ] |
| T5 | Validation layers silent | Run with `VK_LAYER_KHRONOS_validation` | No errors or warnings | [ ] |

---

## Milestone M3 — Descriptor Pool & Per-Frame Sets

> **Session type:** Theory (light, Session A) + Implementation (Session B)
> **Estimated time:** Session A ~1h · Session B ~2.5h
> **Goal:** Understand descriptor pool sizing and the two-phase set lifecycle (allocate then update). Add the descriptor pool on `Renderer`, allocate one set per slot, and wire each set to its slot's UBO via `updateDescriptorSets`.
> **Source:** §05.01 Descriptor Pool & Sets

---

### Session A — Theory (light)

#### Session Checklist
- [ ] Re-read §05.01 — pool sizing already worked through in arch session 37 (Q5)
- [ ] Re-read your answers to architecture Q5 (session 37) before continuing
- [ ] Answer the *fresh* Comprehension Questions below (the ✅ ones are recap pointers)
- [ ] Sketch the per-slot wiring after this milestone: descriptor set → `WriteDescriptorSet` → `DescriptorBufferInfo` → uniform buffer. Label each arrow with the API call that creates it.

#### Comprehension Questions

**Awareness — Why does this exist?**

1. **(fresh)** Why are descriptor sets *allocated from a pool* instead of constructed directly via a `vk::raii::DescriptorSet(device, info)` two-arg constructor (the way buffers are)? What does the pool model give the driver that the per-set construction model does not?
   *→ Hint: think about how command buffers are allocated. Pools are about **batched allocation** with shared backing memory.*

2. ✅ *`eFreeDescriptorSet` flag and the bump-allocator optimisation — covered in arch session 37 (Q5b). The destructor-cost argument is in your session-37 answer.*

**Conceptual — What is it and how does it work?**

3. ✅ *Pool sizing rule (`maxSets` vs `pPoolSizes`) — covered in arch session 37 (Q5a). The "one pool, multiple sets" framing and the `poolSizeCount = number of types` clarification are in your session-37 answer.*

4. **(fresh)** `allocateDescriptorSets` requires a `pSetLayouts` array whose length equals `descriptorSetCount`. If all sets share the same layout, why must you still pass a *replicated* array (`std::vector<vk::DescriptorSetLayout> layouts(N, *layout_)`) rather than a single-element array with `count = N`?

5. **(fresh)** The two-phase lifecycle is: (a) `allocateDescriptorSets` produces empty sets; (b) `updateDescriptorSets` populates each with a real buffer/image binding via `WriteDescriptorSet`. What field of `WriteDescriptorSet` connects it to a specific binding inside the layout, and what would happen if that field's value didn't match any binding in the layout?

6. **(fresh)** `vk::DescriptorBufferInfo.range` can be `sizeof(UBO)` or `vk::WholeSize`. What's the practical difference, and which one does the implementation plan use?

**Dependency / flow — How does information arrive here from earlier work?**

7. **(fresh, implementation prep)** Each `WriteDescriptorSet` needs three things from earlier work: the descriptor set handle (allocated from the pool in this milestone), the buffer handle (created in M2), and the layout binding index (declared in M1). Trace where each of these three values lives in our class hierarchy, and which class assembles them into the write structure.

8. **(fresh, deepens arch Q5)** The implementation plan calls `updateDescriptorSets` once per slot inside `initializeFrameData()` — **not** once per frame inside `drawFrame()`. Why is this safe? What property of the UBO buffer's lifetime makes the descriptor's pointer-to-buffer permanent for the slot's lifetime?

---

### Session B — Implementation

> **Depends on:** Session A complete and all questions answered. M2 Session B complete.

#### Session Checklist
- [ ] Can explain pool sizing and the two-phase set lifecycle before writing any code
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests below pass

#### Implementation Checklist

- [ ] `Renderer.h` — adds `vk::raii::DescriptorPool descriptorPool_` member
- [ ] `Renderer.h` — adds private `createDescriptorPool() → void` declaration
- [ ] `Renderer.cpp` — `createDescriptorPool()` body: single `DescriptorPoolSize{eUniformBuffer, MAX_FRAMES_IN_FLIGHT}`, `DescriptorPoolCreateInfo{maxSets=MAX_FRAMES_IN_FLIGHT, poolSizeCount=1, pPoolSizes=&size, flags={}}` (use explicit type, not bare `{}`)
- [ ] `Renderer` constructor — calls `createDescriptorPool()` between `createCommandPool()` and `initializeFrameData()`
- [ ] `Renderer::initializeFrameData()` extension — for each slot: build `DescriptorSetAllocateInfo`, call `device.allocateDescriptorSets(info)`, move the front element into `slot.descriptorSet_`
- [ ] `Renderer::initializeFrameData()` extension — build `DescriptorBufferInfo{*slot.uniformBuffer_, 0, sizeof(UBO)}` + `WriteDescriptorSet{*slot.descriptorSet_, 0, 0, 1, eUniformBuffer, …, &bufferInfo, …}` and call `device.updateDescriptorSets({write}, {})`
- [ ] Project compiles cleanly; rectangle still renders unchanged (descriptor sets exist and are populated, but pipeline still doesn't bind them — that's M4)

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Pool creates without error | Run app | No allocation failure on startup | [ ] |
| T2 | Sets allocated, slots non-null | Add temporary `std::cout` of `*slot.descriptorSet_` for both slots | Two distinct non-null handles | [ ] |
| T3 | Writes succeed | Validation layer silent during init | No "uninitialized descriptor" warnings | [ ] |
| T4 | Rectangle still renders | Run app | Static rectangle visible | [ ] |
| T5 | Validation layers silent | Run with `VK_LAYER_KHRONOS_validation` | No errors or warnings | [ ] |

---

## Milestone M4 — MVP Math, Y-flip, and End-to-End Spinning Rectangle

> **Session type:** Theory (light, Session A) + Implementation (Session B)
> **Estimated time:** Session A ~1.25h · Session B ~2.5h
> **Goal:** Understand Vulkan vs OpenGL clip space, the Y-flip problem, and the negative-viewport-height solution. Wire the per-frame UBO update into `drawFrame`, compute MVP matrices in `Application`, bind descriptor sets in `record()`, flip the viewport, and watch the rectangle spin.
> **Source:** §05.00 (MVP math + Y-flip) + §05.01 (descriptor binding + winding)

---

### Session A — Theory (light)

#### Session Checklist
- [ ] Re-read §05.00 (MVP math + Y-flip pitfall) and §05.01 (binding + winding) — Y-flip approach already chosen in arch session 37 (Q7c)
- [ ] Re-read your answers to architecture Q7 (session 37) before continuing
- [ ] Answer the *fresh* Comprehension Questions below (the ✅ ones are recap pointers)
- [ ] Sketch (or describe) the data path of one frame's MVP matrix from `Application::computeUniformBufferObject()` through `drawFrame` and into the GPU's vertex shader. Label every transformation and every API call.

#### Comprehension Questions

**Awareness — Why does this exist?**

1. **(fresh)** The model matrix is `glm::rotate(I, time * radians(90), vec3(0,0,1))`. Why is the rotation parameterised by **wall-clock time**, not by frame count or a fixed delta? What property does this give the animation that frame-count parameterisation lacks?

2. ✅ *MVP math home on Application + future migration to `scene/Camera` — covered in arch session 37 (Q7a).*

**Conceptual — What is it and how does it work?**

3. **(fresh)** The vertex shader applies the matrices as `proj * view * model * float4(inPosition, 0, 1)`. Walk through what each matrix does in terms of coordinate-space transformations: object → world → camera → clip. Identify which space the rasterizer cares about and what the homogeneous divide does after the shader.

4. **(fresh, useful diagnostic)** **The Y-flip — symptom identification.** Even though we've chosen the negative-viewport-height fix, the diagnostic skill matters: GLM's `glm::perspective` produces a clip-space matrix where Y points UP; Vulkan's clip-space Y points DOWN. Without *any* fix, what visual symptom appears on screen, and how would you confirm it's specifically the Y axis at fault (not, say, X or Z)?
   *→ Hint: The chapter doc's Pitfalls section lists the symptom.*

5. ✅ *Two Y-flip solutions compared (`proj[1][1] *= -1` vs negative viewport height) — covered in arch session 37 (Q7c). Your decision rationale and the winding-order side-effect of the matrix approach are in your session-37 answer.*

6. ✅ *`GLM_FORCE_DEPTH_ZERO_TO_ONE` and Vulkan's `[0,1]` depth range — covered in arch session 37 (Q7c).*

**Dependency / flow — How does information arrive here from earlier work?**

7. **(fresh, implementation prep)** The descriptor set bound in `GraphicsPipeline::record()` must be the one belonging to the slot whose UBO was just updated by `RenderFrameSlot::updateUniformBuffer()`. Trace the full chain: `Application::run()` computes UBO → passes to `Renderer::drawFrame()` → which slot's `updateUniformBuffer` is called, and which slot's `descriptorSet_` is passed into `record()`? What member variable indexes both?

8. **(fresh, deliberately tricky)** Ch04 set `rasterizer.frontFace = eClockwise` for screen-space-Y-down vertices. With negative viewport height, what should the front-face value be in Ch05, and why? (This is *not* the same answer as the tutorial's, which uses `eCounterClockwise` because of the matrix Y-flip. Reason from first principles — what does the rasterizer actually see in framebuffer space with our setup?)

---

### Session B — Implementation

> **Depends on:** Session A complete and all questions answered. M3 Session B complete.

#### Session Checklist
- [ ] Can explain the negative-viewport-height approach + its impact (or lack thereof) on winding before writing any code
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests below pass

#### Implementation Checklist

- [ ] `Application.h` — adds `std::chrono::high_resolution_clock::time_point startTime_` member
- [ ] `Application.h` — adds private `computeUniformBufferObject(vk::Extent2D, float time_seconds) const → UniformBufferObject` method
- [ ] `Application` constructor — initialises `startTime_ = std::chrono::high_resolution_clock::now()`
- [ ] `Application::computeUniformBufferObject()` body — `model = glm::rotate(I, time * glm::radians(90.0f), {0,0,1})`, `view = glm::lookAt({2,2,2}, {0,0,0}, {0,0,1})`, `proj = glm::perspective(glm::radians(45.0f), w/(float)h, 0.1f, 10.0f)`. **No** `proj[1][1] *= -1`.
- [ ] `Application::mainLoop()` — per frame: compute `time_seconds = duration<float>(now - startTime_).count()`, call `computeUniformBufferObject(swapChain_->getExtent(), time_seconds)`, pass result into `renderer_->drawFrame(...)`
- [ ] `Renderer::drawFrame()` signature — adds `const UniformBufferObject&` parameter
- [ ] `Renderer::drawFrame()` body — between `resetFences()` and `commandBuffer_.reset()`: `slot.updateUniformBuffer(ubo)`
- [ ] `Renderer::drawFrame()` — passes `*slot.descriptorSet_` (or `slot.descriptorSet_` per Vulkan-Hpp idiom) into `graphics_pipeline.record(...)`
- [ ] `GraphicsPipeline::record()` signature — adds `const vk::raii::DescriptorSet& descriptor_set` parameter
- [ ] `GraphicsPipeline::record()` body — `bindDescriptorSets(eGraphics, *layout_, 0, *descriptor_set, {})` after `bindPipeline`, before `mesh.bind()`
- [ ] `GraphicsPipeline::record()` viewport — `viewport.y = static_cast<float>(extent.height)`, `viewport.height = -static_cast<float>(extent.height)`
- [ ] `shaders/triangle.slang` — adds `struct UniformBufferObject { float4x4 model, view, proj; }`, `[[vk::binding(0)]] ConstantBuffer<UniformBufferObject> ubo;`, vertex shader applies `mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(in.position, 0, 1))))`
- [ ] Recompile shader to `.spv`
- [ ] Full build, run — rectangle spins around Z axis at 90°/sec

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Rectangle is visible | Run app | Rectangle renders (not blank) — confirms set is bound + UBO is read | [ ] |
| T2 | Rectangle spins | Watch for ~3 seconds | Rotates around Z at 90°/sec — full rotation in ~4 seconds | [ ] |
| T3 | Aspect ratio correct on resize | Drag window edge to make it tall and narrow | Rectangle stays correctly proportioned (no stretch); cycles through resize without visual artifacts | [ ] |
| T4 | Y-orientation correct | Rotate slowly enough to inspect a single moment — verify the colour gradient is "right side up" | Top-of-screen vertices match top-of-rectangle vertices in the source `vertices[]` array | [ ] |
| T5 | Minimise / maximise clean | Minimise window, then restore | No validation errors; rectangle resumes spinning correctly | [ ] |
| T6 | Validation layers silent | Run with `VK_LAYER_KHRONOS_validation` enabled | No errors or warnings on init, every frame, or shutdown (OBS_HOOK exempt) | [ ] |
| T7 | Clean exit | Close window | No leaked descriptor sets / buffers / pool — validation layer silent on destruction | [ ] |

---

## Chapter Progress Tracker

| Milestone | Theory ✓ | Impl ✓ | Tests Pass ✓ |
|-----------|----------|--------|--------------|
| M1 — Descriptor Set Layout & Pipeline Integration | ✓ | ✓ | ✓ |
| M2 — Per-Frame Uniform Buffers & Persistent Mapping | ✓ | ✓ | ✓ |
| M3 — Descriptor Pool & Per-Frame Sets | ✓ (carried to Ch06) | ✓ | ✓ |
| M4 — MVP Math, Y-flip, and End-to-End Spinning Rectangle | ✓ (carried to Ch06) | ✓ | ✓ |

**Chapter complete when all rows are fully ticked.**

---

## Session Log

Fill in after each session to track progress and blockers.

| Session # | Date | Milestone(s) | What was covered | Blockers / open questions |
|-----------|------|--------------|------------------|--------------------------|
| 38 | | M1-A | | |
| 39 | | M1-B | | |
| 40 | | M2-A | | |
| 41 | | M2-B | | |
| 42 | | M3-A | | |
| 43 | | M3-B | | |
| 44 | | M4-A | | |
| 45 | | M4-B | | |
