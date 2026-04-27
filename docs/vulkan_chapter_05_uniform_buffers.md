# Chapter 05: Uniform Buffers

> **Source:** https://docs.vulkan.org/tutorial/latest/05_Uniform_buffers/00_Descriptor_set_layout_and_buffer.html, https://docs.vulkan.org/tutorial/latest/05_Uniform_buffers/01_Descriptor_pool_and_sets.html
> **Vulkan Version:** 1.4 | **Language:** C++20 | **Shading:** Slang | **Bindings:** Vulkan-Hpp RAII

---

## Overview

Chapter 05 introduces **resource descriptors** — the mechanism by which shaders read from buffers and images that are not part of the per-vertex input stream. The immediate motivation is the Model-View-Projection (MVP) matrix: a set of 4×4 transforms that is constant across every vertex of a draw call. Baking the MVP into vertex data would waste memory and forbid animation; passing it as a uniform buffer solves both.

This chapter has two logical halves:

1. **Descriptor set layout + uniform buffer creation (§05.00).** Declare the *shape* of the resources the pipeline will read (set layout), wire that shape into the pipeline layout, then allocate one uniform buffer per frame-in-flight and persistently map them for per-frame updates.
2. **Descriptor pool + descriptor sets (§05.01).** Allocate descriptor sets from a pool, point each set at its corresponding uniform buffer via `vkUpdateDescriptorSets`, and bind the current frame's set into the command buffer before `drawIndexed`.

By the end of the chapter the rectangle from Chapter 04 should rotate continuously around the Z axis at 90°/second, projected through a perspective camera that correctly adapts to window resize.

Two subtle but important Vulkan concepts also land in this chapter: **std140-style layout alignment rules** for uniform data and the **Y-flip correction** required when using GLM's OpenGL-legacy projection matrices with Vulkan's clip-space convention (and the associated knock-on effect on winding order).

---

## §05.00 — Descriptor Set Layout & Uniform Buffer

### Concepts

**Why descriptors exist.** Vertex buffers can carry *per-vertex* data, but much of what a shader needs is *per-draw*, *per-object*, or *per-frame*: transforms, light positions, camera parameters, texture samplers. Descriptors are the API mechanism by which the pipeline is told *"at binding N of set M you will find a resource of type T"* — then, at draw time, a descriptor set is bound that supplies the actual buffer or image handles.

**Layout vs. set — the critical separation.** The tutorial draws an explicit analogy:

> *"The descriptor set layout specifies the types of resources that will be accessed by the pipeline, just like a render pass specifies types of attachments. A descriptor set specifies the actual buffer or image resources that will be bound to the descriptors, just like a framebuffer specifies the actual image views."*

The layout is a **shape declaration** — created once, baked into the pipeline layout, and immutable. The set is a **resource binding** — allocated from a pool and updated to point at real `VkBuffer`/`VkImage` handles. Many sets can share one layout; many pipelines can share one layout.

**Three-part descriptor workflow.**
1. Create a `DescriptorSetLayout` — declares what the shader expects.
2. Allocate a `DescriptorSet` from a pool — produces a handle to fill in.
3. Bind the set during command recording — tells the GPU *"when the pipeline reads binding 0, look here."*

**Multiple frames in flight — multiple uniform buffers.** Because `MAX_FRAMES_IN_FLIGHT` frames can execute concurrently (CPU recording frame N+1 while GPU reads frame N), a single uniform buffer would be a data race. The tutorial states:

> *"We don't want to update the buffer in preparation of the next frame while a previous one is still reading from it! Thus, we need to have as many uniform buffers as we have frames in flight."*

The same reasoning applies to descriptor sets in §05.01: each frame slot owns its own set pointing at its own buffer.

**Persistent mapping.** Unlike vertex/index buffers (staged once from host-visible memory into device-local VRAM), uniform buffers are kept in **host-visible, host-coherent** memory and **mapped once at creation time**. The mapped pointer is stored and reused every frame. Reasoning:

