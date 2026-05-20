# Chapter 08: Loading Models

> **Source:** https://docs.vulkan.org/tutorial/latest/08_Loading_models.html
> **Vulkan Version:** 1.4 | **Language:** C++20 | **Shading:** Slang | **Bindings:** Vulkan-Hpp RAII

---

## Overview

This chapter replaces the hardcoded rectangle vertices with real 3D geometry loaded from an OBJ file at runtime. It introduces the `tinyobjloader` library, handles the coordinate system difference between OBJ and Vulkan, upgrades the index type from `uint16_t` to `uint32_t`, and adds vertex deduplication via a hash map to eliminate redundant vertex data.

The chapter is a single tutorial page — relatively short in concept but with several small, interconnected changes that ripple through `Vertex`, `Mesh`, `GraphicsPipeline`, and `Application`.

---

## Library Integration

The tutorial uses **tinyobjloader** — the same single-header pattern as `stb_image`. It is already available in this project via FetchContent.

In one `.cpp` file (the tutorial uses the application file; in our project it belongs in whichever `.cpp` file calls `LoadObj` — likely a future `ModelLoader.cpp` or initially `Application.cpp`):

```cpp
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
```

Include the header wherever `LoadObj` is called.

---

## Sample Asset

The tutorial uses the **Viking room** model by nigelgoh (Sketchfab, CC BY 4.0):
- `models/viking_room.obj`
- `textures/viking_room.png`

Two path constants are added at global or class scope:

```cpp
const std::string MODEL_PATH   = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";
```

The model occupies approximately 1.5 × 1.5 × 1.5 world units. Any OBJ of similar scale can be substituted.

---

## Index Type Change

The hardcoded rectangle used `uint16_t` indices, which supports a maximum of 65,535 unique vertices. The Viking room has ~3,500 deduplicated vertices; a raw load produces ~11,500. Either way it exceeds `uint16_t` for non-trivial meshes, so the index type is upgraded:

```cpp
// Before (Mesh, Application)
std::vector<uint16_t> indices;

// After
std::vector<uint32_t> indices;
```

This change must flow through:
- `Vertex.h` / `Mesh.h` — index container type
- `GraphicsPipeline::record()` / `Mesh::bind()` — the `bindIndexBuffer` call must specify `vk::IndexType::eUint32`

---

## The `loadModel()` Function

The loading function uses `tinyobj::LoadObj` to fill three data containers, then iterates all shapes and their index records to reconstruct `Vertex` and index data:

### Concepts

`tinyobj::attrib_t` holds the flat attribute arrays for the entire file — all positions, normals, texture coordinates, and colors as `float` arrays indexed separately from each other.

`tinyobj::shape_t` represents one named object or group in the OBJ file. A file may contain many shapes; the loop processes all of them into a single flat vertex/index list (one mesh for now).

`tinyobj::index_t` is a per-corner struct holding three independent indices: `vertex_index`, `texcoord_index`, `normal_index`. OBJ does not mandate that positions, UVs, and normals share the same index — they can be mixed. This is fundamentally different from Vulkan's interleaved vertex format, where every attribute for a corner shares one index. The iteration rebuilds Vulkan-compatible vertices from these potentially independent indices.

### Code Walkthrough

```cpp
void loadModel()
{
    tinyobj::attrib_t                attrib;
    std::vector<tinyobj::shape_t>    shapes;
    std::vector<tinyobj::material_t> materials;
    std::string                      warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
    {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex{};

            vertex.pos =
            {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord =
            {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = {1.0f, 1.0f, 1.0f};

            auto [it, inserted] = uniqueVertices.insert({vertex, static_cast<uint32_t>(vertices.size())});
            if (inserted)
            {
                vertices.push_back(vertex);
            }

            indices.push_back(it->second);
        }
    }
}
```

Vertex color defaults to white `{1.0f, 1.0f, 1.0f}` — the OBJ format does not carry per-vertex color data for this model; the texture provides all color information.

---

## Texture Coordinate Flipping

### Concepts

