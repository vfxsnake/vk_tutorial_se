# Implementation Plan — Chapter 16: Rendering Multiple Objects

> **Ground truth for all implementation and code-review sessions.**
> Do not deviate from the decisions recorded here without an explicit architecture discussion.

---

## Architecture Decisions (agreed session 71)

| Decision | Choice |
|----------|--------|
| Scene / GPU split | `scene::Object` (CPU) + `ObjectRenderData` (GPU) — separate types |
| Namespace | `namespace scene` wraps all `scene/` types; rest of project unchanged |
| Mesh ownership | `Renderer` owns `std::vector<Mesh> meshes_` — simple pool, no class yet |
| Mesh reference | `scene::Object::mesh_index` — `uint32_t` index into `Renderer::meshes_` |
| Object render data ownership | `Renderer` owns `std::vector<ObjectRenderData> objectRenderData_` |
| Descriptor pool sizing | `MAX_OBJECTS × MAX_FRAMES_IN_FLIGHT`; `MAX_OBJECTS = 16` constant in `Renderer` |
| UBO computation | `Application` computes per-object UBOs; passes `std::span<const UniformBufferObject>` to `drawFrame()` |
| Multi-mesh recording | Ch16: single shared mesh passed to `record()`; multi-mesh grouping is a future optimisation |
| `MeshPool` class | Deferred to "Building a Simple Engine" |
| Full namespace refactor | Deferred to "Building a Simple Engine" |

---

## Folder / File Map

```
src/
├── scene/
│   ├── Transform.h          NEW — namespace scene; struct Transform
│   └── Object.h             NEW — namespace scene; struct Object
├── renderer/
│   ├── ObjectRenderData.h   NEW — per-object × per-frame GPU resources (header-only)
│   ├── RenderFrameSlot.h    MODIFY — strip UBO/descriptor members; keep sync + command buffer only
│   ├── Renderer.h/.cpp      MODIFY — mesh vector, object render data, updated pool + drawFrame
│   └── GraphicsPipeline.h/.cpp  MODIFY — record() loops over per-object descriptor sets
└── Application.h/.cpp       MODIFY — scene objects, setupObjects(), updated init + main loop
```

---

## Step 1 — `src/scene/Transform.h` (NEW)

```
namespace scene {
    struct Transform {
        glm::vec3 position = {0.0f, 0.0f, 0.0f};
        glm::vec3 rotation = {0.0f, 0.0f, 0.0f};  // Euler angles in radians
        glm::vec3 scale    = {1.0f, 1.0f, 1.0f};

        auto getModelMatrix() const -> glm::mat4;
    };
}
```