> *"It doesn't really make any sense to have a staging buffer. It would just add extra overhead in this case and likely degrade performance instead of improving it."*

A staging-upload round trip costs far more than the minor GPU bandwidth hit of reading from host-visible memory for ~192 bytes of matrices.

**The shader-to-host contract.** The shader's UBO struct and the C++ `UniformBufferObject` struct must be **binary compatible** — same field order, same field sizes, same alignment. The tutorial is blunt:

> *"The data in the matrices is binary compatible with the way the shader expects it, so we can later just memcpy a UniformBufferObject to a VkBuffer."*

For a vec4/mat4-only struct with GLM defaults this works out of the box. As soon as a `vec2` or `vec3` appears between `mat4` members, std140 alignment rules bite — covered in Pitfalls below.

**Push constants — briefly mentioned.** The tutorial notes that for very small, frequently changing values, *push constants* are the preferred path. They are deferred to later chapters; for the MVP case, uniform buffers are the right tool.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::DescriptorSetLayoutBinding` | Struct | Describes one descriptor slot: binding index, descriptor type, count, shader stage flags |
| `vk::DescriptorType::eUniformBuffer` | Enum value | The descriptor type used for UBO blocks |
| `vk::ShaderStageFlagBits::eVertex` | Enum value | Restricts which pipeline stages can read this descriptor |
| `vk::DescriptorSetLayoutCreateInfo` | Struct | Aggregates one or more bindings into a single layout |
| `vk::raii::DescriptorSetLayout` | RAII handle | The created layout; consumed by the pipeline layout |
| `vk::PipelineLayoutCreateInfo` | Struct | Now populated with `setLayoutCount = 1` and `pSetLayouts = &*descriptorSetLayout` |
| `vk::BufferUsageFlagBits::eUniformBuffer` | Enum value | Passed to `createBuffer` to tag the buffer's role |
| `vk::MemoryPropertyFlagBits::eHostVisible \| eHostCoherent` | Flags | Host-visible + coherent memory for persistent mapping |
| `device.mapMemory(offset, size)` / `unmapMemory` | Method / method | Map device memory into host address space; uniform buffers call `mapMemory` once and never unmap |
| `std::memcpy` | stdlib | Copy `UniformBufferObject` bytes into the mapped pointer each frame |
| `glm::rotate`, `glm::lookAt`, `glm::perspective` | GLM functions | Build model, view, projection matrices respectively |
| `glm::radians` | GLM function | Convert degrees to radians |
| `std::chrono::high_resolution_clock::now()` | stdlib | Wall-clock for time-based animation; diffed against a static start time |

### Code Walkthrough

**Shader side.** The vertex shader gains a `ConstantBuffer<UniformBuffer>` (Slang) or `uniform UniformBufferObject ubo` block (GLSL) at binding 0. Its three `float4x4` members — `model`, `view`, `proj` — are applied to `inPosition` left-to-right as `proj * view * model * vec4(inPosition, 0.0, 1.0)`. The homogeneous divide happens after the vertex shader; non-1 W components produce perspective foreshortening. The fragment shader is unchanged — it still reads interpolated `color`.

**C++ mirror struct.** A plain aggregate `UniformBufferObject` with three `glm::mat4` fields lives in the same header as (or near) the shader. `sizeof(UniformBufferObject)` is the per-frame uniform buffer size — three 64-byte matrices = 192 bytes.

**Descriptor set layout creation.** Build a single `DescriptorSetLayoutBinding`: binding `0`, type `eUniformBuffer`, `descriptorCount = 1` (single struct; arrays would be >1, e.g. skeletal bone matrices), stage flags `eVertex` (the fragment shader doesn't read it), `pImmutableSamplers = nullptr`. Wrap it in a `DescriptorSetLayoutCreateInfo` with `bindingCount = 1, pBindings = &binding`, and construct a `vk::raii::DescriptorSetLayout` from the device and create info.

**Order constraint — layout first, pipeline second.** The pipeline layout (`vk::PipelineLayoutCreateInfo`) must see the descriptor set layout at construction time. Create the descriptor set layout **before** `createGraphicsPipeline()` runs, then populate `pipelineLayoutInfo.setLayoutCount = 1` and `pipelineLayoutInfo.pSetLayouts = &*descriptorSetLayout`. Multiple sets are possible — `setLayoutCount` can be >1 — but the MVP case uses a single set at slot 0.

**Uniform buffer creation.** Allocate three parallel vectors sized to `MAX_FRAMES_IN_FLIGHT`:
- `std::vector<vk::raii::Buffer>` for the buffer handles
- `std::vector<vk::raii::DeviceMemory>` for the backing allocations
- `std::vector<void*>` for the persistently-mapped pointers

For each slot: call the existing `createBuffer` helper with `sizeof(UniformBufferObject)`, usage `eUniformBuffer`, and memory properties `eHostVisible | eHostCoherent`. Then `mapMemory(0, bufferSize)` on the new `DeviceMemory` and store the returned `void*` in `uniformBuffersMapped[i]`. Do **not** unmap — the mapping stays alive until the `DeviceMemory` is destroyed at shutdown.

**Per-frame update (`updateUniformBuffer`).** Called from `drawFrame()` **before** submit. It needs:
- A `static auto startTime = high_resolution_clock::now();` captured on first call so animation is reproducible across runs.
- A `float time` = seconds since `startTime`.
- `ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), vec3(0, 0, 1))` — 90°/sec around Z.
- `ubo.view = glm::lookAt(vec3(2, 2, 2), vec3(0, 0, 0), vec3(0, 0, 1))` — eye 45° above the scene looking at origin, with Z up.
- `ubo.proj = glm::perspective(glm::radians(45.0f), extent.width / extent.height, 0.1f, 10.0f)` — fov 45°, near 0.1, far 10. Compute the aspect ratio from the **current** swap chain extent so resize stays correct.
- `ubo.proj[1][1] *= -1;` — the Y-flip. GLM was authored for OpenGL (Y-up clip space); Vulkan uses Y-down clip space. Without this flip the image is upside-down on screen.
- `std::memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo))` — single coherent write, no flush needed because the memory is host-coherent.

**Knock-on effect of the Y-flip: winding order.** Flipping the projection's Y scale reverses the screen-space winding of every triangle. If the rasterizer was culling back faces with `eClockwise` as front, it must now be configured with `eCounterClockwise` — otherwise the scene appears blank. This is covered explicitly in §05.01 but originates here.

### Common Pitfalls

- **std140 alignment mismatches.** The Vulkan SPIR-V spec requires each UBO member to sit on an alignment boundary determined by its type: scalar = 4 B, `vec2` = 8 B, `vec3`/`vec4` = 16 B, nested struct or `mat4` = 16 B. A `mat4` always works by accident (64 B is a multiple of 16). Adding a `vec2 foo` before a `mat4` breaks everything because the `mat4` no longer starts at a 16 B boundary. Fix: `alignas(16)` on the C++ members, or `#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES` before including GLM.
- **Skipping the Y-flip.** If the rendered output is upside-down after wiring up the UBO, the flip is missing. If it's invisible, the flip is present but winding order wasn't adjusted.
- **Computing aspect ratio from a stale extent.** Caching the aspect from initial window size makes the image stretch after a resize. Always read `swapChain_->getExtent()` inside `updateUniformBuffer`.
- **Forgetting `ubo.proj[1][1] *= -1`.** Common and silent on square windows where the visual difference is hardest to catch. Force-resize to a rectangular window to verify.
- **Stage flags too broad.** Setting `stageFlags = eAll` works but hints at a wasted state. Use the narrowest flag that matches the shader — `eVertex` here. Validation layers won't complain, but the habit pays off later.
- **Using `descriptorCount > 1` by accident.** `descriptorCount` is for shader arrays (e.g. `UniformBuffer ubos[8]`), not for "multiple buffers across frames." Those require multiple **sets**, not a larger count on a single binding.

