# Chapter 17: Multithreading

> **Source:** https://docs.vulkan.org/tutorial/latest/17_Multithreading.html
> **Vulkan Version:** 1.4 | **Language:** C++20 | **Shading:** Slang | **Bindings:** Vulkan-Hpp RAII

---

## Overview

This chapter extends the particle system from Chapter 11 to distribute compute command buffer recording across multiple CPU threads. The result is that the CPU no longer records all particle commands on the main thread — worker threads each take a slice of the particle array and record their own compute dispatch, in parallel with each other and with the main thread.

Vulkan's design makes this possible. The API was built from the ground up for multithreading: command buffer recording is thread-safe as long as each thread owns its own command pool, explicit synchronization is your responsibility (so you are never fighting hidden driver locks), and queue submission is the only global bottleneck that needs a mutex.

The chapter also covers secondary command buffers, a generic thread pool, and async resource loading as advanced follow-on techniques.

---

## Why Vulkan Supports Multithreading Well

### Concepts

The core observation is that most of a frame's CPU work — recording commands — is embarrassingly parallel. In OpenGL and Direct3D 11, a global driver state machine serialised all recording. Vulkan has no such state: a `vk::CommandBuffer` is a standalone object with no shared mutable state once allocated. Two threads can record to two different command buffers simultaneously with no synchronization required between them.

Three properties enable this:

1. **Thread-safe command buffer recording** — recording to separate command buffers requires no locks. The only requirement is that each thread uses its own command pool (pools are not thread-safe internally).
2. **Explicit synchronization** — Vulkan never hides locks inside API calls. Every hazard you care about is expressed through fences, semaphores, barriers, or mutexes you write yourself. There are no surprise global locks.
3. **Queue-based architecture** — compute, graphics, and transfer work can be submitted to different queues. Where hardware exposes separate async compute queues, compute and graphics can execute on the GPU in parallel as well as the CPU.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::raii::CommandPool` | Object | Allocates command buffers; **not thread-safe** — one pool per thread |
| `vk::raii::CommandBuffer` | Object | Records GPU commands; thread-safe to record if owned by one thread |
| `vk::Queue::submit2` | Function | Submits command buffers to a queue; **must be externally serialised** |
| Push constants | Shader interface | Passes per-dispatch data (e.g. particle range) inline — no buffer needed |

### Common Pitfalls

- Sharing a command pool between threads causes data races — allocating or resetting a pool from two threads simultaneously is undefined behaviour.
- Submitting to the same queue from two threads simultaneously is also undefined. Protect every `submit` call with a mutex.
- Resetting a command buffer that is still in flight crashes or corrupts. Fences guard this — wait on the per-thread fence before resetting.

---

## Per-Thread Command Pools

### Concepts

A command pool is the memory allocator for command buffers. Allocation from a pool involves internal bookkeeping that is not thread-safe. The solution is simple: give every thread its own pool. Command buffers allocated from different pools have no shared state, so they can be recorded in parallel without locks.

Each worker thread holds:
- One `vk::raii::CommandPool` — created at startup, reset (not destroyed) each frame.
- One or more `vk::raii::CommandBuffer` objects allocated from that pool.
- A fence per frame to guard against re-recording a buffer still in flight.

The main thread keeps its existing command pool and command buffer for graphics recording. Worker threads get their own pools purely for compute recording.

### Common Pitfalls

- Do not create a new command pool per frame — creation has overhead. Create once at startup, reset with `commandPool.reset()` at the start of each frame before re-recording.
- `vk::CommandPoolCreateFlagBits::eResetCommandBuffer` enables per-buffer reset if you prefer that over pool-level reset; either is fine.

---

## Splitting Work Across Threads with Push Constants

### Concepts

With one worker thread per particle group, each thread needs to know which slice of the particle buffer it owns. The cleanest way to pass this per-dispatch information is **push constants** — a small inline data block (minimum 128 bytes guaranteed) written into the command buffer at record time with `vkCmdPushConstants`. No descriptor set, no buffer, no upload — just a struct baked into the command stream.

The struct typically looks like:

```
struct PushConstants {
    uint startIndex;  // first particle index this thread owns
    uint count;       // number of particles this thread processes
};
```

The compute shader reads these and uses `startIndex + threadId.x` as its global particle index, bailing out if `threadId.x >= count`.

The CPU divides `PARTICLE_COUNT` evenly across threads:

```
uint32_t particles_per_thread = PARTICLE_COUNT / threadCount;
// thread i owns [i * particles_per_thread, (i+1) * particles_per_thread)
```

Each worker thread records one dispatch: `dispatch(particles_per_thread / 256, 1, 1)`.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::PushConstantRange` | Struct | Declares push constant block in pipeline layout — stage flags + offset + size |
| `commandBuffer.pushConstants(...)` | Method | Writes push constant data into the command stream at record time |
| `[[vk::push_constant]]` | Slang attribute | Marks the push constant block in the shader |

### Code Walkthrough

**Pipeline layout change:** `vk::PushConstantRange` is added to the `vk::PipelineLayoutCreateInfo` for the compute pipeline. It covers `eCompute` stage, offset 0, size `sizeof(PushConstants)`.

**Shader change:** the compute entry point gets a `[[vk::push_constant]] PushConstants push_constants` parameter. The dispatch logic uses `push_constants.startIndex + index` to find the global particle.

