# Implementation Plan — Chapter 17: Multithreading

> **Ground truth for all implementation and code-review sessions.**
> Do not deviate from the decisions recorded here without an explicit architecture discussion.

---

## Architecture Decisions (agreed session 75)

| Decision | Choice |
|----------|--------|
| Thread count | `std::max(1u, std::thread::hardware_concurrency() - 1)` |
| Thread ownership | `Application` owns `ComputeThreadPool` as `unique_ptr` |
| Threading encapsulation | New `ComputeThreadPool` class in `src/renderer/compute/` |
| `ComputeThreadPool` contents | Worker threads, per-thread command pools + CBs + fences, signalling primitives, queue mutex |
| `drawFrame()` integration | `Renderer` calls `computeThreadPool_.dispatch(...)` — frame orchestration stays in one place |
| `PushConstants` struct location | `src/renderer/compute/ComputePipeline.h` — natural owner of its own push constant layout |
| Work distribution | `particles_per_thread = PARTICLE_COUNT / thread_count`; last thread takes remainder |
| Scene | Viking room + particles retained (same as Ch11) |
| Future refactor note | Injection chain flagged for "Building a Simple Engine" `RenderContext` refactor |

---

## Folder / File Map

```
src/
├── renderer/
│   ├── compute/
│   │   ├── ComputePipeline.h/.cpp       MODIFY — add PushConstants struct, push constant range to layout
│   │   └── ComputeThreadPool.h/.cpp     NEW — worker threads, per-thread pools/CBs/fences, dispatch()
│   └── Renderer.h/.cpp                  MODIFY — accept ComputeThreadPool ref, call dispatch() in drawFrame()
└── Application.h/.cpp                   MODIFY — own ComputeThreadPool, pass to Renderer
```

Shader: `shaders/particles.slang` — MODIFY: add `[[vk::push_constant]] PushConstants`, use `startIndex`

---

## Step 1 — `ComputePipeline.h/.cpp` (MODIFY)

### Add `PushConstants` struct to `ComputePipeline.h`

```cpp
struct PushConstants
{
    uint32_t startIndex;
    uint32_t count;
};
```

Declare it inside the `ComputePipeline` class or in the same header at file scope — accessible to `ComputeThreadPool` via `#include "ComputePipeline.h"`.

### Update `createPipelineLayout()` in `ComputePipeline.cpp`

Add a `vk::PushConstantRange` to the pipeline layout:

```
vk::PushConstantRange push_constant_range{
    vk::ShaderStageFlagBits::eCompute,
    0,
    sizeof(PushConstants)
};
```

Pass it to `vk::PipelineLayoutCreateInfo` via `setPushConstantRanges(push_constant_range)`.

**No other changes to `ComputePipeline`.**

---

## Step 2 — `shaders/particles.slang` (MODIFY)

Add push constant block to the compute entry point:

```slang
[[vk::push_constant]]
struct PushConstants
{
    uint startIndex;
    uint count;
};

ConstantBuffer<PushConstants> pushConstants;
```

In `compMain`:
- Replace `uint index = threadId.x` with bounds check against `pushConstants.count`
- Replace direct particle index with `uint globalIndex = pushConstants.startIndex + threadId.x`
- All particle reads/writes use `globalIndex` instead of `threadId.x`

Recompile to `particles.spv` and verify shader compiles before proceeding.

---

## Step 3 — `src/renderer/compute/ComputeThreadPool.h` (NEW)

Header-only declarations. Model on `ComputePipeline.h`.