OBJ files follow the OpenGL/standard convention where the V axis of texture coordinates runs **bottom-to-top** (V = 0 at the bottom of the image). Vulkan's image coordinate system runs **top-to-bottom** (V = 0 at the top). If UVs are used without correction, the texture appears vertically mirrored.

The fix is a one-liner at load time, not a shader change:

```cpp
vertex.texCoord =
{
    attrib.texcoords[2 * index.texcoord_index + 0],         // U — unchanged
    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]  // V — flipped
};
```

Subtracting from `1.0f` maps V=0 (OBJ bottom) → V=1 (Vulkan bottom) and V=1 (OBJ top) → V=0 (Vulkan top).

### Common Pitfall
This flip is only correct because the texture image itself is stored top-to-bottom (standard PNG/JPEG convention). If you load a texture that is already stored bottom-to-top (uncommon), the flip is wrong. The easiest diagnostic is visual: if the texture is upside-down, toggle the flip.

---

## Vertex Deduplication

### Concepts

Without deduplication, every index record in the OBJ produces a separate `Vertex` entry in the output array — including corners that are shared between adjacent triangles. The Viking room produces ~11,484 raw vertices but only ~3,566 unique ones (~70% redundancy). The fix is to use a hash map from `Vertex` → `uint32_t` index and only emit a new vertex when it has not been seen before.

This is a CPU-side optimization applied at load time. It reduces vertex buffer size and memory bandwidth on the GPU.

### Requirements

For `Vertex` to be usable as a hash map key, two things must be true:

**1. Equality operator** — two vertices are the same if all their fields match:

```cpp
bool operator==(const Vertex& other) const
{
    return pos == other.pos && color == other.color && texCoord == other.texCoord;
}
```

This lives inside the `Vertex` struct definition.

**2. Hash function** — `std::unordered_map` requires `std::hash<Vertex>`. GLM provides hash implementations for its types under the experimental header. Combine them with XOR-shift:

```cpp
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace std
{
    template<> struct hash<Vertex>
    {
        size_t operator()(Vertex const& vertex) const
        {
            return ((hash<glm::vec3>()(vertex.pos)
                   ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1)
                   ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}
```

The XOR-shift pattern (`^ ... << 1 ... >> 1`) is a common technique to reduce hash collisions when combining multiple hash values. It is not perfect but is standard practice for small structs.

`GLM_ENABLE_EXPERIMENTAL` must be defined before including `<glm/gtx/hash.hpp>`. The `gtx/` prefix means "GLM experimental extension" — these headers are stable in practice but not considered part of the stable API, hence the opt-in define.

### Hash function placement

The `std::hash` specialization must be visible before any `std::unordered_map<Vertex, ...>` instantiation. In practice this means it goes in `Vertex.h`, after the `Vertex` struct definition (still in the `std` namespace).

### Deduplication loop

```cpp
std::unordered_map<Vertex, uint32_t> uniqueVertices{};

for (const auto& shape : shapes)
{
    for (const auto& index : shape.mesh.indices)
    {
        Vertex vertex{ /* ... fill fields ... */ };

        auto [it, inserted] = uniqueVertices.insert(
            {vertex, static_cast<uint32_t>(vertices.size())}
        );

        if (inserted)
            vertices.push_back(vertex);

        indices.push_back(it->second);
    }
}
```

`insert` returns a `{iterator, bool}` pair. The structured binding `auto [it, inserted]` captures both. If `inserted` is true the vertex is new — add it to `vertices` and the map records its index. Either way, `it->second` is the correct index for this corner.

---

## Architecture Note for This Project

In the tutorial, `loadModel()` is a free function or method on the application class that writes into member arrays. In our modular project this needs slightly different thinking:

- `Vertex` is defined in `src/renderer/buffers/Vertex.h`. The `operator==` and `std::hash` specialization live there too.
- The loading function returns `std::vector<Vertex>` and `std::vector<uint32_t>` — it does not touch the GPU. Those vectors are then handed to `Renderer::createMesh()`, which handles the staging upload.
- For now `loadModel()` can live as a private helper in `Application`. It is the natural precursor to the `scene/` layer (Ch08 in the CLAUDE.md notes ECS starts here), but a simple helper function in `Application` is the right start before that abstraction is warranted.
- A `models/` asset folder needs to be created and deployed via CMake `POST_BUILD` the same way `textures/` is.
- The index type change from `uint16_t` → `uint32_t` must be applied in `Mesh.h` (index vector member type) and `Mesh::bind()` (the `bindIndexBuffer` call).

