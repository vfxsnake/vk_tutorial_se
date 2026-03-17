# Chapter 00–02: Foundations — Introduction, Overview & Development Environment

> **Sources:**
> - https://docs.vulkan.org/tutorial/latest/00_Introduction.html
> - https://docs.vulkan.org/tutorial/latest/01_Overview.html
> - https://docs.vulkan.org/tutorial/latest/02_Development_environment.html
> - https://howtovulkan.com/
>
> **Vulkan Version:** 1.4 | **Language:** C++20 | **Shading:** Slang | **Bindings:** Vulkan-Hpp RAII
>
> **Combined goal:** By the end of this chapter you have a verified, working build environment — a blank window opens, Vulkan initialises, and extensions are enumerated — ready to begin chapter 03 (Drawing a Triangle) without any interruption.

---

## Overview

These three chapters contain no rendering code. Their purpose is to answer three foundational questions before a single GPU command is issued:

1. **Why Vulkan?** — What problem it solves and what mental contract it makes with the developer.
2. **How does Vulkan work at a high level?** — The sequence of objects and steps every Vulkan application must traverse.
3. **Is the toolchain ready?** — Compiler, SDK, libraries, and shaders all building and linking correctly on your specific environment.

Combining them into a single study unit means you complete the environment setup and the mental model together, which is the correct order: you should know *why* each dependency exists before you install it.

---

## Part 1 — Introduction (Chapter 00)

### Concepts

#### What Vulkan is and why it exists

Vulkan is a low-level graphics and compute API developed by the Khronos Group. It was designed from scratch to match the architecture of *modern* GPUs rather than being a layer over fixed-function hardware abstractions from the 1990s.

The APIs it replaced — primarily OpenGL and older versions of Direct3D — were designed at a time when GPUs had fixed pipelines and small amounts of state. Drivers for those APIs grew into enormous, opaque translation layers that tried to guess the programmer's intent, perform implicit synchronisation, and manage resources automatically. The result was unpredictable performance, driver bugs, and no portable way to reason about what the GPU was actually doing.

Vulkan's answer is to make everything explicit:
- You manage memory yourself.
- You describe synchronisation yourself.
- You create pipeline state objects up front, not incrementally.
- You record commands into buffers and submit them yourself.

This verbosity is not accidental. It is the feature. Explicit control means the driver does almost no work at runtime — what you describe is what executes.

#### The tradeoff

The tutorial states it plainly: *"Vulkan isn't meant to be easy."* Drawing a triangle requires roughly ten times more code than in OpenGL. That investment pays off when you need deterministic performance, multi-threading, or portability across GPU vendors and platforms — areas where older APIs fall apart.

The modern additions covered in this tutorial (Vulkan 1.4, dynamic rendering, timeline semaphores, Slang shaders, Vulkan-Hpp RAII) do reduce boilerplate significantly compared to early Vulkan 1.0 patterns. But the fundamental contract — you describe intent explicitly, the driver executes faithfully — does not change.

#### What this tutorial covers

The tutorial progresses through:
- Setting up a working environment and verifying it
- Drawing a triangle (the full Vulkan bootstrap sequence)
- Vertex and index buffers, staging buffers, memory management
- Uniform buffers, descriptor sets, and shader resources
- Texture mapping, image layout transitions, samplers
- Depth buffering, model loading, mipmaps, multisampling
- Compute shaders
- Ray tracing
- Building a simple engine (separate project, after the main chapters)

#### Prerequisites

You need:
- A Vulkan-compatible GPU and drivers (NVIDIA, AMD, Intel on Windows/Linux; Apple Silicon via MoltenVK)
- Solid C++ experience, including RAII, move semantics, and initialiser lists
- A compiler supporting C++20 (Visual Studio 2017+, GCC 7+, Clang 5+)
- Basic familiarity with real-time 3D graphics concepts (matrices, rasterisation, z-buffering)

You do **not** need prior Vulkan, OpenGL, or Direct3D experience.

#### Modern practices used in this project

This project follows the **2026 revision** of the tutorial. Key differences from older Vulkan 1.0 tutorials:

| Old practice | Modern replacement used here |
|---|---|
| GLSL shaders | **Slang** — type-safe, module-aware shading language |
| C-style Vulkan API | **Vulkan-Hpp RAII** (`vk::raii::*`) — automatic lifetime management |
| Render passes + framebuffers | **Dynamic rendering** (`vk::beginRendering`) — no predefined render pass objects |
| Timeline-unaware semaphores | **Timeline semaphores** — explicit multi-frame synchronisation |
| Vulkan 1.0/1.2 | **Vulkan 1.4** baseline — all 1.3 core features available unconditionally |