---

## §05.01 — Descriptor Pool & Descriptor Sets

### Concepts

**Pools as allocators.** Descriptor sets can't be created directly — they must be allocated from a descriptor pool, exactly as command buffers are allocated from a command pool. The pool holds pre-allocated capacity for N descriptor sets and M descriptors per type. Exceeding either limit fails allocation.

**Sizing the pool.** Two numbers drive the pool size:
- `maxSets` — the total number of descriptor sets that will ever be live at once. For a one-set-per-frame pattern, this is `MAX_FRAMES_IN_FLIGHT`.
- `poolSizes[i].descriptorCount` — the total number of descriptors of type `poolSizes[i].type`. For a single UBO binding per set, it is also `MAX_FRAMES_IN_FLIGHT` (each of the N sets consumes one uniform-buffer descriptor).

**Pool flags.** `vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet` allows individual sets to be returned to the pool via `freeDescriptorSets`. If the sets live for the entire program (as in this chapter), the flag is optional — the whole pool is freed at shutdown. The tutorial includes it defensively.

**Sets are filled in two phases.** `allocateDescriptorSets` hands back uninitialised set handles — the descriptor slots inside them point to nothing. A second step, `updateDescriptorSets`, populates each set with the actual `VkBuffer` to read from. Binding an allocated-but-not-updated set is undefined behaviour.

