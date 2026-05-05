# Implementation Plan — Chapter 06: Texture Mapping

> **Ground truth for all implementation sessions.**
> All code review and verification tests refer to this plan.

---

## Architecture Decisions

| Decision | Choice |
|----------|--------|
| Texture class location | `src/renderer/textures/Texture.h/.cpp` |
| Texture owns | `vk::raii::Image`, `vk::raii::DeviceMemory`, `vk::raii::ImageView`, `vk::raii::Sampler` |
| GPU helper location | Private methods on `Renderer` (consistent with `createBuffer`/`copyBuffer`) |
| Texture factory | `Renderer::createTexture(path)` → `Texture` (same pattern as `createMesh()`) |
| New descriptor layout class | `TextureDescriptorLayout` in `src/renderer/descriptors/` |
| Descriptor set structure | Set 0 = UBO (`FrameDescriptorLayout`), Set 1 = texture (`TextureDescriptorLayout`) |
| Texture pool + set ownership | `Renderer` owns the texture descriptor pool and one descriptor set |
| Wiring function | `Renderer::bindTextureToDescriptor(const Texture&)` — writes `DescriptorImageInfo` into set 1 |
| `createImageView` helper | Private method on `Renderer`; `SwapChain` keeps its own view creation unchanged |
| Vertex format change | In-place edit of `Vertex.h`; `GraphicsPipeline.cpp` line 64 updated from `array<...,2>` to `array<...,3>` |

---

## Folder Structure After Chapter 06

```
src/
├── main.cpp
├── Application.h/.cpp
├── core/
│   ├── VulkanContext.h/.cpp       ← samplerAnisotropy enabled
│   └── SwapChain.h/.cpp
├── renderer/
│   ├── Renderer.h/.cpp            ← new helpers + createTexture + bindTextureToDescriptor + texture pool/set
│   ├── GraphicsPipeline.h/.cpp    ← two-set pipeline layout; record() gains texture descriptor set param
│   ├── RenderFrameSlot.h
│   ├── buffers/
│   │   ├── Vertex.h               ← gains glm::vec2 texCoord; getAttributeDescriptions() → array<3>
│   │   ├── Mesh.h/.cpp
│   │   └── UniformBufferObject.h
│   ├── descriptors/
│   │   ├── FrameDescriptorLayout.h/.cpp
│   │   └── TextureDescriptorLayout.h/.cpp   ← NEW
│   └── textures/
│       └── Texture.h/.cpp         ← NEW
├── scene/
└── utils/
    └── FileUtils.h
```

---

## New Files

### `src/renderer/textures/Texture.h`

```
Class: Texture
Constructor: Texture(vk::raii::Image, vk::raii::DeviceMemory, vk::raii::ImageView, vk::raii::Sampler)
Copy: deleted
Move: defaulted
Accessors:
  getImageView() const  → const vk::raii::ImageView&
  getSampler()   const  → const vk::raii::Sampler&
Members (private):
  image_          vk::raii::Image
  memory_         vk::raii::DeviceMemory
  imageView_      vk::raii::ImageView
  sampler_        vk::raii::Sampler
```

No descriptor knowledge. Pure GPU resource container.

---

### `src/renderer/descriptors/TextureDescriptorLayout.h/.cpp`

```
Class: TextureDescriptorLayout
Constructor: TextureDescriptorLayout(const VulkanContext&)
Copy: deleted
Accessor: getLayout() const → const vk::raii::DescriptorSetLayout&
Member (private): layout_  vk::raii::DescriptorSetLayout

Layout binding:
  binding = 0
  descriptorType = eCombinedImageSampler
  descriptorCount = 1
  stageFlags = eFragment
  pImmutableSamplers = nullptr
```

---

## Modified Files

### `src/core/VulkanContext.h/.cpp`

- `isDeviceSuitable()` — add check: `physicalDevice.getFeatures().samplerAnisotropy == VK_TRUE`
- `createLogicalDevice()` — add `samplerAnisotropy = vk::True` to `vk::PhysicalDeviceFeatures` in the feature chain

---

### `src/renderer/Renderer.h/.cpp`

**New private helpers (declared in `.h`, implemented in `.cpp`):**