> **howtovulkan.com note:** The "How to Vulkan in 2026" guide further emphasises **Buffer Device Address** (pointer-based buffer access) and **Descriptor Indexing** (bindless textures) as key modern patterns. These appear in later chapters but are worth keeping in mind from the start as the direction the API is heading.

---

## Part 2 — Overview (Chapter 01)

### Concepts

#### The rendering loop at a glance

Every Vulkan frame follows the same high-level sequence. Understanding this sequence before touching any API is essential — each object you create in chapters 03 onward exists to service one step of this loop.

```
Instance
  └─ Physical Device (GPU selection)
       └─ Logical Device + Queues
            ├─ Surface (window integration)
            │    └─ Swap Chain (render target management)
            │         └─ Image Views
            └─ Graphics Pipeline
                  └─ Command Pool → Command Buffers
                        └─ Submit to Queue → Present to Surface
```

#### Step 1 — Instance (`vk::raii::Instance`)

The instance is the connection between your application and the Vulkan runtime. Creating it involves specifying:
- Your application name and version (used by drivers for profiling)
- Which Vulkan API version you require
- Which **instance-level extensions** to enable (e.g. surface, debug utils)
- Which **validation layers** to enable during development

Nothing GPU-specific happens here. The instance is about Vulkan itself, not about any particular device.

#### Step 2 — Physical Device (`vk::raii::PhysicalDevice`)

After creating an instance you enumerate all available Vulkan-capable GPUs and select one. Selection criteria depend on your needs — you might check:
- Supported Vulkan API version
- Available device extensions (e.g. swap chain support)
- Queue family capabilities (graphics, compute, transfer, presentation)
- Memory heap sizes and types
- Device features (e.g. geometry shaders, sampler anisotropy)

The physical device is a *handle to hardware* — you query it but do not interact with it directly for rendering.

#### Step 3 — Logical Device & Queues (`vk::raii::Device`, `vk::raii::Queue`)

The logical device is your application's view of the GPU. When creating it you specify:
- Which physical device features to enable
- Which queue families to create queues from
- Which device-level extensions to enable

**Queues** are the actual channels through which work is submitted to the GPU. Vulkan separates queue families by capability type:
- **Graphics queues** — rasterisation, drawing commands
- **Compute queues** — general-purpose GPU compute
- **Transfer queues** — memory copy operations
- **Presentation queues** — displaying rendered images (may be same as graphics queue)

Operations submitted to different queues can run concurrently on the GPU, but you are responsible for synchronising them.

#### Step 4 — Window Surface (`vk::raii::SurfaceKHR`)

Vulkan has no built-in concept of a window or display. The `KHR_surface` extension provides a platform-agnostic surface object that represents a window's drawable area. GLFW handles the platform-specific creation of this surface from its window handle.

The surface is needed before selecting a physical device in practice — you must verify that the chosen device can actually present images to the surface you intend to use.

#### Step 5 — Swap Chain (`vk::raii::SwapchainKHR`)

The swap chain is a queue of images that the GPU renders into and the display reads from. Its core purpose is to decouple rendering from presentation so that a partially-rendered frame never appears on screen (tearing).

Key properties you configure:
- **Format** — pixel format of the images (e.g. `VK_FORMAT_B8G8R8A8_SRGB`)
- **Colour space** — e.g. `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`
- **Present mode** — controls vsync behaviour:
  - `FIFO` — vsync on, queue of frames (guaranteed available)
  - `MAILBOX` — vsync on, newest frame replaces queued frame (triple buffering)
  - `IMMEDIATE` — no vsync, may tear
- **Extent** — resolution of the swap chain images (usually matches window size)
- **Image count** — minimum number of images in the chain (double or triple buffering)

#### Step 6 — Image Views (`vk::raii::ImageView`)

Raw swap chain images cannot be used directly in rendering. An `ImageView` describes how to interpret an image's data — which mip levels, array layers, and format interpretation to use. You create one image view per swap chain image.

#### Step 7 — Graphics Pipeline (`vk::raii::Pipeline`)

The pipeline object bakes in virtually all GPU state needed for a draw call:
- Shader stages (vertex, fragment, geometry, etc.) from compiled SPIR-V/Slang bytecode
- Vertex input format and binding descriptions
- Primitive topology (triangles, lines, points)
- Viewport and scissor rectangles
- Rasteriser state (fill mode, cull mode, winding order)
- Multisampling configuration
- Depth and stencil test configuration
- Colour blending mode per attachment
- Which descriptor set layouts and push constants the shaders use

