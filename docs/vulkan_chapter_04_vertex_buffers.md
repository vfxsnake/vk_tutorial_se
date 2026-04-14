# Chapter 04: Vertex Buffers

> **Source:** https://docs.vulkan.org/tutorial/latest/04_Vertex_buffers/
> **Vulkan Version:** 1.4 | **Language:** C++20 | **Shading:** Slang | **Bindings:** Vulkan-Hpp RAII

---

## Overview

Chapter 03 hardcoded triangle vertices directly inside the shader. That works for a proof-of-concept but it means geometry is baked into the GPU program — you cannot change it at runtime without recompiling the shader. Chapter 04 breaks that coupling by introducing **vertex buffers**: regions of GPU-accessible memory that hold vertex data the vertex shader reads from at draw time.

The chapter introduces three progressively more capable memory arrangements:

1. **Host-visible vertex buffer** — the simplest option. CPU writes directly into memory the GPU can also read. Easy, but slower for the GPU.
2. **Staged vertex buffer** — the production pattern. CPU writes into a temporary *staging buffer* in CPU-accessible memory, then a GPU copy command transfers the data into *device-local* memory that the GPU reads at full bandwidth.
3. **Index buffer** — an indirection layer sitting in front of the vertex buffer. Instead of duplicating shared vertices, an index buffer stores integer references into the vertex array, enabling vertex reuse across triangles.

By the end of this chapter the application renders a coloured rectangle built from two indexed triangles, with all geometry living in device-local GPU memory.

---

## §04.00 — Vertex Input Description

### Concepts

Until now the vertex shader owned its own vertex data — three hardcoded positions and colours were declared as `static` arrays inside the shader itself. The GPU had no need to read any external memory; the shader just indexed into its own arrays using `SV_VertexID`.

Vertex buffers change the contract. The application now owns the vertex data, stores it in a buffer object in GPU memory, and tells the pipeline at creation time exactly how that buffer is laid out. The vertex shader receives each vertex's data as a struct of input variables rather than computing it from a raw index.

This requires configuring two things in the graphics pipeline:

**Binding descriptions** describe *how* Vulkan should step through a buffer — how many bytes to advance per vertex, and whether to advance per-vertex or per-instance (for instanced rendering). There is one binding description per buffer bound to the pipeline.

**Attribute descriptions** describe *what* to extract from each vertex — which field in the struct maps to which shader input location, what its data format is, and at what byte offset within the vertex struct it lives. There is one attribute description per per-vertex input variable in the shader.

Together these two description types give Vulkan enough information to fetch the right bytes from the buffer and feed them into the right shader input variables automatically, before the shader body executes.

#### The `Vertex` struct

The C++ side defines a struct that mirrors the shader input struct. For a coloured triangle the minimum useful vertex contains a 2D position and an RGB colour:

```cpp
// GLM types: glm::vec2 = two floats, glm::vec3 = three floats
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
};
```

The shader counterpart in Slang uses the same layout:

```hlsl
struct VSInput {
    [vk::location(0)] float2 inPosition : POSITION;
    [vk::location(1)] float3 inColor    : COLOR;
};
```

The `[vk::location(N)]` attribute binds each shader input to the corresponding attribute description on the C++ side by index. Location 0 ↔ `pos`, location 1 ↔ `color`.

#### Binding Description

One `vk::VertexInputBindingDescription` per bound buffer:

```cpp
vk::VertexInputBindingDescription{
    .binding   = 0,                          // index 0: first (and only) buffer
    .stride    = sizeof(Vertex),             // bytes from one vertex to the next
    .inputRate = vk::VertexInputRate::eVertex // advance per vertex, not per instance
}
```

`inputRate::eInstance` is used for instanced rendering (one entry per object instance rather than per vertex). Not needed here.

#### Attribute Descriptions

Two `vk::VertexInputAttributeDescription` entries — one per field:

```cpp
// Attribute 0: position (two 32-bit floats)
vk::VertexInputAttributeDescription{
    .location = 0,
    .binding  = 0,
    .format   = vk::Format::eR32G32Sfloat,
    .offset   = offsetof(Vertex, pos)    // 0 bytes from start of Vertex
}

// Attribute 1: color (three 32-bit floats)
vk::VertexInputAttributeDescription{
    .location = 1,
    .binding  = 0,
    .format   = vk::Format::eR32G32B32Sfloat,
    .offset   = offsetof(Vertex, color)  // 8 bytes from start of Vertex (after vec2)
}
```