```
beginSingleTimeCommands() → vk::raii::CommandBuffer
  Allocates a command buffer from commandPool_ with eOneTimeSubmit flag, calls begin()

endSingleTimeCommands(vk::raii::CommandBuffer command_buffer) → void
  Calls end(), submits to context_.getQueue(), calls queue.waitIdle()

[refactor] copyBuffer() — rewritten to use beginSingleTimeCommands / endSingleTimeCommands
  (same behaviour, shorter body)

createImage(uint32_t width, uint32_t height, vk::Format format,
            vk::ImageTiling tiling, vk::ImageUsageFlags usage,
            vk::MemoryPropertyFlags mem_props)
  → std::pair<vk::raii::Image, vk::raii::DeviceMemory>
  Fills ImageCreateInfo (imageType=e2D, extent, mipLevels=1, arrayLayers=1,
  samples=e1, tiling, usage, sharingMode=eExclusive, initialLayout=eUndefined)
  Allocates and binds memory (same pattern as createBuffer)

createImageView(vk::Image image, vk::Format format) → vk::raii::ImageView
  Fills ImageViewCreateInfo (viewType=e2D, format, subresourceRange:
  aspectMask=eColor, baseMipLevel=0, levelCount=1, baseArrayLayer=0, layerCount=1)

transitionImageLayout(vk::Image image,
                      vk::ImageLayout old_layout,
                      vk::ImageLayout new_layout) → void
  Executes via single-time command buffer.
  Two supported transitions:
    eUndefined → eTransferDstOptimal:
      srcStageMask=eTopOfPipe, dstStageMask=eTransfer
      srcAccessMask={},        dstAccessMask=eTransferWrite
    eTransferDstOptimal → eShaderReadOnlyOptimal:
      srcStageMask=eTransfer,  dstStageMask=eFragmentShader
      srcAccessMask=eTransferWrite, dstAccessMask=eShaderRead
  Other transitions: throw std::invalid_argument

copyBufferToImage(vk::Buffer buffer, vk::Image image,
                  uint32_t width, uint32_t height) → void
  Executes via single-time command buffer.
  BufferImageCopy: bufferOffset=0, bufferRowLength=0, bufferImageHeight=0,
  imageSubresource: {eColor, 0, 0, 1}, imageOffset={0,0,0}, imageExtent={width,height,1}
```

**New public methods:**

```
createTexture(const std::string& path) → Texture
  1. stbi_load(path, &w, &h, &channels, STBI_rgb_alpha) — throw if null
  2. createBuffer(w*h*4, eTransferSrc, eHostVisible|eHostCoherent) → staging
  3. mapMemory → memcpy pixels → unmapMemory; stbi_image_free(pixels)
  4. createImage(w, h, eR8G8B8A8Srgb, eOptimal, eTransferDst|eSampled, eDeviceLocal)
  5. transitionImageLayout(image, eUndefined, eTransferDstOptimal)
  6. copyBufferToImage(stagingBuffer, image, w, h)
  7. transitionImageLayout(image, eTransferDstOptimal, eShaderReadOnlyOptimal)
  8. createImageView(image, eR8G8B8A8Srgb)
  9. createSampler() (see below)
  10. return Texture(move(image), move(memory), move(imageView), move(sampler))

[private helper]
createSampler() → vk::raii::Sampler
  SamplerCreateInfo:
    magFilter=eLinear, minFilter=eLinear
    mipmapMode=eLinear
    addressModeU/V/W=eRepeat
    anisotropyEnable=vk::True
    maxAnisotropy=context_.getPhysicalDevice().getProperties().limits.maxSamplerAnisotropy
    borderColor=eIntOpaqueBlack
    unnormalizedCoordinates=vk::False
    compareEnable=vk::False, compareOp=eAlways
    mipLodBias=minLod=maxLod=0.0f

bindTextureToDescriptor(const Texture& texture) → void
  Fills DescriptorImageInfo:
    sampler   = *texture.getSampler()
    imageView = *texture.getImageView()
    imageLayout = eShaderReadOnlyOptimal
  Fills WriteDescriptorSet:
    dstSet = *textureDescriptorSet_
    dstBinding = 0
    descriptorType = eCombinedImageSampler
    pImageInfo = &image_info
  Calls device.updateDescriptorSets(write, {})
```

**New private members (declared in correct destruction order):**

```
textureDescriptorLayout_  const TextureDescriptorLayout&   (stored ref, not owned)
textureDescriptorPool_    vk::raii::DescriptorPool
textureDescriptorSet_     vk::raii::DescriptorSet
```

**`Renderer` constructor additions:**

- Gains `const TextureDescriptorLayout& texture_layout` parameter
- Stores `textureDescriptorLayout_` as member reference
- Creates `textureDescriptorPool_`: maxSets=1, one DescriptorPoolSize {eCombinedImageSampler, 1}, flags=eFreeDescriptorSet
- Allocates `textureDescriptorSet_` from that pool using `TextureDescriptorLayout::getLayout()`

**`drawFrame()` additions:**

- Gains `const vk::raii::DescriptorSet& texture_descriptor_set` parameter
- Forwards to `graphics_pipeline.record(..., texture_descriptor_set)`

---

### `src/renderer/GraphicsPipeline.h/.cpp`

**Constructor:**
```
GraphicsPipeline(const VulkanContext&,
                 const FrameDescriptorLayout&,
                 const TextureDescriptorLayout&,   ← new parameter
                 vk::Format color_format)
```
- Stores `textureDescriptorLayout_` as member
- `createPipelineLayout()` updated: `setLayoutCount=2`, `pSetLayouts` points to a `std::array` of two layout handles — `[*frameDescriptorLayout_.getLayout(), *textureDescriptorLayout_.getLayout()]`

