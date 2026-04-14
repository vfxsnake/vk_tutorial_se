# Learning Plan — Chapter 04: Vertex Buffers

> **Estimated total time:** 9 sessions × ~2h = ~18h
> **Calibration basis:** Sessions 12–24 averaged 2h; new-class implementation sessions ran 2h30m–3h37m.
> **Prerequisites:** Chapter 03 complete. Triangle visible. `GraphicsPipeline`, `Renderer`, `Application` all reviewed and verified.

---

## Chapter Milestones Overview

| # | Milestone | Session Type | Est. Sessions | Est. Time |
|---|-----------|--------------|---------------|-----------|
| M1 | Vertex Input Description & Shader Update | Theory + Implementation | 2 | ~3.5h |
| M2 | Vulkan Memory Model | Theory + Implementation | 2 | ~4h |
| M3 | Staging Buffer Pattern & Mesh Class | Theory + Implementation | 3 | ~6h |
| M4 | Index Buffer & Rectangle | Implementation | 2 | ~4h |

---

## Milestone M1 — Vertex Input Description & Shader Update

> **Session type:** Theory (Session A) + Implementation (Session B)
> **Estimated time:** Session A ~1.5h · Session B ~2h
> **Goal:** Replace the shader's hardcoded geometry with a `Vertex` struct, configure the pipeline to read from vertex buffers, and update the Slang shader to accept per-vertex input.
> **Source:** §04.00 Vertex Input Description

---

### Session A — Theory

