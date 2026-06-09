# Chapter 16: Rendering Multiple Objects

> **Source:** https://docs.vulkan.org/tutorial/latest/16_Multiple_Objects.html
> **Vulkan Version:** 1.4 | **Language:** C++20 | **Shading:** Slang | **Bindings:** Vulkan-Hpp RAII

---

## Overview

Up to this point the application draws a single model with a single set of GPU resources: one uniform buffer per frame, one descriptor set per frame. Chapter 16 extends this to handle multiple distinct objects — each with its own position, orientation, and scale — while sharing as much GPU state as possible. The central skill being taught is **resource classification**: identifying which Vulkan objects can be shared across all objects in the scene and which must be replicated per object.

This chapter is also where the `scene/` placeholder folder starts to earn its keep. The tutorial introduces a `GameObject` struct that combines transform data (CPU) with GPU handles (uniform buffers, descriptor sets). In our modular architecture, the two concerns will be separated — the architecture discussion will decide exactly how.

---

## Section 1 — Shared vs. Per-Object Resources

### Concepts

The key insight is that Vulkan resources fall into two categories relative to scene objects:

**Shared resources** are created once and used as-is for every object in the scene. They do not change when rendering a different object. Examples: the vertex buffer and index buffer (assuming all objects share the same mesh), the texture image and sampler, the graphics pipeline, and the pipeline layout.

**Per-object resources** must be distinct for each object because they carry data that differs between objects. The primary per-object resource is the **uniform buffer** — specifically the model matrix inside the `UniformBufferObject`. Each object needs its own UBO (one per frame-in-flight) so that its unique model transform can be uploaded to the GPU without overwriting another object's data. Because each object has its own UBO, it also needs its own **descriptor set** pointing at that UBO.

The view matrix (camera position) and projection matrix are the same for every object in a given frame, so they are computed once and stuffed into each object's UBO during the CPU update step. Only the model matrix differs per object.

This classification principle — share everything that doesn't vary per object, replicate only what must — is fundamental to efficient Vulkan rendering and applies at every scale from a handful of objects up to tens of thousands.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::DescriptorPoolCreateInfo::maxSets` | Field | Total descriptor sets the pool can allocate; must be `MAX_OBJECTS × MAX_FRAMES_IN_FLIGHT` |
| `vk::DescriptorPoolSize::descriptorCount` | Field | Number of descriptors of a given type; must match the new `maxSets` |
| `vk::DescriptorSetAllocateInfo` | Struct | Drives `allocateDescriptorSets` — one call per object, allocating `MAX_FRAMES_IN_FLIGHT` sets at once |
| `device.updateDescriptorSets()` | Function | Writes the UBO and image handles into each object's descriptor sets |

### Code Walkthrough

The tutorial starts by defining a `GameObject` struct. It holds three GLM vectors for transform properties — position, rotation, and scale — plus a helper method `getModelMatrix()` that composes them into a single `glm::mat4` using `glm::translate`, three `glm::rotate` calls (one per axis), and `glm::scale`. This method is called every frame, so the model matrix is computed fresh from the transform properties rather than cached.

Inside the struct, the GPU resources live alongside the transform: a `std::vector<vk::raii::Buffer>` for uniform buffers (one per frame-in-flight), a matching vector of `vk::raii::DeviceMemory`, a vector of `void*` mapped pointers for persistent CPU-side write access, and a `std::vector<vk::raii::DescriptorSet>` for descriptor sets.

The application then declares a fixed-size array of `GameObject` — `std::array<GameObject, MAX_OBJECTS>` — and a `setupGameObjects()` function that positions them at different coordinates, with different rotations and scales, so all three appear as distinct objects in the scene.

**Uniform buffer creation** must be updated: instead of creating `MAX_FRAMES_IN_FLIGHT` buffers for the single former object, the loop now iterates over all game objects and creates `MAX_FRAMES_IN_FLIGHT` buffers for each. Each buffer is the same size (`sizeof(UniformBufferObject)`) and allocation flags (`eHostVisible | eHostCoherent`) as before.

**Descriptor pool sizing** is the first place a subtle multiplication appears. The pool's `maxSets` must be `MAX_OBJECTS × MAX_FRAMES_IN_FLIGHT` rather than `MAX_FRAMES_IN_FLIGHT`, because the pool is the total reservoir for all descriptor set allocations from all objects across all frames. The `descriptorCount` in each `vk::DescriptorPoolSize` entry grows by the same factor — if we have both UBO and combined-image-sampler entries, each is multiplied by `MAX_OBJECTS`.