**Record-time:** before `dispatch()`, the worker calls `commandBuffer.pushConstants(*pipelineLayout_, vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstants), &push_data)`.

### Common Pitfalls

- Push constant size must be declared consistently in the pipeline layout and the shader struct. A mismatch triggers validation errors.
- Push constants are not persistent — they must be set every time you record the command buffer.

---

## Worker Thread Architecture

### Concepts

The threading model used in the tutorial is a **persistent worker thread pool** — threads are created once at startup and live until the application exits. Each frame, the main thread signals workers to start, waits for them to finish, then submits all their command buffers together.

The main thread coordinates work with per-thread atomic flags and condition variables:

```
Main thread loop:
  1. Set threadWorkReady[i] = true for each worker
  2. Record graphics command buffer (main thread's own work)
  3. Wait until all threadWorkDone[i] == true
  4. Submit all compute CBs + graphics CB to queue
  5. Present

Worker thread loop (runs forever):
  while (!shouldExit):
    if !threadWorkReady[i]: yield(); continue
    record compute CB for particle slice i
    threadWorkDone[i] = true
    threadWorkReady[i] = false
    notify main thread
```

The queue submission step uses a mutex because `vk::Queue::submit` is not thread-safe.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `std::thread` | C++ stdlib | OS thread; created once, joined at shutdown |
| `std::atomic<bool>` | C++ stdlib | Lock-free flag for signalling between threads |
| `std::condition_variable` | C++ stdlib | Efficient wait/notify instead of busy-spinning |
| `std::mutex` | C++ stdlib | Serialises queue submission |

### Common Pitfalls

- Busy-spinning on atomics (`while (!flag) yield()`) works but wastes CPU. Condition variables are more efficient for long waits.
- Joining threads on shutdown requires setting `shouldExit = true` before joining, and worker threads must check it.
- Never submit multiple command buffers recorded for the same frame slot simultaneously from different threads — only one thread submits (main thread, after workers signal done).

---

## Secondary Command Buffers

### Concepts

Secondary command buffers are an alternative parallelism strategy: instead of each thread recording its own primary command buffer to be submitted separately, threads record **secondary** buffers that a single primary buffer executes via `vkCmdExecuteCommands`. This is useful when you want parallel recording of a single renderpass.

In dynamic rendering (Vulkan 1.3+), secondary command buffers that record rendering commands need the `eRenderPassContinue` flag and a `VkCommandBufferInheritanceRenderingInfo` struct in the inheritance chain describing the attachment formats.

This is the more complex pattern and the tutorial presents it as an advanced technique — not used in the main particle example.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::CommandBufferLevel::eSecondary` | Enum | Allocates a secondary command buffer |
| `vk::CommandBufferInheritanceInfo` | Struct | Tells secondary CB which renderpass context it inherits |
| `vk::CommandBufferUsageFlagBits::eRenderPassContinue` | Flag | Required for secondary CBs recording rendering commands |
| `commandBuffer.executeCommands(secondaryCBs)` | Method | Primary CB executes recorded secondary CBs |

---

## Performance Considerations

Threading has overhead. The main costs are thread creation (amortised to zero if you use a persistent pool), synchronisation (atomic ops, mutex contention), and cache effects (false sharing if threads write to adjacent memory). You will only see a benefit if each thread has enough work to justify these costs.

Rules of thumb:
- **Create threads at startup**, not per-frame.
- **One command pool per thread** — never shared.
- **One mutex for queue submission** — this is the only global lock in the hot path.
- **Measure, don't assume** — on a fast GPU with a small particle count the CPU overhead of threading can exceed the recording time you save. Profile with and without.
- **Work granularity matters** — splitting 128 000 particles across 4 threads gives ~32 000 particles per thread, which is a reasonable dispatch size. Splitting 256 particles across 4 threads is likely slower than single-threaded.

---

## Summary

- Vulkan supports multithreaded command recording natively: each thread gets its own command pool, records independently, and the main thread submits everything together.
- Push constants are the right mechanism for per-dispatch data (particle start index + count) — inline, no buffer required.
- The worker thread model: create at startup, signal via atomics/condition variables each frame, join at shutdown.
- Queue submission must be serialised with a mutex — it is the only global bottleneck.
- Secondary command buffers enable parallel recording into a single primary buffer but add complexity; useful for renderpass parallelism, not needed for compute splits.

## Implementation Checklist

- [ ] `PushConstants` struct added to `ComputePipeline` — `startIndex`, `count`
- [ ] `vk::PushConstantRange` added to `ComputePipeline::createPipelineLayout()`
- [ ] Compute shader updated to read `[[vk::push_constant]] PushConstants` and use global index
- [ ] Per-thread command pools created at startup (one per worker thread)
- [ ] Per-thread fences created (guarding re-record of in-flight CBs)
- [ ] Worker threads created at startup, joined at shutdown
- [ ] `drawFrame()` signals workers, records graphics CB, waits for workers, submits all CBs
- [ ] Queue submission wrapped in mutex
- [ ] Particle dispatch split evenly across threads via push constants

## Further Reading

- [Vulkan Spec — Command Buffer Lifecycle](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html)
- [Vulkan Spec — Push Constants](https://docs.vulkan.org/spec/latest/chapters/resources.html#descriptorsets-push-constants)
- [Vulkan Spec — Queue Submission](https://docs.vulkan.org/spec/latest/chapters/fundamentals.html#fundamentals-queueoperation)
