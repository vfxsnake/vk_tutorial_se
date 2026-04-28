# Implementation Plan — Chapter 05: Uniform Buffers

> **Agreed during architecture discussion — Sessions 35–37, 2026-04-24 to 2026-04-28**
> **Prerequisite:** Chapter 04 complete, indexed rectangle visible, resize clean, all Ch04 tests passing. Pre-Ch05 semaphore refactor merged (`09f6d48`).

---

## Architectural Decisions

| # | Decision | Choice | Rationale |
|---|----------|--------|-----------|
| Q1 | UBO struct location | `src/renderer/buffers/UniformBufferObject.h` | Pure GPU wire format — sibling to `Vertex.h`. Permanent home; never migrates to `scene/`. |
| Q2 | Descriptor pool ownership | `Renderer` owns the per-frame UBO pool | Pool home is dictated by what it allocates. Material/texture pool will live on future `ResourceManager` (Ch06+). |
| Q3 | Per-frame UBO resources | On `RenderFrameSlot` | One slot = everything needed to render one frame in flight. |
| Q4 | Descriptor set layout | Standalone class `FrameDescriptorLayout` in new `src/renderer/descriptors/` folder | Avoids `Renderer` becoming a grab-bag. Single-purpose; Ch06 textures get a sibling class, not additions here. |
| Q5 | Pool sizing & flags | `maxSets = MAX_FRAMES_IN_FLIGHT`, `poolSizes = [{eUniformBuffer, MAX_FRAMES_IN_FLIGHT}]`, `flags = {}` | Sets allocated once, never freed individually → no `eFreeDescriptorSet` → driver can use cheaper bump allocator. |
| Q5 | Pool lifetime | Created in `Renderer` constructor; never recreated on resize | Pool is indexed by frame-in-flight (compile-time constant), not by swap-chain image count. |
| Q6 | UBO write API | `RenderFrameSlot::updateUniformBuffer(const UniformBufferObject&)` — one-line `memcpy` | Slot owns `uniformMapped_`; encapsulates the write. `Renderer` never touches the mapped pointer. |
| Q6 | UBO compute & flow | `Application` computes UBO each frame → passes `const UniformBufferObject&` into `Renderer::drawFrame()` → routes to slot | Application owns world/camera context; Renderer is pure submission; slot owns the GPU bytes. |
| Q6 | Placement of `updateUniformBuffer` call | In `Renderer::drawFrame()`, between `resetFences()` and `commandBuffer_.reset()` | "Update CPU-side state before recording GPU work that consumes it." |
| Q7a | MVP math home | Private helper on `Application`: `computeUniformBufferObject(extent, time_seconds) → UniformBufferObject` | Migrates cleanly to `Camera` in `scene/` (Ch08+). |
| Q7b | Wall-clock origin | `Application` member `std::chrono::high_resolution_clock::time_point startTime_` initialised in constructor | Visible in class layout; sets precedent for future `Camera` clock; testable. |
| Q7c | Y-flip | **Negative viewport height** in `GraphicsPipeline::record()` (`viewport.y = extent.height; viewport.height = -extent.height`) | Modern Vulkan 1.4 idiom; keeps `glm::perspective` matrix unmodified; conventional CCW winding works without surprises. |
| Q7c | Depth range | Project-wide `GLM_FORCE_DEPTH_ZERO_TO_ONE` (CMake compile definition) | Forces GLM to emit Vulkan-convention `[0,1]` depth from `glm::perspective`. |
| Q7c | Angle convention | Project-wide `GLM_FORCE_RADIANS` (CMake compile definition) | Locks unit convention; required by the new `glm::rotate` calls. |

---

## Folder Structure After Chapter 05

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
│   ├── buffers/
│   │   ├── Vertex.h
│   │   ├── UniformBufferObject.h            ← NEW
│   │   └── Mesh.h/.cpp
│   └── descriptors/                          ← NEW folder
│       └── FrameDescriptorLayout.h/.cpp     ← NEW
├── scene/                                    (still empty)
└── utils/
    └── FileUtils.h
```

---

## New Files

### `src/renderer/buffers/UniformBufferObject.h`

Header-only. No `.cpp`.

```
struct UniformBufferObject
    glm::mat4 model
    glm::mat4 view
    glm::mat4 proj
