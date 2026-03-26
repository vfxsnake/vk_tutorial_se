# Chapter 02 Reference — Build Setup & Development Environment

> **Status:** COMPLETE (Session 3, 2026-03-18). This is reference material only.
> For full context see `docs/vulkan_chapter_00-02_foundations.md` and `docs/session_log.md`.

This project uses **CMake FetchContent** instead of the tutorial's vcpkg approach.
The project targets **Windows** as runtime but is **authored inside WSL2**.

---

## Architecture: WSL2 + Windows build split

| Concern | Where it happens |
|---------|-----------------|
| Writing code, editing files, CMake authoring | WSL2 (Claude Code, VS Code remote, etc.) |
| CMake configure + build + run | Windows — x64 Native Tools Command Prompt |
| Vulkan SDK | Installed on **Windows** via LunarG installer |
| All source files | On the Windows filesystem, accessed from WSL2 via `/mnt/c/...` |

> Keep the project folder on the Windows filesystem (e.g. `C:\DEV\vk_tutorial_se`), not inside the WSL2 virtual disk.

---

## Prerequisites (Windows side — already done)

1. **Vulkan SDK** — installed via LunarG installer. `VULKAN_SDK` env var set automatically.
2. **Visual Studio** — "Desktop development with C++" workload installed.
3. **CMake 4.1.0** — available in x64 Native Tools Command Prompt.
4. **Git 2.50.0** — on PATH on the Windows side.

---

## CMakeLists.txt — FetchContent setup

All dependencies (GLFW, GLM, Vulkan-Hpp, stb, tinyobjloader) are fetched at configure time.

```cmake
cmake_minimum_required(VERSION 3.25)
project(VulkanTutorial LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Vulkan SDK (found via VULKAN_SDK env var set by LunarG installer) ─────────
find_package(Vulkan REQUIRED)

# ── FetchContent dependencies ──────────────────────────────────────────────────
include(FetchContent)

# GLFW — window creation
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM — linear algebra (header-only)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# Vulkan-Hpp — C++ bindings with RAII (header-only, matches installed SDK version)
FetchContent_Declare(
    VulkanHpp
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Hpp.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(VulkanHpp)

# stb — image loading (header-only)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# tinyobjloader — OBJ model loading (header-only)
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        release
)
FetchContent_MakeAvailable(tinyobjloader)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(VulkanTutorial src/main.cpp)

target_include_directories(VulkanTutorial PRIVATE
    ${stb_SOURCE_DIR}
    ${tinyobjloader_SOURCE_DIR}
)

target_link_libraries(VulkanTutorial PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    Vulkan::Headers
)

target_compile_definitions(VulkanTutorial PRIVATE
    VULKAN_HPP_NO_CONSTRUCTORS
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)
```

> Add/remove libraries from `target_link_libraries` as chapters progress. Later chapters need `stb` and `tinyobjloader`.

---

## Build commands (Windows — x64 Native Tools Command Prompt)

```bat
cd C:\DEV\vk_tutorial_se

:: Configure (first time, or after CMakeLists.txt changes)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

:: Build
cmake --build build --config Debug

:: Run
.\build\Debug\VulkanTutorial.exe
```

For Release builds replace `Debug` with `Release` in both `--build` and the exe path.

---

## Accessing the project from WSL2

```bash
/mnt/c/DEV/vk_tutorial_se
```

Edit all source files here. CMake and builds must be triggered from the Windows side.

---

## Shader compilation (Slang)

```bat
%VULKAN_SDK%\Bin\slangc.exe shaders\shader.slang -o shaders\shader.spv
```

---

## VSCode debugger — Windows (MSVC + cppvsdbg)

`.vscode/launch.json`:
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
    },
    {
      "name": "WSL2 — GDB Debug",
      "type": "cppdbg",
      "request": "launch",
      "program": "${command:cmake.launchTargetPath}",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}",
      "externalConsole": false,
      "MIMode": "gdb",
      "miDebuggerPath": "/usr/bin/gdb",
      "setupCommands": [
        { "description": "Enable pretty-printing for GDB", "text": "-enable-pretty-printing", "ignoreFailures": true },
        { "description": "Set disassembly flavor to Intel", "text": "-gdb-set disassembly-flavor intel", "ignoreFailures": true }
      ],
      "preLaunchTask": "CMake: build"
    }
  ]
}
```

`.vscode/settings.json` (Windows):
```json
{
  "cmake.configureArgs": ["-A", "x64"],
  "cmake.buildDirectory": "${workspaceFolder}/build",
  "cmake.buildType": "Debug",
  "cmake.generator": "Visual Studio 17 2022"
}
```

`.vscode/settings.json` (WSL2 remote):
```json
{
  "cmake.buildDirectory": "${workspaceFolder}/build-linux",
  "cmake.buildType": "Debug",
  "cmake.generator": "Unix Makefiles"
}
```

---

## WSL2 build — GCC + Mesa (integrated GPU)

```bash
sudo apt install -y build-essential cmake git gdb libvulkan-dev vulkan-tools \
    vulkan-validationlayers-dev mesa-vulkan-drivers libx11-dev libxrandr-dev \
    libxinerama-dev libxcursor-dev libxi-dev libwayland-dev libxkbcommon-dev pkg-config

cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux -j$(nproc)
./build-linux/VulkanTutorial
```

---

## GPU summary

| Context | GPU | Driver | Primary use |
|---------|-----|--------|-------------|
| Windows build | NVIDIA RTX 2070 | NVIDIA proprietary | Main Vulkan development and rendering |
| WSL2 build | Intel/AMD iGPU | Mesa (open source) | Device enumeration, capability inspection, validation testing |
