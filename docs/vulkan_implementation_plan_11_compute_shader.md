# Implementation Plan — Chapter 11: Compute Shader

## Architecture Decisions

| Q | Decision |
|---|----------|
| Q1 — `Particle` struct location | `src/renderer/buffers/Particle.h` — GPU wire format, alongside `Vertex.h` |
| Q2 — Per-frame compute resources | Separate `ComputeFrameSlot` struct; `Renderer` holds parallel array alongside `renderFrameSlots_` |
| Q3 — Compute descriptor layout | `ParticleDescriptorLayout` in `src/renderer/compute/` — concrete name, not generic |
| Q4 — UBO for compute | Separate `ComputeUniformBufferObject { float deltaTime; }` in `src/renderer/compute/` — `UniformBufferObject` untouched |
| Q5 — `ComputePipeline` class | `src/renderer/compute/ComputePipeline.h/.cpp`, hardcoded `shaders/particles.spv`, `record(commandBuffer, descriptorSet, particle_count)` |
| Q6 — Synchronization | Binary semaphores — `computeFinishedSemaphore_` per slot; graphics submit waits at `eVertexInput`. Timeline semaphores deferred to "Building a Simple Engine" |
| Q7 — `drawFrame()` structure | Single `drawFrame()` handles both compute and graphics sections. Viking room mesh kept alongside particles |
| Q8 — Two rendering passes | Two separate dynamic rendering passes — `GraphicsPipeline` unchanged (pass 1, clears, depth write on); new `ParticleGraphicsPipeline` (pass 2, `loadOp=eLoad`, depth write off, `ePointList`) |

---

## Folder Structure After Chapter 11

```
src/renderer/
├── RenderFrameSlot.h            ← unchanged
├── ComputeFrameSlot.h           ← NEW
├── buffers/
│   ├── Vertex.h                 ← unchanged
│   ├── Mesh.h/.cpp              ← unchanged
│   └── Particle.h               ← NEW
├── compute/
│   ├── ComputeUniformBufferObject.h      ← NEW
│   ├── ParticleDescriptorLayout.h/.cpp   ← NEW
│   ├── ComputePipeline.h/.cpp            ← NEW
│   └── ParticleGraphicsPipeline.h/.cpp   ← NEW
├── descriptors/
│   ├── FrameDescriptorLayout.h/.cpp      ← unchanged
│   └── TextureDescriptorLayout.h/.cpp    ← unchanged
└── image_resources/
    ...                          ← unchanged
```

---

## New Files

### `src/renderer/buffers/Particle.h`
```
struct Particle {
    glm::vec2 position_;
    glm::vec2 velocity_;
    glm::vec4 color_;

    static auto getBindingDescription() -> vk::VertexInputBindingDescription;
    static auto getAttributeDescriptions() -> std::array<vk::VertexInputAttributeDescription, 2>;
    // location 0: position (eR32G32Sfloat)
    // location 1: color    (eR32G32B32A32Sfloat)
    // velocity_ is intentionally omitted — compute-only field
};
```

### `src/renderer/compute/ComputeUniformBufferObject.h`
```
struct ComputeUniformBufferObject {
    float deltaTime_;
};
```

### `src/renderer/compute/ParticleDescriptorLayout.h/.cpp`
Three bindings, all `eCompute`:
- Binding 0: `eUniformBuffer`  — `deltaTime` UBO
- Binding 1: `eStorageBuffer`  — previous frame particles (read)
- Binding 2: `eStorageBuffer`  — current frame particles (write)

Constructor: `ParticleDescriptorLayout(const VulkanContext&)`
Accessor: `auto getLayout() const -> const vk::raii::DescriptorSetLayout&`

### `src/renderer/ComputeFrameSlot.h`
```
struct ComputeFrameSlot {
    vk::raii::CommandBuffer  computeCommandBuffer_  = nullptr;
    vk::raii::Fence          computeInFlightFence_  = nullptr;
    vk::raii::Semaphore      computeFinishedSemaphore_ = nullptr;

    vk::raii::Buffer         particleBuffer_        = nullptr;
    vk::raii::DeviceMemory   particleMemory_        = nullptr;

    vk::raii::Buffer         computeUniformBuffer_  = nullptr;
    vk::raii::DeviceMemory   computeUniformMemory_  = nullptr;
    void*                    computeUniformMapped_  = nullptr;

    vk::raii::DescriptorSet  computeDescriptorSet_  = nullptr;

    void updateComputeUniformBuffer(const ComputeUniformBufferObject& cubo);
};
```
Move-only (= default move ctor + move assign; deleted copy).

### `src/renderer/compute/ComputePipeline.h/.cpp`
Constructor: `ComputePipeline(const VulkanContext&, const ParticleDescriptorLayout&)`
Owns: `pipelineLayout_`, `pipeline_`
Method: `void record(vk::CommandBuffer, const vk::raii::DescriptorSet&, uint32_t particle_count)`

`record()` body:
- `begin(eOneTimeSubmit)`
- `bindPipeline(eCompute, *pipeline_)`
- `bindDescriptorSets(eCompute, *pipelineLayout_, 0, {*descriptor_set}, {})`
- `dispatch(particle_count / 256, 1, 1)`
- `end()`