**`record()` signature:**
```
record(vk::CommandBuffer command_buffer,
       vk::Extent2D extent,
       const vk::raii::ImageView& image_view,
       vk::Image image,
       const Mesh& mesh,
       const vk::raii::DescriptorSet& frame_descriptor_set,
       const vk::raii::DescriptorSet& texture_descriptor_set)   ← new parameter
```
- `bindDescriptorSets(eGraphics, *layout_, 0, {*frame_descriptor_set, *texture_descriptor_set}, {})`

**`createPipeline()` line 64:**
```cpp
// was:
std::array<vk::VertexInputAttributeDescription, 2> vertex_attribute_descriptions = Vertex::getAttributeDescriptions();
// becomes:
auto vertex_attribute_descriptions = Vertex::getAttributeDescriptions();
```
(Using `auto` so it automatically tracks the array size from `Vertex.h`.)

---

### `src/renderer/buffers/Vertex.h`

```cpp
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 texCoord;   // ← new

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions();
    //                                                      ^ was 2
};
```

Third attribute description:
```
location=2, binding=0, format=eR32G32Sfloat, offset=offsetof(Vertex, texCoord)
```

---

### `src/Application.h/.cpp`

**New members (declared in destruction order — between `swapChain_` and `graphicsPipeline_`):**

```cpp
std::unique_ptr<TextureDescriptorLayout> textureDescriptorLayout_;
std::unique_ptr<Texture> texture_;
```

**`initVulkan()` additions:**

```
textureDescriptorLayout_ = make_unique<TextureDescriptorLayout>(*context_)
graphicsPipeline_        = make_unique<GraphicsPipeline>(*context_, *frameDescriptorLayout_,
                                                          *textureDescriptorLayout_, swapChain_->getFormat())
renderer_                = make_unique<Renderer>(*context_, *frameDescriptorLayout_,
                                                  *textureDescriptorLayout_)
...
texture_ = make_unique<Texture>(renderer_->createTexture("textures/texture.jpg"))
renderer_->bindTextureToDescriptor(*texture_)
```

**`mainLoop()` — `drawFrame()` call gains texture descriptor set:**

```cpp
renderer_->drawFrame(
    *swapChain_,
    *graphicsPipeline_,
    *mesh_,
    computeUniformBufferObject(...),
    *textureDescriptorSet_   // accessed via renderer_ accessor or passed from Application
)
```

> Note: `textureDescriptorSet_` is private on `Renderer`. `drawFrame()` can access it directly as a member — the descriptor set is already stored there. `Application` does not pass it; `Renderer::drawFrame()` uses `textureDescriptorSet_` internally and forwards it to `record()`.

**Updated vertex data:**

```cpp
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
};
```

---

### `shaders/triangle.slang`

```
VertexInput struct  ← add float2 texCoord at location 2
VertexOutput struct ← add float2 texCoord
Vertex shader body  ← pass out_vertex.texCoord = vertex_in.texCoord
Fragment shader     ← add [[vk::binding(0, 1)]] Sampler2D texture_sampler
                       return texture_sampler.Sample(vertex_in.texCoord)
```

Slang binding syntax for set 1, binding 0: `[[vk::binding(0, 1)]]`
(first number = binding index, second = descriptor set index)

Recompile: `slangc shaders/triangle.slang -o build/shaders/triangle.spv`

---

## Destruction Order in `Application`

```
renderer_                  ← texture pool + set destroyed here
texture_                   ← Image, Memory, ImageView, Sampler destroyed here
graphicsPipeline_
textureDescriptorLayout_   ← layout destroyed after all sets/pools using it
frameDescriptorLayout_
swapChain_
context_
```

Declaration order in `Application.h` (member list, top = destroyed last):
```cpp
std::unique_ptr<VulkanContext>              context_;
std::unique_ptr<SwapChain>                  swapChain_;
std::unique_ptr<FrameDescriptorLayout>      frameDescriptorLayout_;
std::unique_ptr<TextureDescriptorLayout>    textureDescriptorLayout_;   // ← new, before pipeline
std::unique_ptr<GraphicsPipeline>           graphicsPipeline_;
std::unique_ptr<Renderer>                   renderer_;
std::unique_ptr<Mesh>                       mesh_;
std::unique_ptr<Texture>                    texture_;                   // ← new, after renderer
```

---

## Build Order

Follow bottom-up — each class must compile independently before the next:

1. `TextureDescriptorLayout` (no new dependencies)
2. `Texture` (no descriptor knowledge — just RAII handles)
3. `VulkanContext` changes (anisotropy enable)
4. `Renderer` — new private helpers + `createTexture` + `bindTextureToDescriptor` + constructor changes
5. `GraphicsPipeline` — two-set layout + updated `record()` signature
6. `Vertex.h` + `GraphicsPipeline.cpp` line 64
7. `Application` — new members, `initVulkan()` additions, vertex data update
8. Shader update + recompile
9. Full build + run — texture visible on spinning rectangle

---

## Smoke Test

Build succeeds and at runtime:
- Texture image visible on the rectangle (not a solid colour)
- Rectangle rotates correctly
- Resize preserves texture and proportions
- Validation layers silent (OBS_HOOK exempt)
- Clean shutdown — no leaked image/sampler/descriptor resources