```

Notes:
- Field order is the wire layout — must match the shader's `UniformBufferObject` field order exactly.
- `glm::mat4` is naturally 16-byte aligned, satisfying std140 mat4 alignment without explicit `alignas`.
- No methods. Pure POD wire format.

---

### `src/renderer/descriptors/FrameDescriptorLayout.h` / `FrameDescriptorLayout.cpp`

```
class FrameDescriptorLayout
    constructor(const VulkanContext& context)
    delete copy ctor + copy assignment

    getLayout() const → const vk::raii::DescriptorSetLayout&

private:
    vk::raii::DescriptorSetLayout layout_
```

Constructor body:
1. Build one `vk::DescriptorSetLayoutBinding`:
   - `binding = 0`
   - `descriptorType = eUniformBuffer`
   - `descriptorCount = 1`
   - `stageFlags = eVertex`
2. Build `vk::DescriptorSetLayoutCreateInfo` with that single binding.
3. Construct `layout_` from `context.getLogicalDevice()` and the create-info.
   - **Reminder:** never pass bare `{}` for the create-info — use the explicit type per `feedback_vk_raii_construction.md`.

---

## Modified Files

### `src/renderer/RenderFrameSlot.h`

**New members:**

```
vk::raii::Buffer       uniformBuffer_       = nullptr
vk::raii::DeviceMemory uniformMemory_       = nullptr
void*                  uniformMapped_       = nullptr
vk::raii::DescriptorSet descriptorSet_      = nullptr
```

**New method:**

```
updateUniformBuffer(const UniformBufferObject& ubo) → void
    std::memcpy(uniformMapped_, &ubo, sizeof(ubo))
```

Notes:
- `uniformMapped_` lifetime is tied to `uniformMemory_` — when the device memory's destructor fires, the implicit unmap happens. No manual unmap.
- Persistent mapping pattern: call `mapMemory` once at slot creation; never unmap.
- Member initialisation order matches destruction order (RAII).

---

### `src/renderer/Renderer.h` / `Renderer.cpp`

**Constructor signature change:**

```
// Before:
Renderer(VulkanContext& context)

// After:
Renderer(VulkanContext& context, const FrameDescriptorLayout& frame_layout)
```

**New private member:**

```
const FrameDescriptorLayout& frameLayout_
vk::raii::DescriptorPool     descriptorPool_ = nullptr
```

**New private method:**

```
createDescriptorPool() → void
```

Body:
1. One `vk::DescriptorPoolSize`: `{eUniformBuffer, MAX_FRAMES_IN_FLIGHT}`
2. `vk::DescriptorPoolCreateInfo`:
   - `flags = {}`
   - `maxSets = MAX_FRAMES_IN_FLIGHT`
   - `poolSizeCount = 1`
   - `pPoolSizes = &poolSize`
3. Construct `descriptorPool_` (explicit create-info type, not `{}`).

**Constructor order:**
1. Store `context_`, `frameLayout_`
2. `createCommandPool()`
3. `createDescriptorPool()`     ← NEW
4. `initializeFrameData()`       ← body extended (see below)

**`initializeFrameData()` per-slot extension** (in addition to existing command buffer / fence / imageAvailableSemaphore):

For each `slot` in `renderFrameSlots_`:
1. Allocate the UBO buffer + memory via existing `createBuffer()` helper:
   - size = `sizeof(UniformBufferObject)`
   - usage = `eUniformBuffer`
   - properties = `eHostVisible | eHostCoherent`
   - Move pair into `slot.uniformBuffer_` and `slot.uniformMemory_`.
2. Map memory: `slot.uniformMapped_ = slot.uniformMemory_.mapMemory(0, sizeof(UBO))`. Persistent — never unmapped.
3. Allocate the descriptor set:
   - `vk::DescriptorSetAllocateInfo` with `descriptorPool_`, `descriptorSetCount = 1`, `pSetLayouts = &*frameLayout_.getLayout()`
   - `auto sets = device.allocateDescriptorSets(allocInfo);` → move `sets.front()` into `slot.descriptorSet_`.
4. Wire the set to the buffer: build `vk::DescriptorBufferInfo {buffer = *slot.uniformBuffer_, offset = 0, range = sizeof(UBO)}` + `vk::WriteDescriptorSet {dstSet = *slot.descriptorSet_, dstBinding = 0, descriptorType = eUniformBuffer, descriptorCount = 1, pBufferInfo = &bufferInfo}`. Call `device.updateDescriptorSets({write}, {})`.

**`drawFrame()` signature change:**

```
// Before:
drawFrame(SwapChain&, GraphicsPipeline&, const Mesh&) → bool