**Binding at record time.** The set is not associated with the pipeline layout or the command buffer until `vkCmdBindDescriptorSets` runs. This happens inside the pipeline's `record()` function, after `bindPipeline`, before `drawIndexed`. The binding parameters tie three things together:
1. A pipeline bind point (`eGraphics` vs `eCompute`) — descriptor sets aren't graphics-specific.
2. The pipeline layout that the descriptor set layout was baked into.
3. The starting set index (`firstSet = 0`) and the array of sets to bind.

**Why bind per frame, not once.** Because each frame has its own set (to avoid the data race on buffer updates), the binding call must use `descriptorSets[currentFrame]`. Binding once at startup would pin frame 0's set for every frame.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::DescriptorPoolSize` | Struct | One entry declaring `{descriptorType, count}` for pool capacity |
| `vk::DescriptorPoolCreateInfo` | Struct | Full pool spec: `maxSets`, array of `DescriptorPoolSize`, optional flags |
| `vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet` | Enum value | Allows individual sets to be returned via `freeDescriptorSets` |
| `vk::raii::DescriptorPool` | RAII handle | The created pool; destroying it frees all allocated sets |
| `vk::DescriptorSetAllocateInfo` | Struct | Specifies pool + count + layout array for allocation |
| `vk::raii::DescriptorSet` | RAII handle | One descriptor set, one per frame in this chapter |
| `device.allocateDescriptorSets(info)` | Method | Returns `std::vector<vk::raii::DescriptorSet>` of length `descriptorSetCount` |
| `vk::DescriptorBufferInfo` | Struct | `{buffer, offset, range}` — which buffer region this descriptor reads from |
| `vk::WriteDescriptorSet` | Struct | `{dstSet, dstBinding, dstArrayElement, descriptorCount, descriptorType, pBufferInfo/pImageInfo}` |
| `device.updateDescriptorSets(writes, copies)` | Method | Applies an array of writes (and optional copies) to populate sets |
| `commandBuffer.bindDescriptorSets(bindPoint, layout, firstSet, sets, dynamicOffsets)` | Method | Records binding into a command buffer |
| `vk::PipelineBindPoint::eGraphics` | Enum value | Binds the set for graphics pipeline reads |

### Code Walkthrough

**Creating the pool.** Build a single `DescriptorPoolSize{ eUniformBuffer, MAX_FRAMES_IN_FLIGHT }`. Wrap it in a `DescriptorPoolCreateInfo` with `maxSets = MAX_FRAMES_IN_FLIGHT`, `poolSizeCount = 1`, `pPoolSizes = &poolSize`, and optionally `flags = eFreeDescriptorSet`. Construct a `vk::raii::DescriptorPool`. Call this **after** `createUniformBuffers` but **before** `createDescriptorSets` in the init sequence.

