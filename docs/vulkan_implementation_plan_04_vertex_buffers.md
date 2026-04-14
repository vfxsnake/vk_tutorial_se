# Implementation Plan — Chapter 04: Vertex Buffers

> **Agreed during architecture discussion — Session 25, 2026-04-14**
> **Prerequisite:** Chapter 03 complete, triangle visible, all verification tests passing.

---

## Architectural Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| `Vertex` location | `src/renderer/buffers/Vertex.h` | GPU format descriptor — renderer concern, not scene |
| `Mesh` location | `src/renderer/buffers/Mesh.h/.cpp` | GPU resource container — renderer layer |
| Multi-word folder naming | `snake_case` (consistent with variable naming) | Extends existing lowercase folder convention |
| Mesh ownership | `Renderer` owns `mesh_` member | Renderer is GPU resource manager for tutorial scope |
| Mesh responsibility | Pure GPU resource container — holds RAII handles + `bind()` | Mesh should not know how to upload itself |
| Upload factory | `Renderer::createMesh(vertices, indices)` | Staging logic lives here; migrates to `ResourceManager` in "Building a Simple Engine" |
| Caller of `createMesh()` | `Application::initVulkan()` | Application is stand-in for scene layer until Ch08 |
| Staging helpers | Private methods on `Renderer` | Collocated with `createMesh()`; reused by Ch05 UBOs; migrate together to `ResourceManager` |
| `record()` signature | `GraphicsPipeline::record()` receives `const Mesh&` | Preserves "pipeline records itself" principle; pipeline handles bind + draw |
| Future | `ResourceManager` replaces `Renderer::createMesh()` | Deferred to "Building a Simple Engine" — `Mesh` itself unchanged |

---

## Folder Structure After Chapter 04

```
src/
├── main.cpp
├── Application.h/.cpp
├── core/
│   ├── VulkanContext.h/.cpp
│   └── SwapChain.h/.cpp
├── renderer/
│   ├── Renderer.h/.cpp
│   ├── GraphicsPipeline.h/.cpp
│   ├── RenderFrameSlot.h
│   └── buffers/
│       ├── Vertex.h
│       └── Mesh.h/.cpp
├── scene/                     (empty — ECS placeholder)
└── utils/
    └── FileUtils.h
```

---

## New Files

### `src/renderer/buffers/Vertex.h`

Header-only. No `.cpp`.

```
struct Vertex
    glm::vec2 pos
    glm::vec3 color

    static getBindingDescription()  → vk::VertexInputBindingDescription
    static getAttributeDescriptions() → std::array<vk::VertexInputAttributeDescription, 2>
```

- `getBindingDescription()`: binding=0, stride=sizeof(Vertex), inputRate=eVertex
- `getAttributeDescriptions()`: location 0 = pos (eR32G32Sfloat), location 1 = color (eR32G32B32Sfloat), offsets via `offsetof`

---

### `src/renderer/buffers/Mesh.h` / `Mesh.cpp`

```
class Mesh
    constructor(vk::raii::Buffer  vertex_buffer,
                vk::raii::DeviceMemory vertex_memory,
                vk::raii::Buffer  index_buffer,
                vk::raii::DeviceMemory index_memory,
                uint32_t index_count,
                vk::IndexType index_type)

    delete copy constructor + copy assignment
    default move constructor + move assignment

    bind(const vk::raii::CommandBuffer&) → void
        bindVertexBuffers(0, *vertexBuffer_, {0})
        bindIndexBuffer(*indexBuffer_, 0, indexType_)

    getIndexCount() → uint32_t

private:
    vk::raii::Buffer        vertexBuffer_
    vk::raii::DeviceMemory  vertexMemory_
    vk::raii::Buffer        indexBuffer_
    vk::raii::DeviceMemory  indexMemory_
    uint32_t                indexCount_
    vk::IndexType           indexType_
```

---

## Modified Files

### `src/renderer/Renderer.h` / `Renderer.cpp`

**New private methods:**

