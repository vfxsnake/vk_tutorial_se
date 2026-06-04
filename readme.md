# Vulkan Tutorial Study Project

A structured, chapter-by-chapter implementation of the [Khronos Vulkan Tutorial](https://docs.vulkan.org/tutorial/latest/) using **Vulkan 1.4** as the baseline API. The project is not a straight copy of the tutorial code — every chapter is first studied through deep reading and comprehension questions, then implemented from scratch in a modular C++ architecture designed to grow into a small real-time renderer.

The end goal is to complete all main tutorial chapters, then continue into the "Building a Simple Engine" bonus section: resource manager, multithreading, glTF loading, ray tracing.

## What this project practices

- **Modern Vulkan idioms** — dynamic rendering (no legacy render passes), synchronization2 barriers, Vulkan 1.4 features throughout. No compatibility shims.
- **Vulkan-Hpp RAII** — all Vulkan handles owned via `vk::raii::*`. Destruction order is enforced by C++ member declaration order, not manual cleanup functions.
- **Slang shaders** — compiled to SPIR-V at build time via `slangc`. Vertex, fragment, and compute entry points in the same `.slang` file.
- **Composition over inheritance** — each subsystem (`VulkanContext`, `SwapChain`, `GraphicsPipeline`, `Renderer`) is a self-contained class with explicit dependencies passed by `const` reference. No God objects, no inheritance hierarchies.
- **GPU/CPU separation** — CPU-side geometry lives in `scene/` (future ECS), GPU-side resources live in `renderer/buffers/` and `renderer/image_resources/`. The boundary is explicit.
- **Compute + graphics synchronisation** — binary semaphores coordinate a compute dispatch (particle simulation) with a two-pass graphics frame (viking room pass 1, additive particle pass 2). Timeline semaphores planned for the engine phase.
- **MSAA + mipmaps** — multi-sample colour image resolved into the swap chain image each frame; mipmaps generated on the GPU via blit chain.

## Tech Stack

- C++20, Vulkan 1.4, Vulkan-Hpp RAII (`vk::raii::*`)
- GLFW 3.4, GLM, Slang shaders, stb_image, tinyobjloader
- MSVC x64 (Windows, primary runtime), GCC/Ninja (WSL2, IntelliSense)
- CMake FetchContent for all dependencies

## Architecture

```
src/
├── main.cpp
├── Application.h/.cpp          — window lifecycle, object ownership, main loop
├── core/
│   ├── VulkanContext.h/.cpp    — instance, surface, physical/logical device, queues
│   └── SwapChain.h/.cpp        — swap chain, image views, recreate
├── renderer/
│   ├── Renderer.h/.cpp         — frame orchestration: compute + graphics passes
│   ├── GraphicsPipeline.h/.cpp — viking room pipeline (pass 1)
│   ├── RenderFrameSlot.h       — per-frame graphics resources (CB, semaphores, UBO, descriptors)
│   ├── ComputeFrameSlot.h      — per-frame compute resources (CB, fence, SSBO, semaphore)
│   ├── buffers/                — Vertex, Mesh, Particle (GPU buffer resources)
│   ├── compute/                — ComputePipeline, ParticleGraphicsPipeline, ParticleDescriptorLayout
│   ├── descriptors/            — FrameDescriptorLayout, TextureDescriptorLayout
│   └── image_resources/        — Texture, DepthImage, MsaaColorImage
├── scene/                      — ECS placeholder (populates from Ch16 onward)
└── utils/                      — FileUtils, ModelLoader
```

## Progress

| Chapter | Topic | Status |
|---------|-------|--------|
| 00–02 | Foundations & build setup | Complete |
| 03 | Drawing a triangle | Complete |
| 04 | Vertex buffers | Complete |
| 05 | Uniform buffers | Complete |
| 06 | Texture mapping | Complete |
| 07 | Depth buffering | Complete |
| 08 | Loading models (OBJ) | Complete |
| 09–10 | Mipmaps + MSAA | Complete |
| 11 | Compute shaders (particle system) | Complete |

## Build (Windows)

```bat
rmdir /s /q build

cmake -S . -B build -G "Visual Studio 17 2022" -A x64

cmake --build build --config Debug
cd build
Debug\VulkanTutorial.exe
```