**Allocating the sets.** `allocateDescriptorSets` requires an array of descriptor set layouts — one per set, even when they're identical. Build `std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);` to replicate the same layout N times, fill a `DescriptorSetAllocateInfo` pointing at the pool with `descriptorSetCount = N`, and call `device.allocateDescriptorSets(info)`. The returned vector is stored as a class member (`descriptorSets_`).

**Configuring the writes.** For each frame slot:
1. Build a `DescriptorBufferInfo{ buffer = *uniformBuffers[i], offset = 0, range = sizeof(UniformBufferObject) }`. `range = vk::WholeSize` is valid when the whole buffer is the descriptor.
2. Build a `WriteDescriptorSet{ dstSet = *descriptorSets[i], dstBinding = 0, dstArrayElement = 0, descriptorCount = 1, descriptorType = eUniformBuffer, pBufferInfo = &bufferInfo }`.
3. Call `device.updateDescriptorSets(write, nullptr)` — or batch all N writes into an array and make a single update call for efficiency.

`dstBinding = 0` must match the binding index declared in the descriptor set layout. `dstArrayElement` is only non-zero when updating a range inside a descriptor array. `descriptorCount` here is the number of array elements touched by *this write*, not the pool size.

**Binding during record.** Inside `GraphicsPipeline::record()` (or wherever the command buffer is populated), after `bindPipeline` and before `drawIndexed`, call `commandBuffer.bindDescriptorSets(eGraphics, *pipelineLayout_, 0, *descriptorSets_[currentFrame], nullptr)`:
- `firstSet = 0` — start binding at set slot 0.
- The fourth argument is the set (or array of sets) to bind; Vulkan-Hpp accepts a single `vk::DescriptorSet` via `ArrayProxy`.
- The last argument is the dynamic-offset array — empty here; used with dynamic uniform/storage buffers in later chapters.

**Rasterization winding fix.** The Y-flip in the projection matrix reverses screen-space winding. Change `rasterizer.frontFace` from `eClockwise` to `eCounterClockwise` in `createPipeline()`. If back-face culling is on and the winding isn't flipped, the rectangle disappears.

### Common Pitfalls

- **Pool too small.** `maxSets = 1` with `MAX_FRAMES_IN_FLIGHT = 2` allocation request → error. Similarly, `poolSizes[0].descriptorCount = 1` with 2 sets each needing a UBO descriptor → error. Size both to `MAX_FRAMES_IN_FLIGHT`.
- **`dstBinding` mismatch with layout.** Layout declared binding 0, write targets binding 1 → silent undefined behaviour at shader read time. The validation layer will usually catch this; don't ignore it.
- **Binding the wrong frame's set.** Using `descriptorSets[0]` instead of `descriptorSets[currentFrame]` in `bindDescriptorSets` serialises reads through a single set — defeats the per-frame pattern and creates a race with uniform updates.
- **Forgetting `updateDescriptorSets`.** Allocated sets are empty. Binding an unwritten set and drawing reads garbage. Validation layers will flag this.
- **Updating a set currently in flight.** The per-frame pattern avoids this by construction, but custom patterns must fence appropriately — a set whose buffer is still being read by the GPU cannot be re-pointed safely.
- **Winding order not updated.** After the Y-flip, geometry with back-face culling enabled and `eClockwise` front faces is invisible. Symptom: clear colour only, no rectangle. Fix: `eCounterClockwise`.
- **`pImmutableSamplers` left unset when needed.** For combined image samplers (Chapter 06) this field becomes load-bearing. For pure UBO descriptors, `nullptr` is correct.
- **Layout array length vs set count.** `allocateDescriptorSets` requires `layouts.size() == descriptorSetCount`. Passing a single-element array with `count = N` is a validation error even when all sets share one layout.

---

## Summary

After Chapter 05 the application:

- Defines a `UniformBufferObject` struct on the host and a matching UBO/ConstantBuffer block in the shader.
- Owns a `vk::raii::DescriptorSetLayout` declaring one UBO binding at slot 0, stage `eVertex`.
- Has a `vk::raii::PipelineLayout` that references the descriptor set layout (no longer empty).
- Owns `MAX_FRAMES_IN_FLIGHT` uniform buffers, each host-visible+coherent, each persistently mapped.
- Owns a `vk::raii::DescriptorPool` sized to `MAX_FRAMES_IN_FLIGHT` uniform-buffer descriptors.
- Owns `MAX_FRAMES_IN_FLIGHT` descriptor sets, each populated via `updateDescriptorSets` to point at its matching uniform buffer.
- Calls `updateUniformBuffer(currentFrame)` each frame before submit, computing MVP from a wall-clock time delta and the current swap-chain aspect ratio; writes via `memcpy` into the mapped pointer.
- Binds the current frame's descriptor set into the command buffer before `drawIndexed`.
- Uses `eCounterClockwise` as the pipeline's front-face winding to compensate for the projection Y-flip.

The rectangle from Chapter 04 now rotates smoothly around the Z axis, viewed from an isometric-ish angle, and maintains correct aspect ratio on resize.

## Implementation Checklist

- [ ] Add `UniformBufferObject { glm::mat4 model, view, proj; }` to a shared header.
- [ ] Update `shaders/triangle.slang` to declare a `ConstantBuffer<UniformBuffer>` and apply `proj * view * model` in the vertex entry point.
- [ ] Declare a `vk::raii::DescriptorSetLayout` member; build it with a single `DescriptorSetLayoutBinding{binding=0, type=eUniformBuffer, count=1, stage=eVertex}`.
- [ ] Update `PipelineLayoutCreateInfo` in `GraphicsPipeline::createPipeline()` to include the descriptor set layout.
- [ ] Flip `rasterizer.frontFace` to `eCounterClockwise`.
- [ ] Add three parallel vectors for uniform buffers, device memory, and mapped `void*` pointers, each sized `MAX_FRAMES_IN_FLIGHT`.
- [ ] Implement `createUniformBuffers()` — allocate host-visible+coherent buffers, `mapMemory` each one, store the pointer.
- [ ] Implement `createDescriptorPool()` — single `DescriptorPoolSize` of type `eUniformBuffer` × `MAX_FRAMES_IN_FLIGHT`, `maxSets = MAX_FRAMES_IN_FLIGHT`.
- [ ] Implement `createDescriptorSets()` — replicate the layout N times, `allocateDescriptorSets`, then loop to populate each with a `WriteDescriptorSet` targeting its matching uniform buffer.
- [ ] Implement `updateUniformBuffer(currentFrame)` — compute MVP from `high_resolution_clock` delta and current swap-chain extent; apply `ubo.proj[1][1] *= -1`; `memcpy` into `uniformBuffersMapped_[currentFrame]`.
- [ ] Call `updateUniformBuffer` from `drawFrame` before `submit`.
- [ ] In the pipeline's `record()`, add `bindDescriptorSets(eGraphics, *pipelineLayout_, 0, *descriptorSets_[currentFrame], nullptr)` after `bindPipeline`, before `drawIndexed`.
- [ ] Verify: rectangle rotates 90°/sec around Z, correct aspect ratio after resize, validation layers silent.

## Further Reading

- [Vulkan Spec — Descriptor Sets chapter](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html)
- [Vulkan Spec — Memory Allocation / mapMemory](https://docs.vulkan.org/spec/latest/chapters/memory.html#memory-device-hostaccess)
- [GLM documentation — matrix transforms](https://github.com/g-truc/glm)
- [Vulkan Guide — Descriptor Sets](https://vkguide.dev/docs/chapter-4/descriptors/) (for a second, more engine-oriented explanation)
- std140 layout rules: Vulkan Spec §15.6.4 "Offset and Stride Assignment"