```cpp
#pragma once
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vulkan/vulkan_raii.hpp>
#include "ComputePipeline.h"
#include "ParticleDescriptorLayout.h"

class VulkanContext;

class ComputeThreadPool
{
public:
    ComputeThreadPool(
        const VulkanContext& context,
        const ComputePipeline& compute_pipeline,
        const ParticleDescriptorLayout& particle_descriptor_layout,
        uint32_t thread_count
    );
    ~ComputeThreadPool();

    ComputeThreadPool(const ComputeThreadPool&) = delete;
    ComputeThreadPool& operator=(const ComputeThreadPool&) = delete;

    // Called from Renderer::drawFrame() — signals workers, waits for completion.
    // Returns the per-thread command buffers ready for submission.
    auto dispatch(
        std::span<const vk::raii::DescriptorSet*> compute_descriptor_sets,
        uint32_t particle_count,
        uint32_t current_frame
    ) -> std::vector<vk::CommandBuffer>;

private:
    void workerThreadFunc(uint32_t thread_index);

    void createCommandPoolsAndBuffers();
    void createFences();

    const VulkanContext&              context_;
    const ComputePipeline&            computePipeline_;
    const ParticleDescriptorLayout&   particleDescriptorLayout_;
    uint32_t                          threadCount_;

    // Per-thread GPU resources
    std::vector<vk::raii::CommandPool>   commandPools_;
    std::vector<vk::raii::CommandBuffer> commandBuffers_;
    std::vector<vk::raii::Fence>         fences_;

    // Threading primitives
    std::vector<std::thread>          workerThreads_;
    std::atomic<bool>                 shouldExit_{ false };
    std::vector<std::atomic<bool>>    workReady_;      // main → worker signal
    std::vector<std::atomic<bool>>    workDone_;       // worker → main signal
    std::condition_variable           workDoneCv_;
    std::mutex                        workDoneMutex_;

    // Per-dispatch state set by main thread before signalling workers
    std::vector<vk::DescriptorSet>    currentDescriptorSets_;  // one per thread (current frame)
    uint32_t                          currentParticleCount_{ 0 };
};
```

**Notes:**
- Non-copyable (threads and RAII handles are move-only at best; simplest to delete copy).
- `workReady_` and `workDone_` are `std::atomic<bool>` — lock-free signalling.
- `workerThreads_` is the last member — threads are started in the constructor body after all resources are ready.

---

## Step 4 — `src/renderer/compute/ComputeThreadPool.cpp` (NEW)

### Constructor

1. Store references, `threadCount_`.
2. Resize all per-thread vectors to `threadCount_`.
3. Call `createCommandPoolsAndBuffers()`.
4. Call `createFences()`.
5. Initialize `workReady_[i] = false`, `workDone_[i] = false` for all i.
6. Start worker threads — **last step**: `workerThreads_.emplace_back(&ComputeThreadPool::workerThreadFunc, this, i)`.

### Destructor

```cpp
shouldExit_ = true;
// Wake any sleeping workers so they can check shouldExit_
for (auto& flag : workReady_) flag = true;
for (auto& t : workerThreads_) if (t.joinable()) t.join();
// RAII handles clean up automatically
```

### `createCommandPoolsAndBuffers()`

For each thread i:
- Create `commandPools_[i]` with `eResetCommandBuffer` flag and the compute queue family index from `context_`.
- Allocate one primary command buffer from `commandPools_[i]` into `commandBuffers_[i]`.

### `createFences()`

For each thread i:
- Create `fences_[i]` with `eSignaled` flag (same pattern as `RenderFrameSlot` — signalled so first wait succeeds).

### `workerThreadFunc(uint32_t thread_index)`

```
while true:
    if shouldExit_: break
    if !workReady_[thread_index]: yield(); continue

    // Wait on fence (guard against previous frame still in flight)
    context_.getLogicalDevice().waitForFences(*fences_[thread_index], true, UINT64_MAX)
    context_.getLogicalDevice().resetFences(*fences_[thread_index])

    // Reset and record command buffer
    commandBuffers_[thread_index].reset()
    commandBuffers_[thread_index].begin(vk::CommandBufferBeginInfo{ eOneTimeSubmit })

    // Compute push constants for this thread's slice
    uint32_t particles_per_thread = currentParticleCount_ / threadCount_
    uint32_t start = thread_index * particles_per_thread
    uint32_t count = (thread_index == threadCount_ - 1)
                     ? currentParticleCount_ - start   // last thread takes remainder
                     : particles_per_thread
    PushConstants push{ start, count }

    commandBuffers_[thread_index].bindPipeline(eCompute, *computePipeline_.getPipeline())
    commandBuffers_[thread_index].bindDescriptorSets(eCompute, *computePipeline_.getPipelineLayout(), 0, currentDescriptorSets_[thread_index], {})
    commandBuffers_[thread_index].pushConstants(*computePipeline_.getPipelineLayout(), eCompute, 0, sizeof(PushConstants), &push)
    commandBuffers_[thread_index].dispatch((count + 255) / 256, 1, 1)
    commandBuffers_[thread_index].end()

    workDone_[thread_index] = true
    workReady_[thread_index] = false
    workDoneCv_.notify_one()
```

### `dispatch(...)`

```
// Store per-dispatch state for workers to read
currentParticleCount_ = particle_count
for i in [0, threadCount_):
    currentDescriptorSets_[i] = *compute_descriptor_sets[i]  // descriptor set for current_frame, slot i

// Signal all workers
for i in [0, threadCount_):
    workDone_[i] = false
    workReady_[i] = true

// Wait for all workers to finish
std::unique_lock lock(workDoneMutex_)
workDoneCv_.wait(lock, [&]{
    for i: if !workDone_[i]: return false
    return true
})

// Collect raw command buffer handles for submission
std::vector<vk::CommandBuffer> result
for i: result.push_back(*commandBuffers_[i])
return result
```