#### Session Checklist
- [ ] Read §04.00 in full before answering any questions
- [ ] Answer all Comprehension Questions below (write your answers, don't just read)
- [ ] Review the Key API table for M1
- [ ] Write a one-paragraph summary in your own words: what changed in the pipeline contract between Chapter 03 and Chapter 04?

#### Comprehension Questions

**Awareness — Why does this exist?**

1. In Chapter 03 the vertex shader computed positions from a `static` array indexed by `SV_VertexID`. What is the concrete limitation of that approach that vertex buffers solve?
   *→ Hint: §04.00, opening paragraphs.*

2. If you never changed the pipeline's `PipelineVertexInputStateCreateInfo` but did bind a vertex buffer before drawing, what would happen? Why?

**Conceptual — What is it and how does it work?**

3. A `vk::VertexInputBindingDescription` has three fields: `binding`, `stride`, and `inputRate`. In plain language, what does each one tell the GPU?
   *→ Hint: §04.00 — Binding Descriptions.*

4. A `vk::VertexInputAttributeDescription` has four fields: `location`, `binding`, `format`, and `offset`. Explain what `location` connects to, and what `offset` is measuring.
   *→ Hint: §04.00 — Attribute Descriptions. The `offsetof` macro note is important.*

5. The format `eR32G32B32Sfloat` is borrowed from the image format system. Why do you think Vulkan reuses image formats to describe vertex attribute types instead of defining a separate enum?

6. §04.00 lists a format-type mapping table. What happens if you declare a `float4` in the shader but only supply `eR32G32B32Sfloat` (three components) in the attribute description? What happens in the opposite case — `float2` shader input but `eR32G32B32Sfloat` format?
   *→ Hint: §04.00 — Important Notes.*

**Dependency / flow — How does this connect to earlier work?**

7. `PipelineVertexInputStateCreateInfo` was created in `GraphicsPipeline::createPipeline()` in Chapter 03 with empty arrays (no vertex input). Trace exactly which struct and which two fields you need to update now to wire in the `Vertex` descriptions.
   *→ Refers back to: Session 18 — `createPipeline()` implementation.*

8. The `PipelineVertexInputStateCreateInfo` stores raw pointers (`pVertexBindingDescriptions`, `pVertexAttributeDescriptions`). What lifetime constraint does this impose on the objects you point to, and when exactly must they remain alive?

---

### Session B — Implementation

> **Depends on:** Session A complete and all questions answered.

#### Session Checklist
- [ ] Can explain in plain language what binding descriptions and attribute descriptions do before writing any code
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests below pass

#### Implementation Checklist

- [ ] `Vertex` struct — `glm::vec2 pos`, `glm::vec3 color`; lives in `src/renderer/buffers/Vertex.h`
- [ ] `Vertex::getBindingDescription()` — returns `vk::VertexInputBindingDescription` with correct binding, stride, inputRate
- [ ] `Vertex::getAttributeDescriptions()` — returns `std::array<vk::VertexInputAttributeDescription, 2>`; correct locations, formats, `offsetof`-based offsets
- [ ] `GraphicsPipeline::createPipeline()` — `PipelineVertexInputStateCreateInfo` updated to reference `Vertex` descriptions; includes `renderer/buffers/Vertex.h`
- [ ] `shaders/triangle.slang` — vertex entry point receives `VSInput` struct with `[vk::location(0)]` and `[vk::location(1)]`; hardcoded arrays and `SV_VertexID` removed
- [ ] `CMakeLists.txt` — no new `.cpp` files yet, but confirm shader still recompiles
- [ ] Application still compiles and runs (triangle renders — still using hardcoded `draw(3,1,0,0)` for now)

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Pipeline accepts vertex input state | Build cleanly | No compile errors | [ ] |
| T2 | Shader compiles with VSInput struct | Run CMake build — slangc invoked | `triangle.spv` regenerated, no slangc errors | [ ] |
| T3 | Triangle still renders | Run the application | Triangle visible (hardcoded `draw(3,1,0,0)` still in use — will change in M2) | [ ] |
| T4 | Validation layers are silent | Run with `VK_LAYER_KHRONOS_validation` | No errors or warnings | [ ] |

---

## Milestone M2 — Vulkan Memory Model

> **Session type:** Theory (Session A) + Implementation (Session B)
> **Estimated time:** Session A ~2h · Session B ~2h
> **Goal:** Understand Vulkan's buffer/memory separation and memory type system. Implement a host-visible vertex buffer as a stepping stone before the staging pattern.
> **Source:** §04.01 Vertex Buffer Creation

---

### Session A — Theory

#### Session Checklist
- [ ] Read §04.01 in full before answering any questions
- [ ] Answer all Comprehension Questions below
- [ ] Draw (or describe in words) the sequence of Vulkan calls needed to get vertex data into a usable buffer — from `BufferCreateInfo` through to `memcpy`
- [ ] Write a one-paragraph summary: why are `vk::raii::Buffer` and `vk::raii::DeviceMemory` two separate objects in Vulkan?

#### Comprehension Questions

**Awareness — Why does this exist?**

1. In most graphics APIs (OpenGL, D3D11) you call one function and get a buffer with memory already attached. Why does Vulkan separate buffer creation from memory allocation? What does this separation enable that the single-call model does not?
   *→ Hint: §04.01, and think about the suballocation use-case mentioned in §04.02.*

2. `vk::MemoryRequirements` has three fields: `size`, `alignment`, and `memoryTypeBits`. The tutorial says `size` may differ from what you requested. When and why would the driver give you more memory than you asked for?

**Conceptual — What is it and how does it work?**

3. `memoryTypeBits` is described as a bitmask where bit `i` being set means memory type `i` is compatible. Walk through the `findMemoryType()` logic: given a `typeFilter` and required `properties`, what two conditions must both be true for a memory type to be selected?
   *→ Hint: §04.01 — Finding the Right Memory Type.*

4. Two memory property flags are needed for the host-visible vertex buffer: `eHostVisible` and `eHostCoherent`. What does each one do, and what would break if you used `eHostVisible` alone without `eHostCoherent`?
   *→ Hint: §04.01 — the paragraph about "driver may not immediately copy data".*

5. After calling `unmapMemory()`, the CPU pointer returned by `mapMemory()` is invalid. But the spec says the data is guaranteed to reach the GPU by the next `queue.submit()`. What mechanism ensures this — is it the `eHostCoherent` flag, the `submit()` call, or something else?

6. `bindMemory(*vertexBufferMemory, 0)` takes an offset of `0`. When would you pass a non-zero offset, and what problem does that solve?

**Dependency / flow — How does information travel here?**

7. `findMemoryType()` needs `vk::PhysicalDeviceMemoryProperties`. In our architecture, the physical device lives in `VulkanContext`. Trace exactly how `Renderer::findMemoryType()` accesses it — what call chain does it use?
   *→ Refers back to: `VulkanContext` accessors, Session 13.*

8. After M1, `GraphicsPipeline::record()` still calls `draw(3, 1, 0, 0)` with a hardcoded count. After this milestone you will have a real vertex buffer with `vertices.size()` entries. What two things must change in `record()` to use the buffer: one in recording setup, one in the draw call?

---

### Session B — Implementation

> **Depends on:** Session A complete and all questions answered.
> **Note:** This session implements a host-visible vertex buffer only — no staging yet. The goal is a working vertex buffer draw before adding complexity in M3.

#### Session Checklist
- [ ] Can explain `findMemoryType()`'s two conditions before writing any code
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests below pass

#### Implementation Checklist

- [ ] `vertices` constant defined in `Application.cpp` — three triangle corners as `Vertex{pos, color}` (rectangle comes in M4)
- [ ] `Renderer::findMemoryType(typeFilter, properties)` — loops `memoryTypeCount`, checks bitmask + property flags, throws on failure
- [ ] `Renderer::createBuffer(size, usage, properties)` — returns `std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>`; creates buffer, queries requirements, finds memory type, allocates, binds
- [ ] `Renderer` temporary member `vertexBuffer_` + `vertexMemory_` — host-visible, `eVertexBuffer` usage
- [ ] `Application::initVulkan()` calls a temporary `renderer_->createHostVertexBuffer(vertices)` or equivalent to set up the buffer — this will be replaced by `createMesh()` in M3
- [ ] `GraphicsPipeline::record()` — `bindVertexBuffers(0, *vertexBuffer, {0})` added before draw; `draw()` uses `vertices.size()` count; receives vertex buffer handle as temporary parameter
- [ ] Clean build + triangle renders using vertex buffer data

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | `findMemoryType` finds suitable type | Run app | No `runtime_error` thrown on startup | [ ] |
| T2 | Vertex buffer data used | Change a vertex colour in `vertices[]`, rebuild, run | Triangle colour changes | [ ] |
| T3 | Correct vertex count | Set `vertices` to 3 entries, check draw count | Exactly the triangle defined in `vertices[]` renders | [ ] |
| T4 | Validation layers silent | Run with `VK_LAYER_KHRONOS_validation` | No errors or warnings | [ ] |

---

## Milestone M3 — Staging Buffer Pattern & Mesh Class

> **Session type:** Theory (Session A) + Implementation (Sessions B and C)
> **Estimated time:** Session A ~1.5h · Session B ~2.5h · Session C ~2h
> **Goal:** Implement the full staging upload pattern, create the `Mesh` class as a pure GPU resource container, implement `Renderer::createMesh()` as the factory, and wire `GraphicsPipeline::record()` to accept a `const Mesh&`.
> **Source:** §04.02 Staging Buffer

---

### Session A — Theory

#### Session Checklist
- [ ] Read §04.02 in full before answering any questions
- [ ] Answer all Comprehension Questions below
- [ ] Draw (or describe in words) the full data flow from `std::vector<Vertex>` in CPU RAM to device-local VRAM — every object and every transfer involved
- [ ] Write one paragraph explaining why `queue.waitIdle()` is acceptable here but would not be acceptable in a frame loop

#### Comprehension Questions

**Awareness — Why does this exist?**

1. §04.02 says host-visible memory "isn't the most optimal memory type". On a discrete GPU, why is device-local memory faster? What physically is the difference between the two on PC hardware?

2. `eDeviceLocal` memory cannot be mapped by the CPU. If you need to update geometry every frame (e.g., a particle system), would the staging pattern be appropriate? What alternative would you use?

**Conceptual — What is it and how does it work?**

3. The staging buffer uses `eTransferSrc` and the vertex buffer uses `eTransferDst`. What does §04.02 say about which queue families support transfer operations — do you need a dedicated transfer queue?
   *→ Hint: §04.02 — Transfer Operations paragraph.*

4. `copyBuffer()` allocates a command buffer, records one copy command, submits it, and calls `waitIdle()`. The command buffer begin flag is `eOneTimeSubmit`. What does this flag tell the driver, and why is it appropriate here vs a command buffer that is reused every frame?

5. §04.02 explicitly warns: "It is not possible to specify `VK_WHOLE_SIZE` here, unlike the `vkMapMemory` command." Why does `vkCmdCopyBuffer` require an exact size while `vkMapMemory` accepts `VK_WHOLE_SIZE`?

6. §04.02 ends with a warning about `vkAllocateMemory` limits (as low as 4096 simultaneous allocations). What is the recommended production solution, and what is its name?
   *→ This will inform a future chapter — note it but don't implement it now.*

**Dependency / flow — How does information travel here?**

7. In our architecture, `Mesh` is a pure GPU resource container — it does not perform its own staging upload. Trace exactly how a `std::vector<Vertex>` defined in `Application.cpp` ends up as device-local VRAM: which object is called, what it does step by step, and what the `Mesh` constructor receives at the end.
   *→ Refers back to: Architecture discussion, Session 25 — Renderer::createMesh() factory.*

8. `copyBuffer()` needs a command pool and a queue. Both live in `Renderer`. If we later move `createMesh()` to `ResourceManager`, what will `ResourceManager` need to receive to keep `copyBuffer()` working? What does this tell you about `ResourceManager`'s future dependencies?

---

### Session B — Implementation (Mesh Class + Buffer Helpers)

> **Depends on:** Session A complete and all questions answered. M2 Session B complete.

#### Session Checklist
- [ ] Can explain the staging upload sequence before writing any code
- [ ] All items in the Implementation Checklist below are complete

#### Implementation Checklist

- [ ] `src/renderer/buffers/Mesh.h` — class with RAII members (`vertexBuffer_`, `vertexMemory_`, `indexBuffer_`, `indexMemory_`, `indexCount_`, `indexType_`); constructor takes pre-created handles; copy deleted; move defaulted
- [ ] `Mesh::bind(const vk::raii::CommandBuffer&)` — calls `bindVertexBuffers(0, *vertexBuffer_, {0})` and `bindIndexBuffer(*indexBuffer_, 0, indexType_)`
- [ ] `Mesh::getIndexCount()` — returns `indexCount_`
- [ ] `src/renderer/buffers/Mesh.cpp` added to `CMakeLists.txt`
- [ ] `Renderer::copyBuffer(src, dst, size)` — allocates transient command buffer, records `copyBuffer`, submits with `eOneTimeSubmit`, calls `queue.waitIdle()`
- [ ] `Renderer::createMesh(std::span<const Vertex>, std::span<const uint16_t>)` — full staging for vertices: staging buffer → device-local; staging for indices: staging buffer → device-local; constructs and stores `mesh_` as `std::optional<Mesh>`

---

### Session C — Implementation (Wire Pipeline + Application)

> **Depends on:** Session B complete and building cleanly.

#### Session Checklist
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests below pass

#### Implementation Checklist

- [ ] Remove temporary host-visible vertex buffer from M2 — `Renderer` no longer holds raw buffer members
- [ ] `GraphicsPipeline::record()` signature updated to accept `const Mesh&` as final parameter
- [ ] `GraphicsPipeline::record()` body — replaces `draw()` with `mesh.bind(command_buffer)` then `command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0)`
- [ ] `Renderer::drawFrame()` passes `*mesh_` to `pipeline_->record()`
- [ ] `Application::initVulkan()` calls `renderer_->createMesh(vertices, indices)` after renderer construction
- [ ] `vertices` still has three entries (triangle) for now — `indices` = `{0, 1, 2}` as `uint16_t`; rectangle comes in M4
- [ ] Clean build, triangle renders via device-local memory

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Device-local buffer used | Build and run | Triangle visible — no change in appearance, but memory is now device-local | [ ] |
| T2 | Staging buffer destroyed | Enable validation layers, check no dangling resource warnings | Validation layer silent | [ ] |
| T3 | `copyBuffer` completes | Add stdout print after `createMesh()` | Print appears — no hang or crash during upload | [ ] |
| T4 | Validation layers silent | Run with `VK_LAYER_KHRONOS_validation` | No errors or warnings | [ ] |

---

## Milestone M4 — Index Buffer & Rectangle

> **Session type:** Implementation (Sessions A and B)
> **Estimated time:** Session A ~1.5h · Session B ~2h
> **Goal:** Extend `createMesh()` to stage an index buffer, update `Application` to define a rectangle with 4 vertices and 6 indices, and confirm a coloured rectangle renders.
> **Source:** §04.03 Index Buffer

---

### Session A — Theory

#### Session Checklist
- [ ] Read §04.03 in full before answering any questions
- [ ] Answer all Comprehension Questions below
- [ ] Sketch (or describe) the index layout for a rectangle: label the four corner vertices 0–3 and write out the six indices that produce two clockwise triangles
- [ ] Write one paragraph: what is the performance argument for index buffers in real mesh data?

#### Comprehension Questions

**Awareness — Why does this exist?**

1. §04.03 says "vertices are reused in an average number of three triangles." Calculate: for a 1000-triangle mesh, how many vertex entries does a non-indexed vertex buffer require vs an indexed one (assuming average reuse)? What is the memory saving?

2. Beyond memory, why does vertex reuse matter for GPU performance? Think about what the GPU does with vertex data between the vertex shader and the fragment shader.
   *→ Hint: post-transform vertex cache.*

**Conceptual — What is it and how does it work?**

3. `bindIndexBuffer()` takes three parameters: buffer, offset, and index type. `drawIndexed()` has five parameters. Which parameter in `drawIndexed()` corresponds to the `vertexOffset` concept — what does it do, and when would you use a non-zero value?
   *→ Hint: §04.03 — Drawing with Index Buffers + driver recommendation note.*

4. §04.03 offers a choice between `uint16_t` and `uint32_t` indices. State the trade-off precisely: what does each cost in memory, and what is the maximum vertex count for each?

5. §04.03 ends with a driver recommendation to store the vertex buffer and index buffer in a single `vk::Buffer` using offsets. Which parameters in `bindVertexBuffers()`, `bindIndexBuffer()`, and `drawIndexed()` exist specifically to support this pattern?

**Dependency / flow — How does this connect to M3?**

6. In our `Mesh` class, `bind()` calls both `bindVertexBuffers` and `bindIndexBuffer`. The index type (`uint16_t` vs `uint32_t`) is stored as `vk::IndexType indexType_`. Trace where `indexType_` gets its value — from `Application` through `createMesh()` into the `Mesh` constructor.

---

### Session B — Implementation

> **Depends on:** Session A complete and all questions answered. M3 Session C complete.

#### Session Checklist
- [ ] Can explain the six-index rectangle layout and which two triangles they form before writing any code
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests below pass

#### Implementation Checklist

- [ ] `vertices` in `Application.cpp` updated to four rectangle corners — each with a distinct colour; 2D positions forming a centred rectangle
- [ ] `indices` in `Application.cpp` — `std::vector<uint16_t>{0, 1, 2, 2, 3, 0}` — two triangles, clockwise winding
- [ ] `Renderer::createMesh()` already handles index buffer staging from M3 Session B — confirm it works with 6 indices
- [ ] `Application::initVulkan()` passes both `vertices` and `indices` to `createMesh()`
- [ ] Rectangle renders with four distinct corner colours blending across the surface

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Rectangle renders | Run application | A centred rectangle visible — not a triangle | [ ] |
| T2 | Correct index count | Check `mesh_.getIndexCount()` == 6 | `drawIndexed` called with 6 indices | [ ] |
| T3 | Vertex reuse working | Change colour of vertex 2 only in `vertices[]` | Only the shared corner changes colour — proves both triangles reference the same vertex | [ ] |
| T4 | Resize still works | Resize the window | Rectangle resizes correctly, no crash | [ ] |
| T5 | Validation layers silent | Run with `VK_LAYER_KHRONOS_validation` | No errors or warnings | [ ] |

---

## Chapter Progress Tracker

| Milestone | Theory ✓ | Impl ✓ | Tests Pass ✓ |
|-----------|----------|--------|--------------|
| M1 — Vertex Input Description & Shader Update | [ ] | [ ] | [ ] |
| M2 — Vulkan Memory Model | [ ] | [ ] | [ ] |
| M3 — Staging Buffer Pattern & Mesh Class | [ ] | [ ] | [ ] |
| M4 — Index Buffer & Rectangle | [ ] | [ ] | [ ] |

**Chapter complete when all rows are fully ticked.**

---

## Session Log

Fill in after each session to track progress and blockers.

| Session # | Date | Milestone(s) | What was covered | Blockers / open questions |
|-----------|------|--------------|------------------|--------------------------|
| 26 | | M1-A | | |
| 27 | | M1-B | | |
| 28 | | M2-A | | |
| 29 | | M2-B | | |
| 30 | | M3-A | | |
| 31 | | M3-B | | |
| 32 | | M3-C | | |
| 33 | | M4-A | | |
| 34 | | M4-B | | |
