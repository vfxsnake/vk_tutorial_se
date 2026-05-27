# Chapter 11: Compute Shader

> **Source:** https://docs.vulkan.org/tutorial/latest/11_Compute_Shader.html
> **Vulkan Version:** 1.4 | **Language:** C++20 | **Shading:** Slang | **Bindings:** Vulkan-Hpp RAII

---

## Overview

Compute shaders unlock GPGPU (General-Purpose GPU computing) from within a Vulkan application. Unlike older APIs where GPU compute was optional, Vulkan mandates compute support across all implementations — from high-end desktops to embedded devices.

This chapter uses a GPU-based particle system as the practical vehicle. Each frame, a compute shader updates thousands of particle positions entirely on the GPU. The graphics pipeline then reads those positions directly as vertex data — no CPU round-trip, no PCIe transfer per frame.

Key new concepts introduced:
- **SSBO** (Shader Storage Buffer Object) — a buffer readable and writable from any shader stage
- **Compute pipeline** — a separate, simpler pipeline with no fixed-function stages
- **Compute work groups and invocations** — how GPU parallelism is structured for compute
- **Compute-graphics synchronization** — preventing the vertex shader from reading particles while the compute shader is still writing them
- **Timeline semaphores** — a more powerful Vulkan 1.2+ synchronization primitive

---

## Why Compute on the GPU?

Three concrete advantages over CPU processing:

1. **CPU offloading** — thousands of particles move per frame; doing this on the CPU wastes cycles that could drive game logic, AI, or audio.
2. **Memory efficiency** — particle data lives in GPU-local memory. No round-trip means no PCIe bandwidth consumed on particle uploads every frame.
3. **Parallelism** — a modern GPU has thousands of compute units. A particle update is embarrassingly parallel: each particle is independent. The GPU is structurally ideal for this.

### Where Compute Sits in the Pipeline

In the Vulkan specification's pipeline block diagram, compute is a completely **detached stage** — it doesn't plug into the vertex → rasterizer → fragment flow at all. Descriptor sets are the only resource type shared across both pipelines. Everything else (command buffers, pipelines, queues) has parallel compute equivalents.

This separation is a feature: you can submit compute work and graphics work independently, potentially in parallel on hardware that supports async compute.

---

## Shader Storage Buffer Objects (SSBO)

### Concepts

A **uniform buffer** is read-only from shaders and is small (spec minimum 16 KB). A **Shader Storage Buffer Object** (SSBO) is read-write from shaders and can be arbitrarily large. For a particle system, SSBOs are the natural choice — the compute shader writes new positions, the vertex shader reads them.

The same `VkBuffer` can carry multiple usage flags simultaneously. The particle buffer in this chapter has three:
- `eStorageBuffer` — so the compute shader can read/write it as an SSBO
- `eVertexBuffer` — so the graphics pipeline can bind it directly as vertex input
- `eTransferDst` — so the initial CPU data can be staged into it

There is no performance penalty for dual-flagging a buffer this way. The GPU sees the same memory region regardless of which pipeline stage is accessing it.

### Per-Frame SSBO Pair — The Double-Buffer Pattern

With `MAX_FRAMES_IN_FLIGHT = 2`, we maintain **two SSBO instances** — one per frame slot. But these two serve a different role from the UBOs we've seen before:

- Frame N's compute shader reads from **frame N-1's SSBO** (last frame's particle positions)
- Frame N's compute shader writes to **frame N's SSBO** (this frame's updated positions)

This is a classic GPU double-buffer: one buffer is the read source, the other is the write destination. The `(i - 1) % MAX_FRAMES_IN_FLIGHT` index expression captures this: frame 0 reads from frame 1 (wrapping around) and frame 1 reads from frame 0.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::DescriptorType::eStorageBuffer` | Enum | Declares a descriptor binding as an SSBO |
| `vk::BufferUsageFlagBits::eStorageBuffer` | Enum | Enables a buffer for SSBO access from shaders |
| `StructuredBuffer<T>` | Slang type | Read-only structured buffer (maps to SSBO binding) |
| `RWStructuredBuffer<T>` | Slang type | Read-write structured buffer (maps to SSBO binding) |
| `vk::BufferUsageFlagBits::eTransferDst` | Enum | Allows staging copy into a device-local buffer |

### Shader Declaration

In Slang, two bindings point at the SSBO pair:
- `StructuredBuffer<ParticleSSBO> particlesIn` at binding 1 — the previous frame's data, read-only
- `RWStructuredBuffer<ParticleSSBO> particlesOut` at binding 2 — this frame's data, write destination

The `RW` prefix on `RWStructuredBuffer` is what grants write access. Without it the shader can only read, which maps to a descriptor type of `eStorageBuffer` in read-only usage — though for our purposes, write access is needed for the output.

### Code Walkthrough

Particle data is laid out the same way on both the C++ side (as a struct) and inside the Slang shader. Both declare the same fields in the same order: a 2D position, a 2D velocity, and an RGBA color. This alignment is essential — the compute shader and the vertex shader both interpret the same raw bytes. If the struct layout differs, particles will fly to wrong positions or the colors will be garbage.

Initial particle positions are randomized on the CPU in a ring pattern — uniform random radius and angle in polar coordinates, then converted to Cartesian. The initial velocity points outward from center, scaled to a small constant. This data is copied into a host-visible staging buffer via `mapMemory` + `memcpy`, then transferred to two device-local SSBOs (one per frame slot) using `copyBuffer`. The staging buffer is destroyed after the loop — its only purpose was the initial upload.

### Common Pitfalls

- **Forgetting `eTransferDst` on the device-local SSBO**: `copyBuffer` requires the destination to have `eTransferDst`. Validation layers will fire with a usage mismatch error.
- **Struct layout mismatch between C++ and shader**: If the particle struct gains padding in C++ (e.g., due to alignment) that the shader doesn't account for, positions and velocities will be read from the wrong offsets. Use `offsetof` assertions or `static_assert(sizeof(Particle) == expected)` to catch this early.
- **Forgetting `eVertexBuffer` on the SSBO**: Without it, binding the buffer as vertex input at draw time will produce a validation error.

---

## Storage Images

### Concepts

Where SSBOs give read/write access to buffer data, **storage images** give read/write access to texture data. Use cases include post-processing effects (read from one image, write to another), mipmap generation, and image-space simulations.

The image needs `eSampled | eStorage` usage flags — `eSampled` to allow reads via a sampler, `eStorage` to allow direct pixel reads/writes from a compute shader.

In Slang, `Texture2D<float>` is read-only (sampled), `RWTexture2D<float>` is read-write (storage). Pixels are accessed with integer coordinates via `[int2(x, y)]` indexing — unlike sampled textures, no UV normalization, no filtering.

Storage images are not used in the particle system example, but they're the tool for any effect that needs to both read and produce image data in the same pass.

---

## Compute Queue Families

### Concepts

Compute commands run on a queue that supports `vk::QueueFlagBits::eCompute`. Vulkan guarantees that any implementation that supports graphics has at least one queue family supporting **both graphics and compute** — so no special selection logic is needed for the basic case.

In your codebase, `VulkanContext` already finds a queue family supporting `eGraphics`. That same family supports `eCompute` by spec guarantee. The single `graphicsQueue_` is reused for both graphics and compute submissions.

Dedicated compute queue families (supporting compute but not graphics) exist on some GPUs and hint at **async compute** capability — compute can run in parallel with graphics on separate hardware units. This chapter doesn't use async compute, but the queue family discovery pattern for it is: find a queue family with `eCompute` but without `eGraphics`.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::QueueFlagBits::eCompute` | Enum | Queue capability flag for compute work |
| `vk::PipelineBindPoint::eCompute` | Enum | Bind point for compute pipelines and descriptor sets |
| `vk::ShaderStageFlagBits::eCompute` | Enum | Shader stage flag for descriptor layout bindings |

---

## Compute Pipelines

### Concepts

A compute pipeline is much simpler than a graphics pipeline. There are no fixed-function stages — no rasterizer, no depth test, no blending. It contains exactly one thing: a compute shader stage.

`vk::ComputePipelineCreateInfo` takes:
- A `vk::PipelineShaderStageCreateInfo` with `eCompute` stage and the shader module
- A `vk::PipelineLayout` (descriptor layouts + push constants, same concept as graphics)

That's it. No vertex input state, no dynamic state, no render attachment formats, no multisampling.

The pipeline layout for the particle system has three bindings: a UBO (binding 0, for `deltaTime`), the previous frame's SSBO (binding 1, read-only), and the current frame's SSBO (binding 2, write destination).

The `stageFlags` on the UBO binding can be `eVertex | eCompute` — both the vertex shader (for a potential MVP transform) and the compute shader need `deltaTime`. A single descriptor set can serve both pipeline bind points simultaneously.

### Descriptor Pool Sizing

The compute descriptor pool needs to accommodate storage buffers. The count is `MAX_FRAMES_IN_FLIGHT * 2` for SSBOs — each frame slot references two SSBOs (current and previous), so two `eStorageBuffer` entries per frame.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::ComputePipelineCreateInfo` | Struct | Describes a compute pipeline — shader stage + layout only |
| `vk::Device::createComputePipeline()` | Function | Creates `vk::raii::Pipeline` for compute use |
| `vk::DescriptorType::eStorageBuffer` | Enum | Pool + layout binding type for SSBOs |

### Common Pitfalls

- **Using `createGraphicsPipelines` instead of `createComputePipeline`**: They are separate functions. The compute version takes `vk::ComputePipelineCreateInfo`, not `vk::GraphicsPipelineCreateInfo`.
- **Forgetting `eFreeDescriptorSet` on the pool**: Same rule as for graphics — `vk::raii::DescriptorSet` destructor always calls `vkFreeDescriptorSets`, requiring the flag.

---

## Compute Space: Work Groups and Invocations

### Concepts

GPU parallelism for compute is structured in two levels:

**Work groups** — the batch unit. You specify how many to launch at dispatch time, with up to three dimensions (X, Y, Z). For a 1D particle array, `dispatch(PARTICLE_COUNT / 256, 1, 1)` launches enough work groups to cover all particles.

**Invocations** — individual shader executions within a work group. The shader specifies how many per group with `[numthreads(X, Y, Z)]`. All invocations in a work group run concurrently and can share local memory (not used here).

The total number of shader executions is:
```
workGroupCount.x × workGroupCount.y × workGroupCount.z
×
numthreads.x × numthreads.y × numthreads.z
```

For the particle system: `(PARTICLE_COUNT / 256) × 256 × 1 × 1 = PARTICLE_COUNT` — exactly one invocation per particle.

`SV_DispatchThreadID` (the `threadId` semantic in Slang) gives each invocation a unique 3D index across the entire dispatch. For a 1D dispatch, `threadId.x` gives the particle index directly.

### Sizing Rules

- `numthreads` must divide evenly into the particle count, or you must bounds-check `index < PARTICLE_COUNT` inside the shader and return early.
- Hardware limits: `maxComputeWorkGroupCount`, `maxComputeWorkGroupInvocations`, `maxComputeWorkGroupSize` — queryable via `VkPhysicalDeviceLimits`.
- A common `numthreads` value is 256 for 1D work — large enough to keep compute units busy, small enough to avoid exceeding limits on low-end hardware.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `[numthreads(X,Y,Z)]` | Slang attribute | Declares invocations per work group |
| `SV_DispatchThreadID` | Slang semantic | Global invocation index across entire dispatch |
| `commandBuffer.dispatch(x, y, z)` | Function | Launches `x × y × z` work groups |

---

## The Compute Shader

### Concepts

The particle update shader does three things per invocation:
1. Reads the particle's current position and velocity from the previous frame's SSBO
2. Integrates: `newPosition = oldPosition + velocity × deltaTime`
3. Bounces velocity at the ±1 boundary (window edges in NDC space)

The `deltaTime` UBO ensures frame-rate-independent movement — particles move the same distance per real second regardless of how fast the GPU runs.

### Code Walkthrough

The shader is structured around the double-buffer pattern: `particlesIn` is the source (previous frame), `particlesOut` is the destination (this frame). The index into both arrays is `threadId.x` — the global invocation ID. Every particle is processed in parallel: thousands of invocations run simultaneously, each handling exactly one particle index.

The velocity integration is a simple Euler step: multiply velocity by `deltaTime` and add to position. The bounce check inverts the relevant velocity component when the position exceeds ±1. This gives the classic billiard-ball reflection behavior.

Note that `particlesOut` gets both the new position AND the existing velocity copied into it. If you only write `position`, the `velocity` field in the output buffer would be uninitialized (left over from whatever was there before). Always explicitly write every field of the output struct.

### Common Pitfalls

- **Off-by-one in work group sizing**: If `PARTICLE_COUNT` is not divisible by `numthreads`, the last work group will have invocations with `index >= PARTICLE_COUNT`. These will read and write out-of-bounds memory. Guard with `if (index >= PARTICLE_COUNT) return;`.
- **Forgetting to copy unchanged fields to output**: Any field not written in the compute shader will have stale or garbage data in the output SSBO.

---

## Synchronization: Compute + Graphics

### Concepts

This is the most critical part of the chapter. Two GPU operations must not overlap:

1. **Write-after-read hazard**: Compute must not update particle positions while the vertex shader is still reading them for the previous draw call.
2. **Read-after-write hazard**: The vertex shader must not start reading particles until the compute shader has finished writing them.

The solution is a pair of synchronization primitives per frame slot:
- A **`computeInFlightFence`** — CPU waits on this before re-recording compute commands, same role as the graphics in-flight fence
- A **`computeFinishedSemaphore`** — GPU-side signal that compute is done; the graphics submission waits on this

### The Submit Flow Per Frame

Frame N follows this sequence:

1. **CPU waits** on `computeInFlightFence[N]` — ensures the previous frame's compute work is done before re-recording
2. **CPU resets** the fence and records the compute command buffer
3. **Compute submit**: signals `computeFinishedSemaphore[N]` and `computeInFlightFence[N]`
4. **CPU waits** on `inFlightFence[N]` (graphics) — as before
5. **Graphics submit**: waits on TWO semaphores:
   - `imageAvailableSemaphore[N]` at `eColorAttachmentOutput` (swap chain image ready)
   - `computeFinishedSemaphore[N]` at **`eVertexInput`** (particles ready for vertex fetch)

The `eVertexInput` wait stage is the key detail. It tells the GPU: "don't start fetching vertex data until `computeFinishedSemaphore` is signalled." This is the exact point in the pipeline where particles are consumed, so waiting there is neither too early (which would over-serialize) nor too late (which would allow the hazard).

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::PipelineStageFlagBits::eVertexInput` | Enum | Pipeline stage where vertex buffers are fetched |
| `vk::PipelineStageFlagBits::eComputeShader` | Enum | Pipeline stage for compute shader execution |
| `computeQueue.submit(submitInfo, fence)` | Function | Submits compute work; compute and graphics submits are separate calls |

### Common Pitfalls

- **Waiting at the wrong stage**: Waiting at `eTopOfPipe` is over-conservative (stalls everything), waiting at `eVertexShader` is too late (vertex *input* fetches happen before vertex *shader* execution). `eVertexInput` is the correct, precise stage.
- **Missing the compute fence**: Without `computeInFlightFence`, the CPU might re-record the compute command buffer while the GPU is still executing it from the previous frame.
- **Submitting in the wrong order**: Compute submit must precede graphics submit in the same frame. The semaphore enforces GPU ordering, but the CPU submissions must be sequenced correctly too.

---

## Timeline Semaphores

### Concepts

Binary semaphores (what we've used so far) have two states: signalled and unsignalled. They are consumed by a wait — once waited on, they reset to unsignalled. This is simple but limiting.

**Timeline semaphores** (core in Vulkan 1.2, available via extension earlier) are 64-bit monotonically increasing counters. Instead of signal/wait on a boolean state, you signal *to a value* and wait *for a value*. The counter never resets.

Advantages for compute-graphics synchronization:
- **Single semaphore replaces the whole set**: One timeline semaphore replaces the pair of `computeFinishedSemaphore` + per-frame binary semaphore management.
- **Host can wait**: `device.waitSemaphores()` lets the CPU block until a timeline value is reached — useful for "wait until frame N is fully rendered before presenting."
- **No implicit reset**: You never accidentally re-signal before a wait completes, because you signal to a new, higher value each time.
- **Present without a binary semaphore**: The graphics submission signals the timeline to `graphicsSignalValue`; the CPU calls `device.waitSemaphores()` for that value before `queuePresentKHR`. No separate render-finished binary semaphore needed.

### The Value Pattern Per Frame

Each frame advances the timeline by 2:
1. Compute signals `currentValue + 1`; graphics waits for that value at `eVertexInput`
2. Graphics signals `currentValue + 2`; CPU waits for that value before presenting

The monotonically increasing values are never reused, so there is no ambiguity about which frame's compute work is being waited on.

### Enabling Timeline Semaphores

`timelineSemaphore` is a feature in `VkPhysicalDeviceVulkan12Features` (or the `VK_KHR_timeline_semaphore` extension struct for older drivers). It must be enabled in `createLogicalDevice` via the features chain — the same pattern used for `dynamicRendering` and `synchronization2`. On a Vulkan 1.4 device with a modern driver, it is guaranteed to be available.

Creation requires chaining `vk::SemaphoreTypeCreateInfo` into the `SemaphoreCreateInfo::pNext` with `semaphoreType = eTimeline` and `initialValue = 0`.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::SemaphoreType::eTimeline` | Enum | Selects timeline mode for semaphore creation |
| `vk::SemaphoreTypeCreateInfo` | Struct | Chained into `SemaphoreCreateInfo.pNext` to set type + initial value |
| `vk::TimelineSemaphoreSubmitInfo` | Struct | Chained into `SubmitInfo.pNext` to attach wait/signal values |
| `vk::Device::waitSemaphores()` | Function | CPU blocks until semaphore reaches specified value |
| `vk::SemaphoreWaitInfo` | Struct | Describes which semaphore and value to wait for |

### Common Pitfalls

- **Forgetting to chain `TimelineSemaphoreSubmitInfo` into `SubmitInfo.pNext`**: If missing, the driver ignores the values and treats it as binary, causing incorrect synchronization with no validation error.
- **Non-monotonic signal values**: Signalling a value ≤ current semaphore value is a validation error. Always use `++timelineValue` in order.
- **Mixing binary and timeline in the same submit**: A `VkSubmitInfo` can contain both binary and timeline semaphores in the same arrays, but each timeline entry must have a corresponding entry in `TimelineSemaphoreSubmitInfo::pWaitSemaphoreValues` / `pSignalSemaphoreValues`. Count must match.

---

## Drawing the Particle System

### Concepts

Because the SSBO also carries `eVertexBuffer` usage, it can be bound directly as vertex input at draw time — no copy, no separate vertex buffer. The graphics pipeline reads the same memory that the compute shader just wrote.

The vertex input layout for the graphics pipeline describes only the fields the vertex shader cares about: `position` (location 0, `eR32G32Sfloat`) and `color` (location 1, `eR32G32B32A32Sfloat`). The `velocity` field exists in the struct but has no vertex attribute description — the vertex shader doesn't receive it. The GPU just skips over it when fetching each vertex, using the stride from the binding description.

The draw call uses `draw(PARTICLE_COUNT, 1, 0, 0)` — no index buffer; particles are rendered as individual points (topology `ePointList`).

### Code Walkthrough

At record time, the SSBO for the current frame index is bound via `bindVertexBuffers`. This is the *output* buffer of the compute pass — the one the compute shader just wrote positions into. The binding is identical to how a regular vertex buffer is bound. The pipeline then interprets those bytes as per-vertex position + color data, using the attribute descriptions from the `Particle` struct.

The vertex shader reads `position` and `color` from the vertex input, applies any transform, and outputs to the fragment shader. No uniforms needed beyond what's already in the descriptor sets.

### Common Pitfalls

- **Binding the wrong frame's SSBO**: The SSBO for frame N contains particles updated by frame N's compute dispatch. Accidentally binding frame N-1's SSBO would render one frame behind — usually invisible at high frame rates but incorrect.
- **Topology mismatch**: Particles as points require `vk::PrimitiveTopology::ePointList`. Using `eTriangleList` with a point-topology SSBO will produce incorrect or invisible geometry.

---

## Summary

- Compute shaders run independently of the graphics pipeline, on the same queue family (eCompute ∩ eGraphics is always available).
- SSBOs are the primary data exchange mechanism between compute and graphics — they can carry both `eStorageBuffer` and `eVertexBuffer` usage flags on the same buffer.
- The compute pipeline is minimal: one shader stage + a pipeline layout. No fixed-function state.
- Work groups × invocations per group = total shader executions. `SV_DispatchThreadID.x` gives the particle index per invocation.
- The double-buffer SSBO pattern (read from frame N-1, write to frame N) prevents in-flight data corruption.
- Compute-graphics synchronization requires a `computeFinishedSemaphore` waited at `eVertexInput` in the graphics submit.
- Timeline semaphores replace the binary semaphore pair with a single monotonic counter, enabling host waiting and cleaner multi-frame tracking.

## Implementation Checklist

- [ ] `Particle` struct defined (position, velocity, color) with matching layout on C++ and shader sides
- [ ] `ComputePipeline` class (or equivalent module) created — compute pipeline + layout
- [ ] SSBO pair created per frame slot, initialized from CPU via staging buffer
- [ ] Compute descriptor set layout — UBO at binding 0, SSBO-in at 1, SSBO-out at 2
- [ ] Compute descriptor sets allocated and written (previous-frame SSBO + current-frame SSBO wired per slot)
- [ ] Compute command buffer recorded with `bindPipeline(eCompute)`, `bindDescriptorSets(eCompute)`, `dispatch()`
- [ ] Compute fence + compute-finished semaphore per frame slot
- [ ] Submit order: compute first, graphics second; graphics waits on compute semaphore at `eVertexInput`
- [ ] Graphics pipeline updated for point topology, vertex input from `Particle` struct (position + color only)
- [ ] SSBO bound as vertex buffer at draw time
- [ ] Slang compute shader with `[numthreads(256,1,1)]`, double-buffer read/write, boundary bounce
- [ ] Validation layers silent (no sync hazards, no usage mismatches)

## Further Reading

- [Vulkan Spec — VkComputePipelineCreateInfo](https://docs.vulkan.org/spec/latest/chapters/pipelines.html#pipelines-compute)
- [Vulkan Spec — Shader Storage Buffer Objects](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html#descriptorsets-storagebuffer)
- [Vulkan Spec — Timeline Semaphores](https://docs.vulkan.org/spec/latest/chapters/synchronization.html#synchronization-semaphores-timeline)
- [Vulkan Spec — vkCmdDispatch](https://docs.vulkan.org/spec/latest/chapters/dispatch.html#vkCmdDispatch)
- [Khronos Vulkan Samples — Compute shader topics (async compute, shared memory, atomics, subgroups)](https://github.com/KhronosGroup/Vulkan-Samples)