**Descriptor set creation** uses a nested loop. The outer loop iterates over each game object. For each object, `allocateDescriptorSets` is called with a layout vector of `MAX_FRAMES_IN_FLIGHT` copies of the descriptor set layout, producing `MAX_FRAMES_IN_FLIGHT` descriptor sets at once. An inner loop then fills them in: it builds a `vk::DescriptorBufferInfo` pointing at the object's own UBO for frame `i`, and a `vk::DescriptorImageInfo` pointing at the shared texture sampler and image view. Both are written via `updateDescriptorSets`. After this function, every object has its own set of descriptor sets, each wired to its own UBO but sharing the same texture handles.

**Uniform buffer update** is now a loop over all objects. View and projection are computed once per frame (outside the loop), because they are the same for every object. The model matrix is computed per object by calling `gameObject.getModelMatrix()`, optionally composed with an additional "initial rotation" to correct a mesh's inherent orientation (the tutorial applies a 90° X-axis rotation to bring the viking room from its file orientation into world space). The assembled `UniformBufferObject` is then `memcpy`'d into the object's mapped memory pointer for the current frame.

**Command buffer recording** changes in a focused way. The vertex buffer and index buffer are bound once before the object loop — they are shared, so there is no reason to rebind per object. The loop then iterates over objects: for each, it calls `bindDescriptorSets` with that object's descriptor set for the current frame index, then issues a single `drawIndexed` call. The vertex/index data is identical for every draw — only the descriptor set (and therefore the model matrix it points to) differs.

### Common Pitfalls

**Under-sizing the descriptor pool.** Forgetting to multiply `maxSets` and `descriptorCount` by `MAX_OBJECTS` causes `allocateDescriptorSets` to fail at runtime with `VK_ERROR_OUT_OF_POOL_MEMORY`. The validation layers will call this out.

**Frame index vs. object index confusion.** Each game object holds a vector of `MAX_FRAMES_IN_FLIGHT` descriptor sets and buffers, indexed by frame. The rendering loop must always index by `currentFrame` (or whatever the active frame slot index is), not by object index.

**Mapped pointer lifetime.** The `void*` mapped pointers stored in the `GameObject` remain valid only as long as the associated `DeviceMemory` is not destroyed. Destroying or moving the `DeviceMemory` without unmapping first, or accessing the pointer after destruction, is undefined behaviour. Our existing pattern (persistent map, copy per frame, never unmap) is safe as long as the memory object outlives the pointer.

**Shared texture handles per descriptor set.** The tutorial wires the same `textureSampler` and `textureImageView` into every object's descriptor sets. This is intentional — all three objects share one texture. If different objects need different textures, each descriptor set would need its own `vk::DescriptorImageInfo` pointing at the appropriate texture. That generalisation is natural to add once the architecture discussion for Chapter 16 has settled on how objects reference their materials.

---

## Section 2 — Performance Considerations & Extensions

### Concepts

The tutorial ends with guidance on scaling this approach beyond a handful of objects.

**State-change minimisation.** Vulkan command recording has a CPU cost per command. Changing pipeline state (`bindPipeline`), descriptor sets, vertex buffers, or index buffers all incur driver work. For many objects, grouping draws by material (same pipeline + same textures) reduces the number of state changes dramatically. Within a material group, only the descriptor set needs to rebind per object.

**Instancing.** When objects share the same mesh and material and differ only in transform, `drawIndexed` accepts an instance count parameter. The vertex shader receives a built-in instance ID that it can use to index into an array of model matrices — passed either via a storage buffer or via an instanced vertex attribute. The CPU issues a single draw call, and the GPU spawns one invocation per instance. This is orders of magnitude more efficient for a large number of identical objects (grass blades, trees, particles).

**Push constants.** For small data that changes per draw call — a single model matrix is 64 bytes — push constants bypass descriptor sets entirely. The data is embedded directly in the command stream with `pushConstants()`. Push constants have a guaranteed minimum size of 128 bytes across all Vulkan implementations, making them a natural fit for a single `mat4`. However, using push constants changes the pipeline layout, so this is an architecture decision rather than a drop-in swap.

