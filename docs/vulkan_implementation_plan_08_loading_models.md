.
# Implementation Plan — Chapter 08: Loading Models

> **Based on architecture discussion:** Session 56, 2026-05-19
> **Chapter doc:** `docs/vulkan_chapter_08_loading_models.md`

---

## Architecture Decisions (locked)

| # | Decision |
|---|----------|
| Q1 | `loadModel()` lives in `src/utils/ModelLoader.h/.cpp` as a free function |
| Q2 | Return type is `ModelData { vector<Vertex> vertices; vector<uint32_t> indices; }` defined in `ModelLoader.h` |
| Q3 | `GLM_ENABLE_EXPERIMENTAL` added to `CMakeLists.txt` compile definitions (alongside `GLM_FORCE_RADIANS`) |
| Q4 | `TINYOBJLOADER_IMPLEMENTATION` defined in `ModelLoader.cpp` |
| Q5 | Index type upgraded from `uint16_t` → `uint32_t` throughout |

---

## New Files

| File | Purpose |
|------|---------|
| `src/utils/ModelLoader.h` | `ModelData` struct + `loadModel(path)` declaration |
| `src/utils/ModelLoader.cpp` | `loadModel()` implementation |

---

## Modified Files

| File | Change |
|------|--------|
| `CMakeLists.txt` | Add `GLM_ENABLE_EXPERIMENTAL`, `POST_BUILD` copy for `models/`, add `ModelLoader.cpp` |
| `src/renderer/buffers/Vertex.h` | Add `operator==`, add `std::hash<Vertex>` specialization |
| `src/renderer/buffers/Mesh.h` | Change index vector `uint16_t` → `uint32_t` |
| `src/renderer/buffers/Mesh.cpp` | Change `bindIndexBuffer` to `vk::IndexType::eUint32` |
| `src/Application.cpp` | Remove hardcoded geometry, add path constants, call `loadModel()` |

---

## Build Order

Work through steps in this order to keep the build green at each stage.

### Step 1 — CMakeLists.txt
- Add `GLM_ENABLE_EXPERIMENTAL` to `add_compile_definitions()`
- Add `ModelLoader.cpp` to the source list
- Add `POST_BUILD` copy rule for `models/` → `${CMAKE_BINARY_DIR}/models` (same pattern as `textures/`)

### Step 2 — Assets
- Place `viking_room.obj` in `models/`
- Place `viking_room.png` in `textures/` (replaces or joins `texture.jpg`)

### Step 3 — `Vertex.h`
- Add `operator==` inside the `Vertex` struct:
  ```
  compare pos, color, and texCoord (uv) fields
  ```
- After the struct, add `std::hash<Vertex>` specialization in `namespace std`:
  - Include `#define GLM_ENABLE_EXPERIMENTAL` guard is no longer needed here (CMake handles it)
  - Include `<glm/gtx/hash.hpp>`
  - Hash body: XOR-combine `hash<glm::vec3>(pos)`, `hash<glm::vec3>(color)`, `hash<glm::vec2>(uv)`

### Step 4 — `Mesh.h` + `Mesh.cpp`
- Change `std::vector<uint16_t> indices_` → `std::vector<uint32_t> indices_` in `Mesh.h`
- Change `bindIndexBuffer` second type argument from `eUint16` → `eUint32` in `Mesh.cpp`

### Step 5 — `ModelLoader.h`
- Define `ModelData` struct with `std::vector<Vertex> vertices` and `std::vector<uint32_t> indices`
- Declare `auto loadModel(const std::string& path) -> ModelData`
- Include `Vertex.h`

### Step 6 — `ModelLoader.cpp`
- `#define TINYOBJLOADER_IMPLEMENTATION` before `#include <tiny_obj_loader.h>`
- Implement `loadModel()`:
  - Declare `attrib`, `shapes`, `materials`, `warn`, `err`
  - Call `tinyobj::LoadObj(...)` — throw on failure
  - Build `uniqueVertices` map and iterate shapes → fill `ModelData`
  - Remember the V-flip: `1.0f - attrib.texcoords[2 * index.texcoord_index + 1]`
  - Set `vertex.color = {1.0f, 1.0f, 1.0f}` (OBJ carries no per-vertex color)
  - Return `ModelData`

### Step 7 — `Application.cpp`
- Add file-scope constants:
  ```cpp
  const std::string MODEL_PATH   = "models/viking_room.obj";
  const std::string TEXTURE_PATH = "textures/viking_room.png";
  ```
- In `initVulkan()`, replace hardcoded vertex/index vectors with:
  ```cpp
  auto model_data = loadModel(MODEL_PATH);
  mesh_ = std::make_unique<Mesh>(renderer_->createMesh(model_data.vertices, model_data.indices));
  ```
- Update `createTexture()` call to use `TEXTURE_PATH`
- Add `#include "utils/ModelLoader.h"`

---

## Smoke Tests

| # | Test | Expected |
|---|------|----------|
| T1 | Build clean | No errors or warnings |
| T2 | Viking room renders | Room geometry visible with texture applied |
| T3 | Depth occlusion | Walls correctly occlude geometry behind them |
| T4 | Resize | No crash, no validation errors |
| T5 | Validation layers | Silent throughout |