`offsetof(Vertex, field)` is a standard C macro that computes the byte offset of a struct member at compile time. It handles padding automatically.

#### Format–Type Mapping

The `format` field uses the same `vk::Format` enum that describes image formats (because formats describe memory layout, not the purpose of that memory). The naming pattern is `eR[bits]G[bits]...Sfloat` (signed float) or `Sint`/`Uint`:

| Shader type | `vk::Format` |
|-------------|-------------|
| `float`  | `eR32Sfloat` |
| `float2` | `eR32G32Sfloat` |
| `float3` | `eR32G32B32Sfloat` |
| `float4` | `eR32G32B32A32Sfloat` |
| `int2`   | `eR32G32Sint` |
| `uint4`  | `eR32G32B32A32Uint` |
| `double` | `eR64Sfloat` |

If you use fewer components than the format provides, extra channels are silently discarded. If you use more than the format provides, missing components default to `(0, 0, 0, 1)` — which can produce subtle colour bugs if you specify `float3` in the shader but only provide `eR32G32Sfloat`.

#### Pipeline Integration

The graphics pipeline's `PipelineVertexInputStateCreateInfo` currently points to empty arrays (no vertex input at all, because the old shader needed none). It is updated to reference the binding and attribute descriptions. This struct does not copy the data — it stores raw pointers, so the binding and attribute description objects must remain alive for the duration of the `createGraphicsPipeline()` call.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::VertexInputBindingDescription` | Struct | Per-buffer: binding index, stride, input rate |
| `vk::VertexInputAttributeDescription` | Struct | Per-attribute: location, binding, format, offset |
| `vk::VertexInputRate::eVertex` | Enum value | Advance one entry per vertex (vs. per instance) |
| `vk::PipelineVertexInputStateCreateInfo` | Struct | Plugs binding + attribute descriptions into the pipeline |
| `offsetof(T, field)` | C macro | Compile-time byte offset of a struct member |

### Code Walkthrough

The `Vertex` struct is defined in application code (not in the shader). It has two static methods that return the binding and attribute descriptions — this keeps the pipeline creation code decoupled from the internal layout of `Vertex`. If the struct gains a UV coordinate later, only the `Vertex` methods change.

The shader is updated to remove the `static` arrays and the index-based lookup. The `[shader("vertex")]` entry point now receives a `VSInput` parameter, and the two values (`inPosition`, `inColor`) are read directly from it. The fragment shader's `VSOutput` struct is unchanged.

In `createPipeline()`, the `PipelineVertexInputStateCreateInfo` that was previously zeroed out (empty vertex input) is now populated with one binding description and two attribute descriptions. Nothing else in the pipeline changes — the rest of the fixed-function and shader stage configuration is identical.

The `vertices` array is declared at application scope so both the pipeline setup and the (upcoming) buffer creation can reference it.

### Common Pitfalls

- **Location mismatch** — if the C++ attribute description says location 0 is `eR32G32Sfloat` but the shader declares location 0 as `float3`, validation layers will emit a warning and the GPU may read garbage data.
- **Forgetting to update the shader** — the shader still works after updating the pipeline struct, but it will receive zeros at all inputs (no vertex buffer is bound yet). The triangle disappears. This is expected during incremental implementation.
- **Pointer lifetime** — `pVertexBindingDescriptions` and `pVertexAttributeDescriptions` are raw pointers. The objects must not go out of scope before `createGraphicsPipeline()` returns. Storing them as local variables in the same scope as the `GraphicsPipelineCreateInfo` is sufficient.
- **`offsetof` on non-standard-layout types** — `offsetof` is only well-defined on standard-layout structs. Any struct that inherits from another, has virtual methods, or uses `private` members may not qualify. Keep `Vertex` a plain aggregate.

---

## §04.01 — Vertex Buffer Creation

### Concepts

Declaring the vertex data format is separate from actually allocating the memory that holds it. This section creates a `vk::raii::Buffer` and backs it with allocated `vk::raii::DeviceMemory`.

#### Buffers and Memory in Vulkan