The key insight: in Vulkan, you cannot change most of this state at draw time. If you need different state (e.g. wireframe vs. solid), you create a separate pipeline object. This up-front cost is what enables drivers to compile optimised machine code for the GPU without guessing at runtime.

#### Step 8 — Command Buffers (`vk::raii::CommandBuffer`)

Commands are not sent to the GPU one at a time. They are *recorded* into command buffers, then *submitted* in bulk. This allows:
- Recording on multiple CPU threads in parallel
- Reuse of pre-recorded command buffers
- Efficient GPU scheduling

With dynamic rendering (Vulkan 1.3+), you no longer need a separate render pass object. You specify colour and depth attachments directly when calling `beginRendering`, which simplifies the recording sequence considerably.

Command buffers are allocated from a **command pool** tied to a specific queue family.

#### Step 9 — The Main Loop: Acquire → Record → Submit → Present

Each frame follows this sequence:
1. **Acquire** an available image from the swap chain (signal a semaphore when ready)
2. **Record** draw commands into a command buffer referencing that image
3. **Submit** the command buffer to the graphics queue (wait on the acquire semaphore, signal a render-complete semaphore)
4. **Present** the image to the surface (wait on the render-complete semaphore)

Semaphores coordinate GPU-to-GPU synchronisation. Fences coordinate GPU-to-CPU synchronisation (letting the CPU know when the GPU has finished so you can reuse resources).

#### Validation Layers

Validation layers are optional components inserted between your application and the Vulkan driver. During development they:
- Validate every API call against the specification
- Check for resource lifetime errors
- Report synchronisation hazards
- Catch incorrect struct field combinations

They have near-zero cost in release builds because they are simply not loaded. The canonical layer is `VK_LAYER_KHRONOS_validation`. **Always run with validation layers enabled during development.** They will catch almost every mistake before it becomes a GPU crash or silent corruption.

#### API usage pattern

All Vulkan object creation follows the same shape:
1. Populate a `CreateInfo` struct with all parameters
2. Call the creation function (or constructor in Vulkan-Hpp RAII)
3. The resulting handle is valid until explicitly destroyed

With Vulkan-Hpp RAII (`vk::raii::*`), step 3 produces an object whose destructor calls the correct Vulkan cleanup function automatically. Destruction order still matters — child objects must be destroyed before parent objects — but the RAII wrappers enforce this when objects go out of scope in the correct order.

### Key API Types

| Type | Kind | Purpose |
|---|---|---|
| `vk::raii::Instance` | Object | Entry point to the Vulkan runtime |
| `vk::raii::PhysicalDevice` | Handle | Represents a GPU; queried, not created |
| `vk::raii::Device` | Object | Logical device; your interface for all GPU operations |
| `vk::raii::Queue` | Handle | Channel for submitting commands to the GPU |
| `vk::raii::SurfaceKHR` | Object | Platform-agnostic window drawable area |
| `vk::raii::SwapchainKHR` | Object | Manages the queue of renderable images |
| `vk::raii::ImageView` | Object | Interpretation descriptor for a `vk::Image` |
| `vk::raii::Pipeline` | Object | Compiled GPU state machine for a draw call |
| `vk::raii::CommandPool` | Object | Allocator for command buffers |
| `vk::raii::CommandBuffer` | Object | Recorded sequence of GPU commands |
| `vk::raii::Semaphore` | Object | GPU-to-GPU synchronisation primitive |
| `vk::raii::Fence` | Object | GPU-to-CPU synchronisation primitive |

---

## Part 3 — Development Environment (Chapter 02)

> **Project-specific override:** This project uses **CMake FetchContent** instead of vcpkg. Code is **authored in WSL2** and **built on Windows** using MSVC. All instructions below reflect this setup. The tutorial's original chapter 02 instructions do not apply here.

### Architecture

| Concern | Where it happens |
|---|---|
| Writing code, editing files, CMake authoring | WSL2 (Claude Code, VS Code remote) |
| CMake configure + build + run | Windows — x64 Native Tools Command Prompt |
| Vulkan SDK | Installed on Windows via LunarG installer |
| All source files | Windows filesystem, accessed from WSL2 via `/mnt/c/...` |

The project root lives on the Windows filesystem (`C:\DEV\vk_tutorial_se`). This is critical — keeping it on the Windows side avoids filesystem performance issues and ensures MSVC can access all files natively.