Shader entry point: `compMain` from `shaders/particles.spv`

### `src/renderer/compute/ParticleGraphicsPipeline.h/.cpp`
Constructor: `ParticleGraphicsPipeline(const VulkanContext&, const ParticleDescriptorLayout&, vk::Format color_format, vk::Format depth_format, vk::SampleCountFlagBits samples)`

Key pipeline differences from `GraphicsPipeline`:
- Topology: `ePointList`
- Vertex input: `Particle::getBindingDescription()` + `getAttributeDescriptions()`
- Color attachment `loadOp = eLoad` (preserve first pass)
- Depth `depthWriteEnable = false`, `depthTestEnable = true`
- Blending: additive (`srcColorBlendFactor = eSrcAlpha`, `dstColorBlendFactor = eOne`)
- `rasterizationSamples` = `samples` (same MSAA as graphics pipeline)

Method: `void record(vk::CommandBuffer, vk::Extent2D, vk::ImageView color_view, vk::ImageView depth_view, vk::ImageView msaa_view, const vk::raii::DescriptorSet&, vk::Buffer particle_buffer, uint32_t particle_count)`

`record()` body:
- `beginRendering()` with `loadOp = eLoad`, `storeOp = eStore`, same attachments as `GraphicsPipeline`
- `bindPipeline(eGraphics, *pipeline_)`
- `setViewport` / `setScissor`
- `bindDescriptorSets(eGraphics, ...)`
- `bindVertexBuffers(0, {particle_buffer}, {0})`
- `draw(particle_count, 1, 0, 0)`
- `endRendering()`
- Transition to `ePresentSrcKHR` (owned by this pass since it runs last)

Note: `GraphicsPipeline::record()` must no longer do the `ePresentSrcKHR` transition — that responsibility moves to `ParticleGraphicsPipeline` since it's the final pass. When particles are disabled, `GraphicsPipeline` would need to own the transition again. For this chapter, particles are always present.

---

## Modified Files

### `src/renderer/Renderer.h/.cpp`

**Constructor gains two new const-ref parameters:**
```cpp
Renderer(VulkanContext&,
         SwapChain&,
         const FrameDescriptorLayout&,
         const TextureDescriptorLayout&,
         GraphicsPipeline&,
         const ComputePipeline&,              // NEW
         const ParticleGraphicsPipeline&)     // NEW
```

**New members:**
```cpp
const ComputePipeline&          computePipeline_;
const ParticleGraphicsPipeline& particleGraphicsPipeline_;
std::vector<ComputeFrameSlot>   computeFrameSlots_;
vk::raii::DescriptorPool        computeDescriptorPool_ = nullptr;
uint32_t                        particleCount_         = 0;
```

**New private methods:**
- `initializeComputeFrameSlots()` — creates per-slot: command buffer, fence (signalled), semaphore, UBO buffer + map, SSBO, descriptor set wired to (prev-slot SSBO, current-slot SSBO, UBO)
- `createComputeDescriptorPool()` — `maxSets = MAX_FRAMES_IN_FLIGHT`, pool sizes: `eUniformBuffer × MAX_FRAMES_IN_FLIGHT`, `eStorageBuffer × MAX_FRAMES_IN_FLIGHT × 2`

**New public method:**
```cpp
void createParticleSystem(const std::vector<Particle>& particles);
```
Stores `particle_count_`, stages initial particle data into all `computeFrameSlots_[i].particleBuffer_` via existing `uploadBufferToDevice` helper, then calls `initializeComputeFrameSlots()`.

**`drawFrame()` gains `float delta_time` parameter:**

New compute section (runs BEFORE existing graphics section):
```
1. waitForFences(computeInFlightFence_[currentFrame_])
2. computeFrameSlots_[currentFrame_].updateComputeUniformBuffer({delta_time})
3. resetFences(computeInFlightFence_[currentFrame_])
4. computeCommandBuffer_.reset()
5. computePipeline_.record(computeCommandBuffer, computeDescriptorSet, particleCount_)
6. submit to graphicsQueue_:
   - waitSemaphores: none
   - signalSemaphores: computeFinishedSemaphore_[currentFrame_]
   - fence: computeInFlightFence_[currentFrame_]
```

Modified graphics submit (step previously labelled "submit"):
```
waitSemaphores:    [imageAvailableSemaphore_,     computeFinishedSemaphore_[currentFrame_]]
waitStageMasks:    [eColorAttachmentOutput,        eVertexInput]
signalSemaphores:  [renderFinishedSemaphores_[imageIndex]]
```

Modified record section — two `record()` calls:
```
graphicsPipeline_.record(...)          // pass 1: viking room, clears, depth write on
particleGraphicsPipeline_.record(...)  // pass 2: particles, loadOp=eLoad, depth write off
```

### `src/Application.h/.cpp`

**New members (in declaration/destruction order — particles destroyed before pipelines):**
```cpp
std::unique_ptr<ParticleDescriptorLayout>    particleDescriptorLayout_;
std::unique_ptr<ComputePipeline>             computePipeline_;
std::unique_ptr<ParticleGraphicsPipeline>    particleGraphicsPipeline_;
```