`getModelMatrix()` composes: `translate × rotateX × rotateY × rotateZ × scale` (same order as the tutorial's `GameObject::getModelMatrix()`). Implemented inline in the header — it's a pure math function with no side effects.

**Dependencies:** `<glm/glm.hpp>`, `<glm/gtc/matrix_transform.hpp>`

---

## Step 2 — `src/scene/Object.h` (NEW)

```
#include "Transform.h"

namespace scene {
    struct Object {
        Transform transform;
        uint32_t  mesh_index    = 0;
        uint32_t  texture_index = 0;  // reserved — no TexturePool yet
    };
}
```

`texture_index` is part of the data model for future material support; it is not consumed by the renderer in Ch16.

**Dependencies:** `scene/Transform.h`

---

## Step 3 — `src/renderer/ObjectRenderData.h` (NEW)

Move-only struct. Holds per-object GPU resources, indexed by frame slot.

```
struct ObjectRenderData {
    std::vector<vk::raii::Buffer>        uniformBuffers_;
    std::vector<vk::raii::DeviceMemory>  uniformBufferMemories_;
    std::vector<void*>                   uniformBuffersMapped_;
    std::vector<vk::raii::DescriptorSet> descriptorSets_;

    // Move-only — Rule of Five
    ObjectRenderData() = default;
    ObjectRenderData(ObjectRenderData&&) = default;
    ObjectRenderData& operator=(ObjectRenderData&&) = default;
    ObjectRenderData(const ObjectRenderData&) = delete;
    ObjectRenderData& operator=(const ObjectRenderData&) = delete;

    void updateUniformBuffer(uint32_t frame_index, const UniformBufferObject& ubo);
};
```

`updateUniformBuffer()` is a one-line `memcpy` from `ubo` into `uniformBuffersMapped_[frame_index]`. Implemented inline.

Uses `std::vector` (not `std::array`) for consistency with the rest of the project and to avoid aggregate-initialisation complexity with move-only `vk::raii::*` types.

**Dependencies:** `<vulkan/vulkan_raii.hpp>`, `renderer/buffers/UniformBufferObject.h`

---

## Step 4 — `src/renderer/RenderFrameSlot.h` (MODIFY)

**Remove** these members entirely:
- `vk::raii::Buffer uniformBuffer_`
- `vk::raii::DeviceMemory uniformBufferMemory_`
- `void* uniformBufferMappedMemory_`
- `vk::raii::DescriptorSet descriptorSet_`
- `void updateUniformBuffer(const UniformBufferObject&)`

**Keep** (unchanged):
- `vk::raii::CommandBuffer commandBuffer_`
- `vk::raii::Semaphore imageAvailableSemaphore_`
- `vk::raii::Fence inFlightFence_`

Also remove the `#include "buffers/UniformBufferObject.h"` and `#include <cstring>` includes if they are no longer needed after the removal.

---

## Step 5 — `src/renderer/Renderer.h/.cpp` (MODIFY)

### New constant
```cpp
static constexpr uint32_t MAX_OBJECTS = 16;
```

### New members (declare after `frameDescriptorLayout_`, before `renderFrameSlots_`)
```cpp
std::vector<Mesh>            meshes_;
std::vector<ObjectRenderData> objectRenderData_;
std::vector<uint32_t>        objectMeshIndices_;  // parallel to objectRenderData_
```

### New public methods

```cpp
// Stores mesh in meshes_, returns its index. Application calls once per unique mesh.
auto addMesh(Mesh&& mesh) -> uint32_t;

// Creates ObjectRenderData for one object: allocates MAX_FRAMES_IN_FLIGHT UBO buffers,
// maps them, allocates MAX_FRAMES_IN_FLIGHT descriptor sets, writes UBO buffer info.
// Appends to objectRenderData_ and objectMeshIndices_. Call once per scene::Object.
void createObjectRenderData(uint32_t mesh_index);
```

### Updated public method — `drawFrame()`

**Old signature:**
```cpp
bool drawFrame(SwapChain&, GraphicsPipeline&, const Mesh&,
               const UniformBufferObject&, vk::ImageView, vk::ImageView, float);
```

**New signature:**
```cpp
bool drawFrame(
    SwapChain& swap_chain,
    GraphicsPipeline& graphics_pipeline,
    std::span<const UniformBufferObject> ubos,   // one per scene object, same order as objectRenderData_
    vk::ImageView depth_image_view,
    vk::ImageView msaa_color_image_view,
    float delta_time
);
```

Remove `const Mesh& mesh` — Renderer looks up `meshes_[objectMeshIndices_[i]]` internally.

### `createDescriptorPool()` update

Change `maxSets` and `eUniformBuffer` descriptorCount from `MAX_FRAMES_IN_FLIGHT` to `MAX_OBJECTS * MAX_FRAMES_IN_FLIGHT`.

### `initializeFrameData()` update

Remove all UBO buffer creation, memory mapping, and descriptor set allocation from this function — those now live in `createObjectRenderData()`. The function only initialises the sync primitives and command buffer in each `RenderFrameSlot`.

### `drawFrame()` body changes

After fence wait and image acquire, before command buffer reset:

```
for each i in [0, objectRenderData_.size()):
    objectRenderData_[i].updateUniformBuffer(currentFrame_, ubos[i])
```

When calling `graphics_pipeline.record()`:
- Pass `meshes_[objectMeshIndices_[0]]` as the shared mesh (Ch16: all objects share mesh 0)
- Build a `std::vector<vk::DescriptorSet>` of per-object descriptor sets for `currentFrame_`:
  `objectRenderData_[i].descriptorSets_[currentFrame_]` for each i
- Pass that vector (as a span) to `record()`

**Note:** Remove the `const Mesh& mesh` parameter from the `drawFrame()` call site in `Application.cpp`.

---

## Step 6 — `src/renderer/GraphicsPipeline.h/.cpp` (MODIFY)

### Updated `record()` signature

**Old:**
```cpp
void record(
    vk::CommandBuffer command_buffer,
    const vk::raii::DescriptorSet& descriptor_set,
    const vk::raii::DescriptorSet& texture_descriptor_set,
    vk::Extent2D extent,
    vk::Image image,
    vk::ImageView image_view,
    vk::ImageView msaa_color_image_view,
    const Mesh& mesh,
    vk::ImageView depth_image_view
);
```

**New:**
```cpp
void record(
    vk::CommandBuffer command_buffer,
    std::span<const vk::DescriptorSet> per_object_descriptor_sets,
    const vk::raii::DescriptorSet& texture_descriptor_set,
    vk::Extent2D extent,
    vk::Image image,
    vk::ImageView image_view,
    vk::ImageView msaa_color_image_view,
    const Mesh& mesh,
    vk::ImageView depth_image_view
) const;
```

### `record()` body changes

The dynamic rendering begin/end and all pipeline + viewport setup stay unchanged.

The draw section changes from one bind+draw to:

```
mesh.bind(command_buffer)            // vertex + index buffers — once, outside the loop

bind texture_descriptor_set (set 1)  // shared texture — once, outside the loop

for each descriptor_set in per_object_descriptor_sets:
    bind descriptor_set (set 0)      // per-object UBO
    drawIndexed(mesh.indexCount(), 1, 0, 0, 0)
```

Add `#include <span>` to the header.

---

## Step 7 — `src/Application.h/.cpp` (MODIFY)

### Header changes

**Remove:**
- `std::unique_ptr<Mesh> mesh_` member
- Forward declaration of `Mesh`

**Add:**
```cpp
#include "scene/Object.h"           // pulls in scene::Object + scene::Transform

std::vector<scene::Object> objects_;

void setupObjects();
```

`mesh_` is gone — mesh lives in `Renderer::meshes_`. Application stores nothing about the mesh directly.

### `initVulkan()` changes

Where `mesh_` was created, replace with:

```
// 1. Load model data (vertices + indices) — same as before
// 2. Create mesh in renderer and store its index:
uint32_t mesh_idx = renderer_->addMesh(renderer_->createMesh(vertices, indices));
// 3. Setup scene objects:
setupObjects(mesh_idx);
// 4. Create GPU render data for each object:
for (const auto& obj : objects_) {
    renderer_->createObjectRenderData(obj.mesh_index);
}
```

### `setupObjects(uint32_t mesh_idx)` (new private method)

Creates 3 `scene::Object` instances in `objects_`, each with a distinct transform.
The base orientation fix (90° X rotation to stand the viking room upright) is encoded in
`transform.rotation.x` here — not applied as a correction matrix in the UBO helper.

Example layout (exact values from the tutorial):
- Object 0: position (0, 0, 0), rotation (-90° X for upright), scale (1, 1, 1)
- Object 1: position (-2, 0, -1), rotation (-90° X + 45° Y), scale (0.75, 0.75, 0.75)
- Object 2: position (2, 0, -1), rotation (-90° X − 45° Y), scale (0.75, 0.75, 0.75)

All three set `mesh_index = mesh_idx`.

### `computeUniformBufferObject()` signature change

**Old:**
```cpp
auto computeUniformBufferObject(vk::Extent2D extent, float time_seconds) const -> UniformBufferObject;
```

**New:**
```cpp
auto computeUniformBufferObject(
    const scene::Transform& transform,
    vk::Extent2D extent
) const -> UniformBufferObject;
```

- `time_seconds` parameter removed — rotation is now animated by mutating `transform.rotation.y`
  directly each frame (see main loop below), so the UBO helper just reads the current transform state
- View and proj computed the same way as before (same camera, same FOV)
- Model matrix: `transform.getModelMatrix()` — no additional correction matrix needed
  because the base orientation is encoded in the transform itself

### Main loop changes

```
// 1. Animate objects (mutate transform before computing UBOs)
for (auto& obj : objects_) {
    obj.transform.rotation.y += delta_time * ROTATION_SPEED;
}

// 2. Compute one UBO per object
std::vector<UniformBufferObject> ubos;
ubos.reserve(objects_.size());
for (const auto& obj : objects_) {
    ubos.push_back(computeUniformBufferObject(obj.transform, swap_chain_extent));
}

// 3. Draw
renderer_->drawFrame(swapChain, graphicsPipeline, ubos, depthView, msaaView, delta_time);
```

Add a `static constexpr float ROTATION_SPEED = 1.0f;` (radians per second) alongside the other constants.

---

## Build Order

Implement and compile-check in this order:

1. `scene/Transform.h` — no Vulkan deps, compiles in isolation
2. `scene/Object.h` — depends only on Transform
3. `renderer/ObjectRenderData.h` — Vulkan-Hpp + UniformBufferObject
4. `renderer/RenderFrameSlot.h` — simplification only, verify it still compiles with Renderer
5. `renderer/Renderer.h/.cpp` — largest change; get it compiling before touching Application
6. `renderer/GraphicsPipeline.h/.cpp` — update record() signature + body
7. `Application.h/.cpp` — wire everything together; this is where final integration errors surface

---

## Verification Tests

| # | What to test | How | Expected result |
|---|-------------|-----|-----------------|
| T1 | Three viking room models visible | Run the app | Three models at distinct positions and scales |
| T2 | Each model rotates independently | Watch for 5 seconds | All three rotate at the same rate around Y axis |
| T3 | All models share the same mesh | Check GPU memory (optional) | Only one vertex/index buffer on GPU |
| T4 | Resize works | Drag window edge | Models redraw correctly at new size |
| T5 | Minimise / restore works | Minimise then restore | No crash, rendering resumes |
| T6 | Validation layers silent | Run with `VK_LAYER_KHRONOS_validation` | No errors or warnings in stderr |
| T7 | Descriptor pool not exhausted | Validation layers | No `VK_ERROR_OUT_OF_POOL_MEMORY` |