### Prerequisites (Windows side — do once)

**1. Vulkan SDK**
Download from https://vulkan.lunarg.com/sdk/home. Accept the default install path. The installer sets `VULKAN_SDK` as a system environment variable automatically.

Verify: run `vkcube.exe` from `%VULKAN_SDK%\Bin`. A rotating cube should appear.

The SDK provides:
- Vulkan headers and loader (`vulkan.h`, `vulkan_raii.hpp`)
- Validation layers (`VK_LAYER_KHRONOS_validation`)
- Slang shader compiler (`slangc.exe`)
- Debugging tools (`RenderDoc` integration, GPU-assisted validation)

**2. Visual Studio**
Ensure the **"Desktop development with C++"** workload is installed. This provides MSVC, the linker, and the x64 Native Tools Command Prompt.

**3. CMake**
Use the CMake bundled with Visual Studio, or install standalone from https://cmake.org.
Verify: `cmake --version` in the x64 Native Tools prompt.

**4. Git**
Must be on PATH on the Windows side — CMake FetchContent uses it to clone dependencies.
Verify: `git --version`.

### Project Structure

```
C:\DEV\vk_tutorial_se\              ← Windows path
│                                    (edit from WSL2 at /mnt/c/DEV/vk_tutorial_se)
├── CLAUDE.md
├── CMakeLists.txt
├── docs/                           ← study documents (this file lives here)
├── src/
│   └── main.cpp                   ← smoke test for chapter 00-02
├── shaders/
│   └── (empty until chapter 03)
└── build/                          ← generated by CMake, not committed
```

### CMakeLists.txt

The `CMakeLists.txt` uses FetchContent to pull all dependencies at configure time. No manual library installation needed beyond the Vulkan SDK.

Key points about the setup:
- `find_package(Vulkan REQUIRED)` locates the SDK via the `VULKAN_SDK` environment variable
- GLFW handles window creation and provides the platform surface for Vulkan
- GLM provides matrix and vector math that matches GLSL/Slang conventions
- Vulkan-Hpp provides the C++ RAII wrappers (`vk::raii::*`) over the raw C API
- stb provides single-header image loading (used from chapter 06 onward)
- tinyobjloader provides OBJ model loading (used from chapter 08 onward)

The two critical compile definitions:
- `VULKAN_HPP_NO_CONSTRUCTORS` — forces use of designated initialisers (`VkFoo{ .field = value }`) instead of positional constructors, making struct initialisation readable and explicit
- `GLM_FORCE_DEPTH_ZERO_TO_ONE` — makes GLM's projection matrices output depth in Vulkan's `[0, 1]` range instead of OpenGL's `[-1, 1]` range

### Build Instructions (Windows — x64 Native Tools Command Prompt)

```bat
:: Navigate to project root
cd C:\DEV\vk_tutorial_se

:: Configure (first time, or after CMakeLists.txt changes)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

:: Build
cmake --build build --config Debug

:: Run
.\build\Debug\VulkanTutorial.exe
```

For Release builds replace `Debug` with `Release` in both the `--build` command and the exe path.

If you have a different Visual Studio version:
- VS 2019 → `"Visual Studio 16 2019"`
- VS 2022 → `"Visual Studio 17 2022"`
- Check available generators: `cmake --help`

### Shader Compilation (Slang)

`slangc.exe` ships with the Vulkan SDK:

```bat
%VULKAN_SDK%\Bin\slangc.exe shaders\shader.slang -o shaders\shader.spv
```

Automatic compilation at build time via `add_custom_command` will be added to `CMakeLists.txt` in chapter 03 when shaders are first introduced.

### VSCode Debugger Setup

**Required extensions (Windows side):**
- `ms-vscode.cpptools` — provides cppvsdbg (Visual Studio debugger engine)
- `ms-vscode.cmake-tools` — provides `cmake.launchTargetPath`