This is one of the more explicit aspects of the Vulkan API: **a buffer object and the memory it uses are two separate things**. You create a buffer, query what kind of memory it needs, allocate that memory separately, then bind the memory to the buffer. The reason is flexibility — you might allocate one large block of device memory and suballocate several buffers from it using offsets.

A `vk::raii::Buffer` is just a description of what you want (size, usage flags, sharing mode). It has no storage until you call `bindMemory()`.

#### Memory Types

Physical devices expose a list of *memory types*, each belonging to a *memory heap* (a pool of memory of a specific total size). Memory types carry flags that describe their properties:

| Flag | Meaning |
|------|---------|
| `eDeviceLocal` | Lives in GPU VRAM — fastest for the GPU to read, not CPU-accessible |
| `eHostVisible` | CPU can map this memory and write to it via a regular pointer |
| `eHostCoherent` | Writes are automatically visible to the GPU without explicit flushes |
| `eHostCached` | CPU reads are cached (useful for readback) |

For this first buffer implementation, you need `eHostVisible | eHostCoherent` — the CPU writes vertex data into it, and you want those writes to be visible to the GPU immediately without a manual `vkFlushMappedMemoryRanges` call.

`eHostCoherent` trades off a small amount of CPU write bandwidth for simplicity. Flushing manually (without coherent) is slightly faster in theory but adds synchronisation complexity that isn't worth it at this stage.

#### Finding the Right Memory Type

`vk::PhysicalDevice::getMemoryProperties()` returns a list of memory types. Choosing among them requires two checks:

1. **Compatibility**: The buffer's `memoryTypeBits` field is a bitmask. Bit `i` is set if memory type `i` is compatible with this buffer's usage. You must only pick a type whose bit is set.
2. **Properties**: The chosen type's `propertyFlags` must include all the flags you need (i.e., `(flags & required) == required`).

This is encapsulated in a `findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)` helper that throws if no suitable type exists.

#### Mapping, Copying, Unmapping

Once memory is allocated and bound, you call `mapMemory(offset, size)` to get a `void*` pointer into the memory region. You `memcpy` your CPU-side vertex array into it, then `unmapMemory()`. After this the buffer contains your vertex data and the pointer is no longer valid.

You do not need to keep the memory mapped between frames. This is a one-time upload — the vertex data is static geometry.

#### Binding During Recording

Before `draw()` you call `bindVertexBuffers(firstBinding, buffers, offsets)`. The `firstBinding` matches the `.binding` index in the `VertexInputBindingDescription`. The offset array (one entry per buffer) is usually all zeros.