```
findMemoryType(uint32_t type_filter,
               vk::MemoryPropertyFlags properties) → uint32_t

createBuffer(vk::DeviceSize size,
             vk::BufferUsageFlags usage,
             vk::MemoryPropertyFlags properties)
    → std::pair<vk::raii::Buffer, vk::raii::DeviceMemory>

copyBuffer(const vk::raii::Buffer& src,
           const vk::raii::Buffer& dst,
           vk::DeviceSize size) → void

createMesh(std::span<const Vertex> vertices,
           std::span<const uint16_t> indices) → Mesh
```

**New private member:**

```
std::optional<Mesh> mesh_
```

(`std::optional` avoids default-constructing an invalid Mesh before `createMesh()` is called.)

**`createMesh()` logic:**
1. Create staging buffer (`eTransferSrc | eHostVisible | eHostCoherent`) for vertices
2. Map, `memcpy` vertex data, unmap
3. Create device-local vertex buffer (`eVertexBuffer | eTransferDst | eDeviceLocal`)
4. `copyBuffer()` staging → vertex buffer, destroy staging
5. Repeat steps 1–4 for index data (`eIndexBuffer | eTransferDst`)
6. Construct and return `Mesh` with the four RAII handles

**`copyBuffer()` logic:**
1. Allocate one command buffer from `commandPool_` with `eOneTimeSubmit`
2. Record `copyBuffer(src, dst, region)`
3. Submit to graphics queue
4. `queue.waitIdle()`

---

### `src/renderer/GraphicsPipeline.h` / `GraphicsPipeline.cpp`

**`createPipeline()` changes:**
- `PipelineVertexInputStateCreateInfo` updated to use `Vertex::getBindingDescription()` and `Vertex::getAttributeDescriptions()`
- Includes `renderer/buffers/Vertex.h`

**`record()` signature change:**
```
// Before:
record(const vk::raii::CommandBuffer&,
       vk::Extent2D,
       const vk::raii::ImageView&,
       vk::Image) → void

// After:
record(const vk::raii::CommandBuffer&,
       vk::Extent2D,
       const vk::raii::ImageView&,
       vk::Image,
       const Mesh&) → void
```

**`record()` body changes:**
- Replace `commandBuffer.draw(3, 1, 0, 0)` with:
  1. `mesh.bind(command_buffer)`
  2. `command_buffer.drawIndexed(mesh.getIndexCount(), 1, 0, 0, 0)`

---

### `src/Application.h` / `Application.cpp`

**`Application.cpp` additions:**
- Define `vertices` constant — four rectangle corners, each `{pos, color}`
- Define `indices` constant — `{0, 1, 2, 2, 3, 0}` (`uint16_t`)
- In `initVulkan()`: call `renderer_->createMesh(vertices, indices)` after `Renderer` construction

---

### `shaders/triangle.slang`

**Vertex shader changes:**
- Remove `static` position and colour arrays
- Remove `SV_VertexID`-based lookup
- Add `VSInput` struct:
  ```hlsl
  struct VSInput {
      [vk::location(0)] float2 inPosition : POSITION;
      [vk::location(1)] float3 inColor    : COLOR;
  };
  ```
- Entry point receives `VSInput` parameter directly

**Fragment shader:** no changes.

---

### `CMakeLists.txt`

- Add `src/renderer/buffers/Mesh.cpp` to the source list

---

## Build Order

Work bottom-up — each step compiles cleanly before moving to the next.

| Step | File(s) | What to verify |
|------|---------|----------------|
| 1 | `Vertex.h` | Included in `GraphicsPipeline.cpp` — clean compile |
| 2 | `Mesh.h/.cpp` | Included in `Renderer.cpp` — clean compile |
| 3 | `Renderer` — helpers | `findMemoryType`, `createBuffer`, `copyBuffer` — clean compile |
| 4 | `Renderer` — `createMesh()` | Clean compile |
| 5 | `GraphicsPipeline` — vertex input + `record()` | Clean compile |
| 6 | `Application` — vertices, indices, `createMesh()` call | Clean compile |
| 7 | `triangle.slang` | Shader recompiles to `.spv` cleanly |
| 8 | Full build + run | Coloured rectangle visible |