// After:
drawFrame(SwapChain&, GraphicsPipeline&, const Mesh&, const UniformBufferObject&) → bool
```

**`drawFrame()` body changes:**

Two insertions, no deletions:

1. **Right after `resetFences(...)` and before `commandBuffer_.reset()`** (between current lines 99 and 102):
   - `renderFrameSlots_[currentFrame_].updateUniformBuffer(ubo);`

2. **In the `graphics_pipeline.record(...)` call** — add `slot.descriptorSet_` as a new parameter (see GraphicsPipeline section below).

---

### `src/renderer/GraphicsPipeline.h` / `GraphicsPipeline.cpp`

**Constructor signature change:**

```
// Before:
GraphicsPipeline(const VulkanContext& context, vk::Format color_format)

// After:
GraphicsPipeline(const VulkanContext& context,
                 vk::Format color_format,
                 const FrameDescriptorLayout& frame_layout)
```

**New private member:**

```
const FrameDescriptorLayout& frameLayout_
```

**`createPipelineLayout()` change:**

- `vk::PipelineLayoutCreateInfo`:
  - `setLayoutCount = 1`
  - `pSetLayouts = &*frameLayout_.getLayout()`
  - (push constants stay zero/null)

**`record()` signature change:**

```
// Before:
record(vk::CommandBuffer, vk::Extent2D, vk::Image, vk::ImageView, const Mesh&) → void

// After:
record(vk::CommandBuffer, vk::Extent2D, vk::Image, vk::ImageView, const Mesh&,
       const vk::raii::DescriptorSet& descriptor_set) → void
```

**`record()` body changes:**

1. **Negative viewport height** (replaces the existing viewport setup in record):
   - `viewport.x = 0.0f`
   - `viewport.y = static_cast<float>(extent.height)`
   - `viewport.width = static_cast<float>(extent.width)`
   - `viewport.height = -static_cast<float>(extent.height)`
   - `viewport.minDepth = 0.0f`
   - `viewport.maxDepth = 1.0f`
2. **Bind descriptor set** — after `bindPipeline(eGraphics, *pipeline_)`, before `mesh.bind(...)`:
   - `command_buffer.bindDescriptorSets(eGraphics, *layout_, 0, *descriptor_set, {});`

Front-face winding: stays `eClockwise` for now — Ch05 still uses 2D rectangle. Revisit when 3D content lands in Ch07.

---

### `src/Application.h` / `Application.cpp`

**New includes:** `<chrono>`, `"renderer/descriptors/FrameDescriptorLayout.h"`, `"renderer/buffers/UniformBufferObject.h"`.

**New member declarations:**

```
std::unique_ptr<FrameDescriptorLayout> frameDescriptorLayout_
std::chrono::high_resolution_clock::time_point startTime_
```

Member declaration order (controls destruction order — last-declared is destroyed first):

```
context_
swapChain_
frameDescriptorLayout_     ← NEW (must outlive Pipeline and Renderer)
graphicsPipeline_
renderer_
mesh_
```

**New private method:**

```
computeUniformBufferObject(vk::Extent2D extent, float time_seconds) const
    → UniformBufferObject