---

## Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `tinyobj::LoadObj` | Function | Parses an OBJ file into attrib, shapes, materials |
| `tinyobj::attrib_t` | Struct | Flat float arrays: all positions, UVs, normals in the file |
| `tinyobj::shape_t` | Struct | One named object; `.mesh.indices` is the per-corner index list |
| `tinyobj::index_t` | Struct | Per-corner indices: `vertex_index`, `texcoord_index`, `normal_index` |
| `std::unordered_map<Vertex, uint32_t>` | Container | Vertex deduplication map |
| `vk::IndexType::eUint32` | Enum | Required in `bindIndexBuffer` once indices exceed 65,535 |
| `glm::gtx/hash.hpp` | Header | Hash implementations for `glm::vec2`, `glm::vec3`, `glm::vec4` |

---

## Common Pitfalls

- **Forgetting the V-flip** — texture appears upside-down on the model.
- **Leaving index type as `uint16_t`** — vertex count exceeds 65,535 and indices wrap silently, producing garbage geometry.
- **Missing `GLM_ENABLE_EXPERIMENTAL`** — `glm/gtx/hash.hpp` include fails with a `#error`.
- **Placing `std::hash<Vertex>` in the wrong translation unit** — the specialization must be visible at the point where `std::unordered_map<Vertex, ...>` is instantiated. Putting it in `Vertex.h` is the safest location.
- **`TINYOBJLOADER_IMPLEMENTATION` in a header** — causes multiple-definition linker errors. Define it in exactly one `.cpp` file.
- **Model path relative to wrong directory** — the `models/` folder must exist relative to the working directory at runtime (same rule as `textures/` and `shaders/` in our CMake setup).

---

## Summary

- Replaced hardcoded vertex/index arrays with OBJ file loading via `tinyobjloader`
- Upgraded index type from `uint16_t` to `uint32_t` for models with >65,535 vertices
- Flipped the V texture coordinate to reconcile OBJ (bottom-up) vs Vulkan (top-down) conventions
- Added `operator==` and `std::hash<Vertex>` to enable vertex deduplication
- Reduced Viking room vertex count ~70% through deduplication
- Application now renders an externally authored 3D model with full texturing and depth

## Implementation Checklist

- [ ] Add `viking_room.obj` and `viking_room.png` to the project assets
- [ ] Create `models/` folder and add CMake `POST_BUILD` copy rule for it
- [ ] Change index type to `uint32_t` in `Mesh.h`, `Application`, and `Mesh::bind()`
- [ ] Add `operator==` to `Vertex` struct in `Vertex.h`
- [ ] Add `std::hash<Vertex>` specialization in `Vertex.h` (with `GLM_ENABLE_EXPERIMENTAL`)
- [ ] Add `TINYOBJLOADER_IMPLEMENTATION` define to the appropriate `.cpp`
- [ ] Implement `loadModel()` (or equivalent helper) returning `vector<Vertex>` + `vector<uint32_t>`
- [ ] Wire `loadModel()` into `Application::initVulkan()` replacing hardcoded data
- [ ] Update `TEXTURE_PATH` constant to point to `viking_room.png`
- [ ] Verify: model renders correctly, texture right-side up, depth occlusion correct

## Further Reading

- [tinyobjloader GitHub](https://github.com/tinyobjloader/tinyobjloader)
- [OBJ file format reference](https://en.wikipedia.org/wiki/Wavefront_.obj_file)
- [Vulkan Spec — vkCmdBindIndexBuffer](https://docs.vulkan.org/spec/latest/chapters/drawing.html#vkCmdBindIndexBuffer)
- [GLM gtx/hash documentation](https://glm.g-truc.net/0.9.9/api/a00152.html)