`draw()` must use `vertices.size()` as the vertex count now, not the hardcoded `3` from the previous chapter.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::BufferCreateInfo` | Struct | Specifies size, usage, sharing mode |
| `vk::raii::Buffer` | RAII handle | Owns the buffer object (not the memory) |
| `vk::MemoryRequirements` | Struct | size, alignment, and memoryTypeBits |
| `Buffer::getMemoryRequirements()` | Method | Queries what memory the buffer needs |
| `vk::PhysicalDevice::getMemoryProperties()` | Method | Returns all memory types and heaps |
| `vk::MemoryAllocateInfo` | Struct | Allocation size + chosen memory type index |
| `vk::raii::DeviceMemory` | RAII handle | Owns a GPU memory allocation |
| `DeviceMemory::mapMemory()` | Method | Returns a CPU-writable pointer into the allocation |
| `DeviceMemory::unmapMemory()` | Method | Invalidates the CPU pointer |
| `Buffer::bindMemory(memory, offset)` | Method | Binds allocated memory to the buffer |
| `CommandBuffer::bindVertexBuffers()` | Method | Binds one or more vertex buffers before draw |
| `vk::MemoryPropertyFlagBits::eHostVisible` | Flag | Memory is CPU-mappable |
| `vk::MemoryPropertyFlagBits::eHostCoherent` | Flag | Writes visible to GPU without manual flush |

### Code Walkthrough

`createVertexBuffer()` is a new method called during initialisation, after the command pool is created and before command buffers are recorded. It does five things in sequence: creates the buffer object with `eVertexBuffer` usage, queries its memory requirements, calls `findMemoryType()` to pick the right heap, allocates device memory, and binds the memory to the buffer. Then it maps, copies, and unmaps.

`findMemoryType()` is a private helper. It loops over `memProperties.memoryTypeCount`, checks the compatibility bitmask, and checks the property flags. It throws `std::runtime_error` if nothing matches — this indicates a driver or platform that does not support the required combination, which should never happen on any hardware that supports Vulkan.

In `recordCommandBuffer()`, two lines change: `bindVertexBuffers()` is inserted before `draw()`, and the vertex count argument changes from `3` to `vertices.size()`.

### Common Pitfalls

- **Allocating memory before binding**: Memory must be allocated *and then* bound before the buffer is used. Forgetting `bindMemory()` results in a validation error when you try to bind the buffer in the command buffer.
- **Using the buffer after unmapping**: `unmapMemory()` invalidates the CPU pointer. Accessing it afterward is undefined behaviour (no Vulkan error, just a crash or garbage read).
- **Not waiting for the queue**: After submitting a command buffer that uses this vertex buffer, the GPU may still be reading from it. For a static vertex buffer this is fine — you only write to it once during init and never touch it again. But if you ever update a mapped buffer while the GPU is reading it, you need synchronisation.
- **Exceeding `maxMemoryAllocationCount`**: The Vulkan spec allows implementations to limit the number of simultaneous `vkAllocateMemory` calls (as low as 4096). One allocation per buffer quickly hits this limit. The staging buffer section notes this explicitly and recommends VulkanMemoryAllocator for production.

---

## §04.02 — Staging Buffer

### Concepts

The vertex buffer from §04.01 works but uses memory flagged `eHostVisible | eHostCoherent`. On discrete GPUs this type typically lives in CPU-accessible VRAM or system RAM — not in the GPU's fastest, dedicated VRAM. The GPU is forced to read vertex data across the PCIe bus every time it executes a draw call.

The correct production pattern is:

1. Create a **staging buffer** in `eHostVisible | eHostCoherent` memory. This is a temporary upload buffer.
2. Write the vertex data into the staging buffer from the CPU.
3. Create a **vertex buffer** in `eDeviceLocal` memory. This is the real render buffer that lives in fast GPU VRAM. It is *not* CPU-accessible.
4. Issue a GPU copy command that reads from the staging buffer and writes to the vertex buffer.
5. Destroy the staging buffer. It was only needed for the upload.

After step 4, the vertex data is in device-local memory. Every subsequent draw reads it at full GPU bandwidth with no PCIe involvement.

#### Transfer Operations

Copying from one buffer to another is a *transfer operation*. The good news: any queue family that supports `eGraphics` or `eCompute` implicitly supports transfer operations (the `eTransfer` capability is always included). You can issue the copy on the same graphics queue without needing a separate transfer queue.

You mark the buffers' intended roles with usage flags:
- `eTransferSrc` — this buffer will be the *source* of a copy operation
- `eTransferDst` — this buffer will be the *destination* of a copy operation

The staging buffer gets `eTransferSrc`. The vertex buffer gets `eTransferDst | eVertexBuffer` (it is both a copy destination and a vertex buffer).

#### `createBuffer` Helper

Both the staging buffer and the vertex buffer need the same creation steps: `BufferCreateInfo`, `getMemoryRequirements()`, `findMemoryType()`, `MemoryAllocateInfo`, `bindMemory()`. To avoid duplication, a `createBuffer(size, usage, properties)` helper is extracted that returns (or outputs) the buffer and its memory together.

#### `copyBuffer` Helper

The GPU copy is a command buffer operation. `copyBuffer` must:

1. Allocate a temporary command buffer from the command pool (with `eTransient` flag set on the pool, or just use the existing pool).
2. Begin recording with `eOneTimeSubmit` — tells the driver this buffer is submitted once and discarded.
3. Record `vk::BufferCopy{ .srcOffset=0, .dstOffset=0, .size=size }` using `cmd.copyBuffer(src, dst, regions)`.
4. End recording.
5. Submit to the graphics queue.
6. Call `queue.waitIdle()` to wait for the copy to complete before returning.
7. The RAII command buffer is destroyed automatically at end of scope.

`eOneTimeSubmit` is a hint to the driver that it should not cache this command buffer for reuse — important for transient operations.

Note that `copyBuffer` uses `queue.waitIdle()` rather than a fence or semaphore. This is the simplest correct synchronisation for a one-time upload during init — it stalls the CPU until the GPU finishes the copy. For production streaming uploads you would use a fence so the CPU can do other work while the GPU copies, but that complexity is not needed here.

#### Why Not `VK_WHOLE_SIZE` for Copy Regions?

When mapping memory, you can pass `VK_WHOLE_SIZE` to mean "the entire allocation". For `vkCmdCopyBuffer`, you cannot — the `size` field in `vk::BufferCopy` must be an exact byte count. This is a common confusion point.

#### VulkanMemoryAllocator (VMA)

The tutorial explicitly calls out that production applications should not call `vkAllocateMemory` individually for each buffer. Implementations may limit simultaneous allocations to as few as 4096. The recommended approach is to:

- Allocate one large block of device memory.
- Suballocate regions of it for individual buffers using offsets.

Implementing a custom allocator is significant work. The GPU Open **VulkanMemoryAllocator** (VMA) library handles this entirely and is the industry-standard solution. It also handles alignment requirements, buffer-image granularity, and memory type selection automatically. This is noted as future work — for the tutorial scope, one allocation per buffer is acceptable.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::MemoryPropertyFlagBits::eDeviceLocal` | Flag | Fast GPU-local VRAM, not CPU-accessible |
| `vk::BufferUsageFlagBits::eTransferSrc` | Flag | Buffer may be source of a copy command |
| `vk::BufferUsageFlagBits::eTransferDst` | Flag | Buffer may be destination of a copy command |
| `vk::CommandBufferUsageFlagBits::eOneTimeSubmit` | Flag | Command buffer submitted once and discarded |
| `vk::BufferCopy` | Struct | Defines srcOffset, dstOffset, size for a buffer copy |
| `CommandBuffer::copyBuffer(src, dst, regions)` | Method | Records a GPU-side buffer-to-buffer copy |
| `Queue::waitIdle()` | Method | Blocks CPU until all queued work on this queue completes |