**`initVulkan()` additions (after existing layout/pipeline construction):**
```
particleDescriptorLayout_ = make_unique<ParticleDescriptorLayout>(*context_)
computePipeline_          = make_unique<ComputePipeline>(*context_, *particleDescriptorLayout_)
particleGraphicsPipeline_ = make_unique<ParticleGraphicsPipeline>(*context_, *particleDescriptorLayout_,
                                swapChain_->getFormat(), context_->findDepthFormat(),
                                context_->getMsaaSamples())
renderer_                 = make_unique<Renderer>(..., *computePipeline_, *particleGraphicsPipeline_)
renderer_->createParticleSystem(generateParticles())
```

**New private helper:**
```cpp
auto generateParticles() const -> std::vector<Particle>;
```
Generates `PARTICLE_COUNT` particles in a ring pattern using `std::default_random_engine` + `std::uniform_real_distribution`. Called once in `initVulkan()`.

**`mainLoop()` / `drawFrame()` call gains `delta_time`:**
```cpp
float delta_time = std::chrono::duration<float>(now - lastFrameTime_).count();
lastFrameTime_   = now;
renderer_->drawFrame(ubo, delta_time, ...);
```
New member `lastFrameTime_` of type `std::chrono::high_resolution_clock::time_point`, initialised in constructor initialiser list.

**New constant:**
```cpp
static constexpr uint32_t PARTICLE_COUNT = 256 * 500; // 128000, divisible by numthreads(256)
```

### `shaders/particles.slang`

Three entry points in one file:
- `[shader("compute")] [numthreads(256,1,1)] void compMain(uint3 threadId : SV_DispatchThreadID)` — reads `particlesIn[index]`, writes position + velocity to `particlesOut[index]`, bounces at ±1 boundary
- `[shader("vertex")] VertexOutput vertMain(VertexInput input)` — passes position + color through, no MVP transform (particles live in NDC space)
- `[shader("fragment")] float4 fragMain(VertexOutput input)` — outputs `input.color`

Bindings:
- `[vk::binding(0, 0)] ConstantBuffer<ComputeUniformBuffer> ubo` — `deltaTime`
- `[vk::binding(1, 0)] StructuredBuffer<ParticleSSBO> particlesIn`
- `[vk::binding(2, 0)] RWStructuredBuffer<ParticleSSBO> particlesOut`

### `CMakeLists.txt`

New source files:
```cmake
src/renderer/compute/ParticleDescriptorLayout.cpp
src/renderer/compute/ComputePipeline.cpp
src/renderer/compute/ParticleGraphicsPipeline.cpp
```

New shader target:
```cmake
add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/shaders/particles.spv
    COMMAND ${SLANGC} ${CMAKE_SOURCE_DIR}/shaders/particles.slang
            -o ${CMAKE_BINARY_DIR}/shaders/particles.spv
            -profile glsl_450 -target spirv
    DEPENDS ${CMAKE_SOURCE_DIR}/shaders/particles.slang)
```

---

## Build Order

Work bottom-up. Build and verify clean compile after each step before moving to the next.

| Step | File(s) | What to verify |
|------|---------|----------------|
| 1 | `Particle.h` | Struct compiles; binding/attribute descriptions match `Vertex.h` pattern |
| 2 | `ComputeUniformBufferObject.h` | Header-only; include in step 3 |
| 3 | `ParticleDescriptorLayout.h/.cpp` + CMake entry | Clean compile |
| 4 | `ComputeFrameSlot.h` | Header-only; `updateComputeUniformBuffer` one-liner |
| 5 | `ComputePipeline.h/.cpp` + CMake entry | Clean compile; `shaders/particles.slang` compute entry written |
| 6 | `ParticleGraphicsPipeline.h/.cpp` + CMake entry | Clean compile; particles.slang vertex+fragment entries written |
| 7 | `Renderer.h/.cpp` — add compute members, `createParticleSystem()`, `initializeComputeFrameSlots()` | Clean compile |
| 8 | `Renderer::drawFrame()` — add compute section, second wait semaphore, two `record()` calls | Clean compile |
| 9 | `Application.h/.cpp` — new members, `generateParticles()`, `lastFrameTime_`, updated `initVulkan()` and `mainLoop()` | Full build clean |
| 10 | Windows build + smoke tests | See verification bar below |

---

## Smoke Test Bar

| Test | Expected result |
|------|----------------|
| T1 | Viking room renders correctly, texture visible, depth occlusion correct |
| T2 | Particles visible as coloured points, moving and bouncing at window edges |
| T3 | Particle motion is frame-rate independent (speed consistent) |
| T4 | MSAA applies to both passes (no jagged edges on viking room, particles are points so N/A) |
| T5 | Window resize — both viking room and particles resize correctly, no crash |
| T6 | Minimise / restore — no crash, both objects reappear correctly |
| T7 | Validation layers silent (OBS_HOOK harmless) — no sync hazards, no usage mismatches |