Create `.vscode/launch.json`:
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Windows — MSVC Debug",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${command:cmake.launchTargetPath}",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "environment": [
        { "name": "PATH", "value": "${env:PATH};${env:VULKAN_SDK}\\Bin" }
      ],
      "console": "integratedTerminal",
      "preLaunchTask": "CMake: build"
    }
  ]
}
```

Create `.vscode/settings.json`:
```json
{
  "cmake.configureArgs": ["-A", "x64"],
  "cmake.buildDirectory": "${workspaceFolder}/build",
  "cmake.buildType": "Debug",
  "cmake.generator": "Visual Studio 17 2022"
}
```

> Open VSCode from the x64 Native Tools Command Prompt (`code .`) so that MSVC environment variables are inherited correctly.

### WSL2 Build (optional — Mesa iGPU)

A secondary build target for WSL2 lets you verify Vulkan device enumeration on the Intel/AMD integrated GPU exposed by Mesa. This is useful for understanding how device capabilities differ between GPU vendors — a concept central to chapter 03.

WSL2 prerequisites (run once inside WSL2):
```bash
sudo apt update && sudo apt install -y \
    build-essential cmake git gdb \
    libvulkan-dev vulkan-tools vulkan-validationlayers-dev \
    mesa-vulkan-drivers \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    libwayland-dev libxkbcommon-dev pkg-config
```

Verify Mesa Vulkan is available:
```bash
vulkaninfo --summary
```

Build:
```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux -j$(nproc)
./build-linux/VulkanTutorial
```

| Context | GPU | Driver | Use |
|---|---|---|---|
| Windows build | NVIDIA RTX 2070 | NVIDIA proprietary | Main Vulkan development |
| WSL2 build | Intel/AMD iGPU | Mesa (open source) | Device enumeration, capability comparison |

### Common Pitfalls

**`find_package(Vulkan)` fails**
The `VULKAN_SDK` environment variable is not set. On Windows this is set by the LunarG installer — open a *new* command prompt after installing the SDK. Verify with `echo %VULKAN_SDK%`.

**FetchContent clone fails**
Git is not on PATH in the build environment. Run `git --version` in the x64 Native Tools prompt. If missing, add Git to the Windows PATH via the Git installer settings.

**`VULKAN_HPP_NO_CONSTRUCTORS` breaks existing code**
This is intentional. All Vulkan struct creation must use designated initialiser syntax. Example: `vk::ApplicationInfo{ .apiVersion = VK_API_VERSION_1_4 }`.

**Validation layers not found**
The `VK_LAYER_KHRONOS_validation` layer ships with the Vulkan SDK. If it isn't found, ensure the SDK is installed and `VULKAN_SDK` is set. Check available layers: call `vk::raii::Context{}.enumerateInstanceLayerProperties()`.

---

## Summary

- Vulkan is an explicit, low-level GPU API designed for modern hardware. Verbosity is intentional — it gives you control the driver cannot take away.
- Every Vulkan application follows the same bootstrap sequence: Instance → Physical Device → Logical Device → Surface → Swap Chain → Image Views → Pipeline → Command Buffers → Submit → Present.
- Dynamic rendering (Vulkan 1.3+, used throughout this project) eliminates render pass and framebuffer objects from that sequence.
- Validation layers catch API misuse at development time with near-zero release overhead. Always enable them.
- This project's environment: code in WSL2, build on Windows with MSVC, run on NVIDIA RTX 2070.
- Dependencies are managed via CMake FetchContent — no vcpkg, no manual downloads beyond the Vulkan SDK.

## Implementation Checklist

- [ ] Vulkan SDK installed and `VULKAN_SDK` environment variable confirmed
- [ ] `vkcube.exe` runs successfully
- [ ] Visual Studio with C++ workload installed
- [ ] CMake available in x64 Native Tools Command Prompt
- [ ] Git available in x64 Native Tools Command Prompt
- [ ] Project folder created at `C:\DEV\vk_tutorial_se`
- [ ] `CMakeLists.txt` written with FetchContent for GLFW, GLM, Vulkan-Hpp, stb, tinyobjloader
- [ ] `src/main.cpp` written: opens a GLFW window, creates a Vulkan instance, enumerates and prints extensions
- [ ] `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` completes without errors
- [ ] `cmake --build build --config Debug` completes without errors
- [ ] `.\build\Debug\VulkanTutorial.exe` opens a blank window and prints extension count to stdout
- [ ] Validation layers confirmed present (enumerate and print layer names)
- [ ] (Optional) WSL2 Mesa build compiles and `vulkaninfo --summary` shows a device

## Further Reading

- [Vulkan 1.4 Specification](https://docs.vulkan.org/spec/latest/)
- [Vulkan-Hpp README](https://github.com/KhronosGroup/Vulkan-Hpp)
- [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
- [Slang Shader Language](https://shader-slang.com/)
- [howtovulkan.com — Modern Vulkan 2026](https://howtovulkan.com/)
- [GLFW Vulkan Guide](https://www.glfw.org/docs/latest/vulkan_guide.html)