### Code Walkthrough

`createBuffer()` is a private helper that accepts size, usage flags, and memory property flags. It creates the buffer, queries requirements, finds the memory type, allocates, and binds. It stores the resulting `vk::raii::Buffer` and `vk::raii::DeviceMemory` as output parameters or as a returned pair.

`copyBuffer()` is a private helper that accepts source buffer, destination buffer, and size. It allocates a single command buffer from the command pool, records the copy, submits it, and calls `waitIdle()`. The command buffer is a local RAII object and is automatically freed when the function returns.

`createVertexBuffer()` is restructured: it calls `createBuffer()` twice (once for staging, once for the final vertex buffer), maps and copies into the staging buffer, calls `copyBuffer()`, then the staging buffer and its memory fall out of scope and are automatically destroyed.

The vertex buffer and its memory become class members so they live for the lifetime of the application.

### Common Pitfalls

- **Wrong usage flags on the staging buffer**: The staging buffer must have `eTransferSrc`. If it only has `eHostVisible` but not `eTransferSrc`, the copy command is invalid (validation error).
- **Destroying the staging buffer too early**: The staging buffer must survive until `copyBuffer()` completes (i.e., until after `waitIdle()`). If it is destroyed before the GPU finishes the copy, the GPU reads from freed memory.
- **Device-local memory with no `eTransferDst`**: The vertex buffer must have `eTransferDst` in addition to `eVertexBuffer`. Without it, the copy command writes to a buffer not flagged as a copy destination — validation error.
- **Reusing a command pool without `eResetCommandBuffer`**: If your command pool was created with `eResetCommandBuffer`, individual buffers can be reset and reused. If not, the whole pool must be reset. For the `copyBuffer` use case, the simplest approach is to allocate a fresh command buffer each time (the pool handles the underlying memory, so short-lived allocations are cheap).

---

## §04.03 — Index Buffer

### Concepts

The vertex buffer stores unique vertices. But triangulated geometry reuses vertices constantly — every interior edge of a mesh is shared by two triangles, meaning both triangles need the same vertex. Duplicating vertices wastes memory and, worse, forces the GPU to process the same vertex multiple times.

An **index buffer** is an array of integer indices into the vertex buffer. Instead of listing all six vertices of a rectangle (with two duplicates), you list four unique vertices and an index array `{0, 1, 2, 2, 3, 0}` that refers to them. The GPU looks up each index in the vertex buffer to retrieve the actual vertex data.