```

Body:
- `model = glm::rotate(glm::mat4(1.0f), time_seconds * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f))`
- `view  = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f))`
- `proj  = glm::perspective(glm::radians(45.0f), extent.width / static_cast<float>(extent.height), 0.1f, 10.0f)`
- **No** `proj[1][1] *= -1` — Y-flip is in the viewport.
- Return `{model, view, proj}`.

**Constructor change:**

- Initialise `startTime_ = std::chrono::high_resolution_clock::now();`

**`initVulkan()` change — additions in order:**

1. After `swapChain_` is constructed, before `graphicsPipeline_`:
   - `frameDescriptorLayout_ = std::make_unique<FrameDescriptorLayout>(*context_);`
2. Pass `*frameDescriptorLayout_` into both `GraphicsPipeline` and `Renderer` constructors.
3. Rest unchanged: `renderer_->initializePerImageResources(swapChain_->getImageCount());`, `mesh = renderer_->createMesh(vertices, indices);`.

**`mainLoop()` per-frame change:**

Inside the `while (!glfwWindowShouldClose(window_))` body, before `renderer_->drawFrame(...)`:

1. `auto now = std::chrono::high_resolution_clock::now();`
2. `float time_seconds = std::chrono::duration<float>(now - startTime_).count();`
3. `auto ubo = computeUniformBufferObject(swapChain_->getExtent(), time_seconds);`
4. `renderer_->drawFrame(*swapChain_, *graphicsPipeline_, *mesh_, ubo);`

---

### `shaders/triangle.slang`

**Add UBO struct + binding:**

```hlsl
struct UniformBufferObject {
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

[[vk::binding(0)]]
ConstantBuffer<UniformBufferObject> ubo;
```

**Vertex shader change:**

Replace the current passthrough:
```
out_vertex.position = float4(vertex_in.position, 0.0, 1.0);
```
with:
```
out_vertex.position = mul(ubo.proj,
                          mul(ubo.view,
                              mul(ubo.model,
                                  float4(vertex_in.position, 0.0, 1.0))));
```

Fragment shader: no changes.

---

### `CMakeLists.txt`

**Source list — add:**

```
src/renderer/descriptors/FrameDescriptorLayout.cpp
```

**Compile definitions — add (project-wide):**

```
target_compile_definitions(VulkanTutorial PRIVATE
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_RADIANS)
```

These must be set before any GLM header is included anywhere in the project. Compile-definition scope is the safest place — no risk of a missed include guard.

---

## Build Order

Bottom-up, each step compiles cleanly before moving on. Use `main.cpp` as a temp test harness only if needed; the chain below should keep it building end-to-end.

| Step | File(s) | What to verify |
|------|---------|----------------|
| 1 | `UniformBufferObject.h` | Standalone include compiles. |
| 2 | `CMakeLists.txt` — add `GLM_FORCE_*` defines | Project still builds; no GLM warnings. |
| 3 | `FrameDescriptorLayout.h/.cpp` + `CMakeLists.txt` source entry | Include from a temp spot, construct + destruct cleanly under validation. |
| 4 | `RenderFrameSlot.h` — add UBO members + `updateUniformBuffer` method | Existing call sites still compile (only new members added). |
| 5 | `Renderer` — add `frameLayout_` member, `descriptorPool_`, `createDescriptorPool()`, extend `initializeFrameData()` | `Renderer` constructs cleanly; validation silent on pool + sets + writes. |
| 6 | `Renderer::drawFrame` — add UBO param + slot.update call | Call site in `Application` now broken (expected). |
| 7 | `GraphicsPipeline` — constructor takes layout, `createPipelineLayout()` uses it, `record()` takes set + binds it, viewport flipped | Pipeline builds; validation silent. |
| 8 | `Application` — `frameDescriptorLayout_`, `startTime_`, `computeUniformBufferObject()`, mainLoop computes UBO + passes to `drawFrame` | Project links; rectangle still renders (first frame may show distortion until shader updated). |
| 9 | `triangle.slang` — UBO binding + matrix multiply | Recompile shader to `.spv`. |
| 10 | Full build + run | Spinning rectangle visible; aspect ratio preserved on resize; validation layer silent. |

---

## Verification (smoke-test bar)

These are the gates the implementation must clear; the formal test matrix lives in the learning plan.

- Rectangle spins around Z axis at 90°/s.
- Aspect ratio remains correct on window resize (no stretching).
- Resize / minimise / maximise cycle clean, no validation errors.
- `VK_LAYER_KHRONOS_validation` silent on startup, every frame, and shutdown (OBS_HOOK warning excluded — third-party).
- Clean exit (no leaked descriptor sets, no leaked buffers).