---

## Step 5 — `src/renderer/Renderer.h/.cpp` (MODIFY)

### `Renderer.h`

Add `#include "compute/ComputeThreadPool.h"` (or forward-declare + include in `.cpp` — forward declaration is cleaner since `ComputeThreadPool` is a reference member).

Add constructor parameter and reference member:
```cpp
const ComputeThreadPool& computeThreadPool_;
```

Add to constructor initializer list after `particleGraphicsPipeline_`.

### `Renderer.cpp` — `drawFrame()` changes

**Replace** the existing single-threaded compute block:
```cpp
// OLD: computePipeline_.record(...)
```

**With** the multithreaded dispatch:
```cpp
// Get per-thread CBs
auto compute_cbs = computeThreadPool_.dispatch(
    compute_descriptor_set_ptrs,  // one vk::raii::DescriptorSet* per thread, current frame
    particleCount_,
    currentFrame_
);

// Submit compute CBs (all threads' work in one submit)
vk::SubmitInfo compute_submit_info{};
compute_submit_info.setCommandBuffers(compute_cbs);
compute_submit_info.setSignalSemaphores(*compute_slot.computeFinishedSemaphore_);
context_.getComputeQueue().submit(compute_submit_info, *compute_slot.computeInflightFence_);
```

**Note:** `computeThreadPool_.dispatch()` blocks until all workers are done — the fence wait is inside the worker thread, not here. The main thread waits via condition variable.

**Note:** The compute descriptor sets passed to `dispatch()` are `computeFrameSlots_[i].descriptorSet_[currentFrame_]` — one pointer per worker thread (each thread records its own descriptor set bind). In Ch11 each compute slot had its own descriptor set already; the threading just records them from separate threads.

---

## Step 6 — `Application.h/.cpp` (MODIFY)

### `Application.h`

Add forward declaration:
```cpp
class ComputeThreadPool;
```

Add member (after `computePipeline_`, before `particleGraphicsPipeline_`):
```cpp
std::unique_ptr<ComputeThreadPool> computeThreadPool_;
```

Add `#include <thread>` for `std::thread::hardware_concurrency()`.

### `Application.cpp` — `initVulkan()`

After `computePipeline_` is created, before `particleGraphicsPipeline_`:

```cpp
uint32_t thread_count = std::max(1u, std::thread::hardware_concurrency() - 1);
computeThreadPool_ = std::make_unique<ComputeThreadPool>(
    *context_,
    *computePipeline_,
    *particleDescriptorLayout_,
    thread_count
);
```

Update `Renderer` constructor call to pass `*computeThreadPool_` as the new parameter.

### `CMakeLists.txt`

Add `src/renderer/compute/ComputeThreadPool.cpp` to the source list.

---

## Build Order

1. `ComputePipeline.h/.cpp` — add `PushConstants` + push constant range; compile-check
2. `shaders/particles.slang` — add push constants, recompile to `particles.spv`
3. `ComputeThreadPool.h` — header only, no deps beyond existing headers
4. `ComputeThreadPool.cpp` — implement; compile-check
5. `Renderer.h/.cpp` — add `computeThreadPool_` ref, update `drawFrame()`
6. `Application.h/.cpp` — wire `ComputeThreadPool` creation and pass to Renderer
7. Full build + Windows smoke tests

---

## Verification Tests

| # | What to test | How | Expected result |
|---|-------------|-----|-----------------|
| T1 | App launches | Run on Windows | Window opens, viking room + particles visible |
| T2 | Worker threads created | Add `std::cout` in constructor (remove after) | N thread messages at startup (N = hardware_concurrency - 1) |
| T3 | Particles animate correctly | Watch for 5 seconds | Same particle behaviour as Ch11 — no missing/frozen particles |
| T4 | All particles covered | Verify count stays at 128000 | No reduction in particle density vs Ch11 |
| T5 | Resize works | Drag window edge | Particles + viking room redraw correctly |
| T6 | Minimise / restore | Minimise then restore | No crash, rendering resumes |
| T7 | Validation layers silent | Run with `VK_LAYER_KHRONOS_validation` | No errors or warnings in stderr |
| T8 | No data race | Run with ThreadSanitizer (WSL2, gcc) | No races reported |