Real-world impact: for a complex mesh with many triangles, "vertices are reused in an average number of three triangles." With indices you store each vertex once; without indices you store it three times on average.

#### Index Type

The index buffer contains either `uint16_t` (16-bit) or `uint32_t` (32-bit) integers. The choice is made at `bindIndexBuffer()` time via the `vk::IndexType::eUint16` or `eUint32` enum.

- `uint16_t`: Maximum 65 535 unique vertices. Saves memory and can be faster (smaller transfers, better cache). Use this unless you need more vertices.
- `uint32_t`: Maximum ~4 billion unique vertices. Required for complex geometry (OBJ loading in Chapter 08 will need this).

#### Rectangle Data

The tutorial upgrades from a triangle to a rectangle at this point:

- Four unique vertices: top-left, top-right, bottom-right, bottom-left (each with a colour).
- Indices `{0, 1, 2, 2, 3, 0}`: first triangle uses vertices 0, 1, 2; second triangle reuses 2 and 0 and adds 3.

The winding order matters for face culling — the index order must be consistent with the `eFrontFace` setting in the pipeline's rasterisation state.

#### Creating the Index Buffer

Index buffer creation is identical to vertex buffer creation: allocate a staging buffer, copy the index array into it, create a device-local index buffer with `eIndexBuffer | eTransferDst` usage, copy from staging to device-local, destroy the staging buffer. The `createBuffer()` helper written in §04.02 is reused here.

#### Recording Changes

Two changes to command buffer recording:

1. `cmd.bindIndexBuffer(indexBuffer, 0, vk::IndexType::eUint16)` — after binding the vertex buffer. The `0` is the byte offset into the index buffer.
2. `cmd.draw()` is replaced by `cmd.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance)`:
   - `indexCount`: total number of indices (e.g., 6 for a rectangle).
   - `instanceCount`: 1 (no instancing).
   - `firstIndex`: 0 (start from the beginning of the index buffer).
   - `vertexOffset`: 0 (offset added to each index before looking up in the vertex buffer — useful when packing multiple meshes into one vertex buffer).
   - `firstInstance`: 0.

#### Driver Recommendation: Co-locate Buffers

The tutorial surfaces a driver developer recommendation: store the vertex buffer and index buffer inside a **single `vk::Buffer`** and use offsets to locate each one. This improves cache coherency because the GPU fetches the vertex and index data from nearby memory locations. The `vertexOffset` parameter in `drawIndexed()` and the `offset` parameter in `bindIndexBuffer()` exist specifically to support this pattern.