**Indirect drawing.** `drawIndexedIndirect` reads draw parameters (index count, instance count, offsets) from a GPU buffer rather than CPU-supplied arguments. This enables the GPU to generate its own draw commands — for example, after a frustum-culling compute pass — without any round-trip to the CPU. This is an advanced technique relevant to high-performance scene rendering.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `commandBuffer.drawIndexed(indexCount, instanceCount, ...)` | Function | `instanceCount > 1` enables instancing; GPU spawns one VS invocation per instance |
| `commandBuffer.pushConstants(layout, stageFlags, offset, size, data)` | Function | Uploads small data directly into the command buffer — no descriptor set needed |
| `commandBuffer.drawIndexedIndirect(buffer, offset, drawCount, stride)` | Function | GPU-driven draw: parameters come from a buffer written by a prior compute pass |
| `vk::PushConstantRange` | Struct | Declares push constant block in the pipeline layout: stage flags, byte offset, byte size |

### Code Walkthrough

This section of the tutorial is conceptual rather than implementing the advanced techniques in full. It describes the trade-offs and points toward the next evolution of the renderer. No new function implementations are introduced. The multi-object loop from Section 1 is the working implementation; the performance section explains when and why you would replace or augment it.

The instancing note is worth internalising: the current loop issues `MAX_OBJECTS` separate `drawIndexed` calls, each binding a different descriptor set. This is correct and adequate for a small, fixed number of distinct objects. As soon as objects start sharing meshes and materials, instancing becomes the right tool. Our architecture discussion will need to decide whether the `scene/` layer should expose an abstraction that lets the renderer detect when instancing is applicable.

### Common Pitfalls

**Push constants and pipeline layout compatibility.** If you add push constants to the shader, the pipeline layout must declare a matching `vk::PushConstantRange`. Any pipeline created with a layout that does not include the push constant range will fail validation when you call `pushConstants`. Layout changes also invalidate all pipelines built with the old layout.

**Instance index in the vertex shader.** In Slang/GLSL, `SV_InstanceID` starts at 0 for the first instance in a non-indirect draw, but in an indirect draw it may be offset by `firstInstance`. If instanced data is fetched from a storage buffer using `SV_InstanceID`, ensure the offset arithmetic accounts for this.

---

## Summary

- **Shared resources** (mesh, texture, pipeline) are created once and reused across all objects.
- **Per-object resources** (uniform buffer, descriptor set) are replicated per object × per frame-in-flight.
- The descriptor pool's `maxSets` and `descriptorCount` must be scaled by `MAX_OBJECTS`.
- Command buffer recording binds shared vertex/index buffers once; the per-object loop only rebinds descriptor sets and reissues `drawIndexed`.
- View and projection matrices are computed once per frame; only the model matrix differs per object.
- For identical objects, instancing collapses N draw calls into one.
- Push constants offer a low-latency path for small per-draw data (up to 128 bytes).
- Indirect drawing enables GPU-driven culling and draw generation — the far end of the performance spectrum.

## Implementation Checklist

- [ ] Define a transform-and-GPU-resource container (tutorial calls it `GameObject`)
- [ ] `setupObjects()` — position, rotate, scale each object differently
- [ ] Per-object UBO allocation: `MAX_OBJECTS × MAX_FRAMES_IN_FLIGHT` buffers
- [ ] Update descriptor pool: `maxSets` and `descriptorCount` scaled by `MAX_OBJECTS`
- [ ] Per-object descriptor set allocation and population
- [ ] `updateUniformBuffers()` loop: view/proj shared, model computed per object
- [ ] Command buffer: bind vertex/index once, then per-object bind descriptor set + draw

## Further Reading

- [Vulkan Spec — vkAllocateDescriptorSets](https://registry.khronos.org/vulkan/specs/latest/man/html/vkAllocateDescriptorSets.html)
- [Vulkan Spec — vkCmdDrawIndexed (instanceCount)](https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdDrawIndexed.html)
- [Vulkan Spec — Push Constant Ranges](https://registry.khronos.org/vulkan/specs/latest/man/html/VkPushConstantRange.html)
- [Vulkan Spec — vkCmdPushConstants](https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdPushConstants.html)
- [Vulkan Spec — vkCmdDrawIndexedIndirect](https://registry.khronos.org/vulkan/specs/latest/man/html/vkCmdDrawIndexedIndirect.html)