This is an optimisation to apply later — the tutorial keeps them separate for clarity.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::BufferUsageFlagBits::eIndexBuffer` | Flag | Buffer is used as an index buffer |
| `CommandBuffer::bindIndexBuffer(buffer, offset, indexType)` | Method | Binds the index buffer before indexed draw |
| `vk::IndexType::eUint16` / `eUint32` | Enum | Index data type (affects max vertex count) |
| `CommandBuffer::drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance)` | Method | Indexed draw call — replaces `draw()` |

### Code Walkthrough

`createIndexBuffer()` follows the exact same staging pattern as `createVertexBuffer()`. The usage flags differ: `eIndexBuffer | eTransferDst` instead of `eVertexBuffer | eTransferDst`.

In `recordCommandBuffer()`, three changes apply: `bindIndexBuffer()` is added after `bindVertexBuffers()`, `draw()` is replaced with `drawIndexed()` using the index count (6) and all other params at 0 or 1, and the `vertices` array is updated to four entries (the rectangle corners).

The index and vertex buffers each remain separate class members. They are destroyed in reverse order of creation (RAII handles this automatically when the enclosing object is destroyed).

### Common Pitfalls

- **Calling `draw()` instead of `drawIndexed()`**: After binding an index buffer, you must use `drawIndexed()`. Calling `draw()` ignores the index buffer entirely and draws from the vertex buffer sequentially — you get a triangle, not a rectangle.
- **Wrong `indexCount`**: Passing `vertices.size()` instead of `indices.size()` is a common mistake. The index count is the number of *indices* (6 for two triangles), not the number of vertices (4).
- **Index type mismatch**: If your index array uses `uint16_t` but you pass `vk::IndexType::eUint32` to `bindIndexBuffer()`, the GPU interprets pairs of 16-bit values as single 32-bit integers, producing garbage index values and a validation warning.
- **Forgetting `eTransferDst` on the index buffer**: Same as for the vertex buffer — the staging copy fails without this flag.
- **Winding order with the rectangle**: The index order `{0,1,2, 2,3,0}` must be consistent with front-face convention. If the rectangle appears blank (back-face culled away), flip the winding: `{0,2,1, 1,2,3}` or change the pipeline's `frontFace` setting.

---

## Summary

- The `Vertex` struct combines GLM position and colour fields. Static methods return the binding and attribute descriptions needed by the pipeline.
- The graphics pipeline's `PipelineVertexInputStateCreateInfo` is updated from empty to reference the `Vertex` binding and attribute descriptions.
- The vertex shader changes from hardcoded arrays + `SV_VertexID` to receiving a `VSInput` struct populated by the pipeline from buffer memory.
- A `vk::raii::Buffer` and `vk::raii::DeviceMemory` are two separate objects. The buffer describes what you need; the memory is the actual allocation. You bind one to the other.
- Vulkan's memory system exposes multiple types per physical device. For a GPU-readable, CPU-writable buffer use `eHostVisible | eHostCoherent`. For the fastest GPU reads use `eDeviceLocal` (requires a staging copy).
- The staging buffer pattern: CPU writes into a host-visible staging buffer → GPU copies to a device-local buffer via `copyBuffer()` → staging buffer is destroyed. All subsequent draws read from fast VRAM.
- Index buffers store integer references into the vertex buffer, enabling vertex reuse across triangles. They are created identically to vertex buffers, using `eIndexBuffer | eTransferDst` usage and a staging upload.
- `drawIndexed()` replaces `draw()` once an index buffer is bound. The key parameter is `indexCount` (number of indices), not vertex count.

## Implementation Checklist

- [ ] Define `Vertex` struct with `glm::vec2 pos` and `glm::vec3 color`
- [ ] Add `getBindingDescription()` static method to `Vertex`
- [ ] Add `getAttributeDescriptions()` static method to `Vertex`
- [ ] Update `PipelineVertexInputStateCreateInfo` in `GraphicsPipeline` to use vertex descriptions
- [ ] Update `triangle.slang` shader: replace hardcoded arrays with `VSInput` struct and `[vk::location]` attributes
- [ ] Implement `findMemoryType()` helper
- [ ] Implement `createBuffer()` helper (size, usage, memory properties)
- [ ] Implement `copyBuffer()` helper (src, dst, size — allocates transient command buffer)
- [ ] Implement `createVertexBuffer()` using staging pattern (staging → device-local)
- [ ] Implement `createIndexBuffer()` using staging pattern
- [ ] Update command buffer recording: `bindVertexBuffers()`, `bindIndexBuffer()`, `drawIndexed()`
- [ ] Update `vertices` array to four rectangle corners
- [ ] Add `indices` array `{0, 1, 2, 2, 3, 0}` for two triangles
- [ ] Confirm coloured rectangle renders (not a triangle)

## Further Reading

- [Vulkan Spec — vkCreateBuffer](https://docs.vulkan.org/spec/latest/chapters/resources.html#vkCreateBuffer)
- [Vulkan Spec — vkAllocateMemory](https://docs.vulkan.org/spec/latest/chapters/memory.html#vkAllocateMemory)
- [Vulkan Spec — vkBindBufferMemory](https://docs.vulkan.org/spec/latest/chapters/resources.html#vkBindBufferMemory)
- [Vulkan Spec — vkCmdCopyBuffer](https://docs.vulkan.org/spec/latest/chapters/copies.html#vkCmdCopyBuffer)
- [Vulkan Spec — vkCmdDrawIndexed](https://docs.vulkan.org/spec/latest/chapters/drawing.html#vkCmdDrawIndexed)
- [GPU Open — VulkanMemoryAllocator](https://gpuopen.com/vulkan-memory-allocator/)
- [Vulkan Spec — Memory Types and Heaps](https://docs.vulkan.org/spec/latest/chapters/memory.html#memory-device)
