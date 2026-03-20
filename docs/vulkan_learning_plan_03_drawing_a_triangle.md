# Learning Plan — Chapter 03: Drawing a Triangle

> **Estimated total time:** ~14 sessions × ~2 hours = ~28 hours
> **Prerequisites:** Chapter 00-02 complete — working build environment, GLFW window opens, Vulkan extensions enumerated
> **Implementation ground truth:** `docs/vulkan_implementation_plan_03_drawing_a_triangle.md`

---

## Chapter Milestones Overview

| # | Milestone | Session Type | Est. Sessions |
|---|-----------|--------------|---------------|
| M1 | Instance & Validation Layers | Theory + Implementation | 2 |
| M2 | Surface, Physical Device & Queue Families | Theory + Implementation | 2 |
| M3 | Logical Device & Queues | Implementation | 1 |
| M4 | Swap Chain & Image Views | Theory + Implementation | 3 |
| M5 | Graphics Pipeline | Theory + Implementation | 3 |
| M6 | Command Buffers, FrameData & Synchronisation | Theory + Implementation | 2 |
| M7 | Render Loop & Swap Chain Recreation | Implementation | 2 |

**Total: 15 sessions**

> Chapter 03 is the most object-heavy chapter in the tutorial. The number of objects you must create before a single pixel appears on screen is deliberately large — this is Vulkan's explicit design. Expect M1–M3 to feel like setup. The payoff arrives at M7.

---

## Milestone M1 — Instance & Validation Layers

> **Session type:** Theory + Implementation
> **Estimated time:** Session A (Theory ~1.5h) + Session B (Implementation ~2h)
> **Goal:** Understand why the Vulkan instance exists and what validation layers do, then implement `VulkanContext` up to and including the debug messenger.
> **Source:** §03.00.01 Instance | §03.00.02 Validation Layers | `vulkan_chapter_03_drawing_a_triangle.md` §2 and §3

---

### Session A — Theory

#### Session Checklist
- [ ] Read §03.00.01 and §03.00.02 in `vulkan_chapter_03_drawing_a_triangle.md` in full
- [ ] Read the corresponding tutorial pages at `https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/01_Instance.html` and `02_Validation_layers.html`
- [ ] Answer all Comprehension Questions below (write your answers — don't just read)
- [ ] Write a one-paragraph summary of what the instance and validation layers are in your own words

#### Comprehension Questions

**Awareness — Why does this exist?**

1. The `vk::raii::Context` is not the same as the Vulkan instance. Read §03.00.01 and explain: what is the difference between `vk::raii::Context` and `vk::raii::Instance`? What does each represent?
   *→ Hint: §03.00.01, "Key API Types" table and "Code Walkthrough"*

2. The tutorial says Vulkan has "minimal driver overhead" by default. What does that mean for error checking? What would happen if you passed a null pointer to a required Vulkan function parameter in a release build with no validation layers?

3. Read §03.00.02. Validation layers are described as "optional components." When exactly should you enable them, and when should you disable them? What macro controls this in the implementation plan?
   *→ Hint: Implementation Plan `VulkanContext.h`, `ENABLE_VALIDATION_LAYERS` definition*

**Conceptual — What is it and how does it work?**

4. The `vk::ApplicationInfo` struct has an `apiVersion` field. What happens if you set this to a version higher than what the physical device's driver supports? At what point in the application lifecycle does this fail — instance creation, device creation, or later?

5. There is a "bootstrapping problem" with the debug messenger. Describe the problem in your own words: why can't you simply create the debug messenger after the instance and have it cover all validation messages?
   *→ Hint: §03.00.02, "Code Walkthrough" — the `pNext` trick*

6. The callback function for the debug messenger must have a specific C-compatible signature. Look at the implementation plan's `debugCallback` declaration. Why is it a `static` class method rather than a regular member function? What would prevent a non-static member function from being used here?

**Dependency / flow**

7. The `VulkanContext` constructor takes a `GLFWwindow*`. At this milestone (instance + debug messenger only), is the window pointer actually used? If not, why is it passed at all — what later step inside `VulkanContext` will need it?

---

### Session B — Implementation

> **Depends on:** M1 Theory session complete and all questions answered.

#### Session Checklist
- [ ] Can explain in plain language what the instance and debug messenger are before writing any code
- [ ] All M1 theory questions answered
- [ ] All items in the Implementation Checklist below complete
- [ ] All Verification Tests below pass

#### Implementation Checklist

- [ ] `VulkanContext.h` — class skeleton with correct member declaration order, `= delete` copy/move, all private method declarations
- [ ] `VulkanContext.cpp` — constructor skeleton calling private methods in order
- [ ] `createInstance()` — `ApplicationInfo`, extension collection from GLFW, validation of supported extensions, `InstanceCreateInfo` with `pNext` debug messenger info (debug builds), RAII instance constructed
- [ ] `VALIDATION_LAYERS` and `REQUIRED_DEVICE_EXTENSIONS` static member definitions in `.cpp`
- [ ] `checkValidationLayerSupport()` — enumerates instance layer properties, searches for `VK_LAYER_KHRONOS_validation`
- [ ] `getRequiredInstanceExtensions()` — collects GLFW extensions, appends `VK_EXT_debug_utils` in debug builds
- [ ] `makeDebugMessengerCreateInfo()` — builds create info with warning+error severity, validation+performance types, callback pointer
- [ ] `debugCallback()` — static method that prints `callbackData->pMessage` to stderr, returns `VK_FALSE`
- [ ] `setupDebugMessenger()` — creates `vk::raii::DebugUtilsMessengerEXT` (debug builds only)
- [ ] `Application.h` — class skeleton, GLFW window, four `unique_ptr` members declared in dependency order
- [ ] `Application.cpp` — `initWindow()`, `run()` skeleton calling init methods, resize callback stub
- [ ] `main.cpp` — entry point only, creates `Application`, calls `run()`, catches `std::exception`

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Instance creates without crash | Run the app | Window opens | [ ] |
| T2 | Validation layer active | Run with `VK_LAYER_KHRONOS_validation` enabled — check stderr | No validation errors | [ ] |
| T3 | Debug callback fires | Intentionally pass wrong `apiVersion` (e.g., Vulkan 99.0), observe output | Validation error message printed to stderr | [ ] |
| T4 | Restore correct version | Revert T3 change | App runs cleanly again | [ ] |

---

## Milestone M2 — Surface, Physical Device & Queue Families

> **Session type:** Theory + Implementation
> **Estimated time:** Session A (Theory ~1.5h) + Session B (Implementation ~2h)
> **Goal:** Understand why Vulkan separates hardware selection from hardware usage, what queue families are, and why the surface must exist before device selection. Implement `createSurface`, `pickPhysicalDevice`, and `findQueueFamily`.
> **Source:** §03.00.03 Physical Devices | §03.01.00 Window Surface | `vulkan_chapter_03_drawing_a_triangle.md` §4 and §6

---

### Session A — Theory

#### Session Checklist
- [ ] Read §03.00.03 and §03.01.00 in `vulkan_chapter_03_drawing_a_triangle.md` in full
- [ ] Read `https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/03_Physical_devices_and_queue_families.html`
- [ ] Read `https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/00_Window_surface.html`
- [ ] Answer all Comprehension Questions below

#### Comprehension Questions

**Awareness — Why does this exist?**

1. Most graphics APIs pick a GPU automatically. Vulkan forces you to enumerate and select one explicitly. What is the practical benefit of this on a system with both an integrated GPU and a discrete GPU? Name one scenario where you would choose the integrated over the discrete.

2. The tutorial checks four criteria for device suitability. Read the implementation plan's `isDeviceSuitable()` notes. Why does it check `apiVersion ≥ 1.3` rather than just checking for specific feature support? What does the API version actually represent?

3. The surface (`VkSurfaceKHR`) must be created before physical device selection. Explain why: what specific check during device selection requires the surface to already exist?
   *→ Hint: §03.00.03 "Common Pitfalls" and §03.01.00 "Concepts"*

**Conceptual — What is it and how does it work?**

4. A queue family is described as a group of queues that share the same capabilities. A physical device might expose three queue families: one with `eGraphics | eCompute | eTransfer`, one with `eTransfer` only, one with `eCompute` only. For this tutorial's simple renderer, which of these three families would `findQueueFamily` select, and why might you want to use a dedicated transfer-only family in a more advanced renderer?

5. `glfwCreateWindowSurface` is described as a cross-platform abstraction. What platform-specific work is it hiding? If you were on Windows without GLFW, what Vulkan extension and function would you call directly?
   *→ Hint: §03.01.00 "Concepts", "Platform-Specific Implementation"*

**Dependency / flow**

6. The `VulkanContext` constructor calls five private methods in order. After M1, `instance_` and `debugMessenger_` are created. Trace the data flow from `instance_` into `createSurface`: what exact call uses `instance_` to create the surface, and what is passed from the Application side (via constructor parameter) to make platform-specific surface creation work?

7. `findQueueFamily` checks two independent conditions on each queue family. Explain why graphics support (`eGraphics` flag) and presentation support (`getSurfaceSupportKHR`) are checked separately rather than being a single combined flag. What does this tell you about Vulkan's design philosophy?

---

### Session B — Implementation

#### Session Checklist
- [ ] M1 complete (instance + debug messenger working, T1–T4 passing)
- [ ] M2 theory questions answered
- [ ] All items in the Implementation Checklist below complete
- [ ] All Verification Tests below pass

#### Implementation Checklist

- [ ] `createSurface()` — calls `glfwCreateWindowSurface`, wraps raw handle in `vk::raii::SurfaceKHR`, called after instance creation
- [ ] `pickPhysicalDevice()` — enumerates physical devices, calls `isDeviceSuitable` on each, stores first passing device and its queue family index; logs selected device name to stdout
- [ ] `isDeviceSuitable()` — checks API version ≥ 1.3, calls `findQueueFamily`, calls `checkDeviceExtensionSupport`, chains feature structs to check `dynamicRendering` and `extendedDynamicState`
- [ ] `findQueueFamily()` — iterates queue families, returns index where both `eGraphics` set and `getSurfaceSupportKHR` true; throws `std::runtime_error` if none found
- [ ] `checkDeviceExtensionSupport()` — enumerates device extensions, verifies all `REQUIRED_DEVICE_EXTENSIONS` are present
- [ ] `getSurface()` accessor implemented
- [ ] Physical device name printed to stdout on startup (inside `pickPhysicalDevice`)

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Physical device selected | Run app | GPU name printed to stdout (e.g., "NVIDIA GeForce RTX 2070") | [ ] |
| T2 | Surface created | Run with validation layers | No errors related to surface | [ ] |
| T3 | Wrong device rejected | Temporarily raise minimum API version to 99.0 | `std::runtime_error` thrown, app exits with error message | [ ] |
| T4 | Validation layers silent | Run normally | Zero errors or warnings in stderr | [ ] |

---

## Milestone M3 — Logical Device & Queues

> **Session type:** Implementation
> **Estimated time:** ~1.5 hours
> **Goal:** Complete `VulkanContext` by creating the logical device with the required feature chain and retrieving the queue handle.
> **Depends on:** M2 fully complete. The physical device and queue family index must be known before this step.
> **Source:** §03.00.04 Logical Device | `vulkan_chapter_03_drawing_a_triangle.md` §5

> **Note:** No dedicated theory session — the concept of logical device as an "interface to the physical device" follows directly from M2. However, `vk::StructureChain` and feature chaining are new syntax — read §03.00.04 "Concepts" carefully before implementing.

#### Session Checklist
- [ ] Read §03.00.04 in `vulkan_chapter_03_drawing_a_triangle.md` before writing any code
- [ ] Can explain in plain language what `vk::StructureChain` does and why it exists
- [ ] All items in the Implementation Checklist below complete
- [ ] All Verification Tests below pass

#### Comprehension Questions (answer before implementing)

1. `vk::StructureChain<A, B, C>` automatically links the `pNext` pointers of A, B, and C. What problem does it solve compared to manually setting `pNext` yourself? What class of bug does it prevent?

2. You enable `dynamicRendering = true` in `PhysicalDeviceVulkan13Features`. But you already checked that the physical device *supports* this feature during `isDeviceSuitable`. Why must you enable it again here — what would happen if you checked support but forgot to enable it in the device create info?
   *→ Hint: §03.00.04 "Common Pitfalls"*

3. Dependency flow: `findQueueFamily` was called in M2 and stored `queueFamilyIndex_`. How does `createLogicalDevice` consume this stored value — is it passed as a parameter or read from the member? Why is this safe even though `pickPhysicalDevice` and `createLogicalDevice` are separate functions?

#### Implementation Checklist

- [ ] `createLogicalDevice()` — builds `vk::StructureChain` with `PhysicalDeviceFeatures2`, `PhysicalDeviceVulkan13Features` (`dynamicRendering = true`, `synchronization2 = true`), `PhysicalDeviceExtendedDynamicStateFeaturesEXT` (`extendedDynamicState = true`)
- [ ] `vk::DeviceQueueCreateInfo` — queue family index from `queueFamilyIndex_`, count 1, priority 1.0f
- [ ] `vk::DeviceCreateInfo` — `pNext` = address of root feature struct from chain, references queue create info and `REQUIRED_DEVICE_EXTENSIONS`
- [ ] `vk::raii::Device` constructed from physical device and create info
- [ ] Queue retrieved via `device_.getQueue(queueFamilyIndex_, 0)` and stored in `graphicsQueue_`
- [ ] All accessors (`getDevice`, `getPhysicalDevice`, `getQueue`, `getQueueFamilyIndex`, `getSurface`, `getInstance`) implemented

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Logical device created | Run app | No crash, no validation errors | [ ] |
| T2 | Feature chain accepted | Run with validation layers | No errors about unsupported features | [ ] |
| T3 | `VulkanContext` fully functional | All accessors return non-null handles | Confirmed by adding a temporary stdout print of `device_.getDispatcher()` not being null | [ ] |

---

## Milestone M4 — Swap Chain & Image Views

> **Session type:** Theory + Implementation
> **Estimated time:** Session A (Theory ~1.5h) + Session B (Implementation ~2h) + Session C (Image views + recreate ~1.5h)
> **Goal:** Understand the swap chain's role as Vulkan's presentation infrastructure, the three property categories, and the four presentation modes. Implement the full `SwapChain` class including creation, image views, and recreation.
> **Source:** §03.01.01 Swap Chain | §03.01.02 Image Views | `vulkan_chapter_03_drawing_a_triangle.md` §7 and §8

---

### Session A — Theory

#### Session Checklist
- [ ] Read §03.01.01 and §03.01.02 in `vulkan_chapter_03_drawing_a_triangle.md` in full
- [ ] Read `https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/01_Swap_chain.html`
- [ ] Read `https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/02_Image_views.html`
- [ ] Answer all Comprehension Questions below

#### Comprehension Questions

**Awareness — Why does this exist?**

1. OpenGL has a default framebuffer that appears automatically. Vulkan has no default framebuffer. Explain in your own words why the swap chain exists and what it replaces conceptually.

2. The tutorial lists four presentation modes. For each one, describe in one sentence what it does and whether it can cause screen tearing:
   - `eImmediate`
   - `eFifo`
   - `eFifoRelaxed`
   - `eMailbox`
   Which is the only mode guaranteed to exist on all Vulkan implementations, and why?

3. An image view is described as "a view into an image." Why can't you use a `vk::Image` directly as a render target? What additional information does the image view provide that the raw image handle does not?

**Conceptual — What is it and how does it work?**

4. The swap extent must use `glfwGetFramebufferSize` rather than `glfwGetWindowSize`. Explain the difference between these two functions and why using the wrong one produces incorrect results on high-DPI (Retina) displays.

5. The swap chain creation info has a field `imageSharingMode`. The implementation plan uses `eExclusive`. Under what condition would you need `eConcurrent` instead, and what is the performance trade-off between the two?
   *→ Hint: §03.01.01 "Creating the Swap Chain" — `imageSharingMode` explanation*

6. `swapChain_.getImages()` returns a `std::vector<vk::Image>`. These images are not created by you and must not be destroyed by you. Who owns them, and what happens to them when the swap chain is destroyed?

**Dependency / flow**

7. Trace the path of the surface from creation to swap chain use: the surface was created inside `VulkanContext` in M2. How does `SwapChain` access it — is it passed as a constructor parameter, or read from `VulkanContext`? Write the exact accessor call the `SwapChain` implementation will use.

8. The `SwapChain` constructor takes `const VulkanContext& ctx`. List every piece of data that `SwapChain` reads from `ctx` during `create()`. For each one, name the accessor method it will call.

---

### Session B — Implementation (Swap Chain Creation)

#### Session Checklist
- [ ] M3 complete and all tests passing
- [ ] M4 theory questions answered
- [ ] All items in the Implementation Checklist below complete

#### Implementation Checklist

- [ ] `SwapChain.h` — class skeleton, member declarations, all method signatures
- [ ] `SwapChain.cpp` — constructor stores refs, calls `create()` then `createImageViews()`
- [ ] `chooseFormat()` — scans for `eB8G8R8A8Srgb` + `eSrgbNonlinear`, falls back to `formats[0]`
- [ ] `choosePresentMode()` — prefers `eMailbox`, falls back to `eFifo`
- [ ] `chooseExtent()` — returns `currentExtent` if locked; else queries `glfwGetFramebufferSize` and clamps to `[min, max]`
- [ ] `chooseImageCount()` — `max(3, minImageCount)` clamped to `maxImageCount` (if non-zero)
- [ ] `create()` — queries all three property categories, calls choosers, builds `vk::SwapchainCreateInfoKHR`, constructs `vk::raii::SwapchainKHR`, stores format and extent as members
- [ ] All accessors (`getFormat`, `getExtent`, `imageCount`, `get`, `getImageViews`) implemented

---

### Session C — Implementation (Image Views + Recreation)

#### Session Checklist
- [ ] Session B complete
- [ ] All items below complete
- [ ] All Verification Tests pass

#### Implementation Checklist

- [ ] `createImageViews()` — iterates `swapChain_.getImages()`, creates one `vk::raii::ImageView` per image (`e2D`, `eColor` aspect, mip 0, layer 0, identity component mapping)
- [ ] `cleanup()` — resets `imageViews_` vector to empty, resets `swapChain_` to null
- [ ] `recreate()` — calls `cleanup()`, `create()`, `createImageViews()` in order
- [ ] `Application::initVulkan()` — constructs `swapChain_` via `make_unique<SwapChain>(*context_, window_)`
- [ ] Selected format and present mode printed to stdout at creation

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Swap chain created | Run app | No crash, format + present mode printed to stdout | [ ] |
| T2 | Image count ≥ 3 | Add temporary stdout print of `swapChain_->imageCount()` | Printed count ≥ 3 | [ ] |
| T3 | Image views match image count | Add temporary stdout print of `imageViews_.size()` | Equals `imageCount()` | [ ] |
| T4 | Validation layers silent | Run normally | Zero errors or warnings | [ ] |

---

## Milestone M5 — Graphics Pipeline

> **Session type:** Theory + Implementation
> **Estimated time:** Session A (Theory ~2h) + Session B (Shaders + Layout ~2h) + Session C (Fixed functions + Pipeline + record() ~2h)
> **Goal:** Understand every stage of the graphics pipeline and the purpose of dynamic rendering. Write the Slang shaders, build all fixed-function state, create the pipeline object, and implement `record()`.
> **Source:** §03.02.00–§03.02.04 | `vulkan_chapter_03_drawing_a_triangle.md` §9–§13

---

### Session A — Theory

#### Session Checklist
- [ ] Read §9 through §13 in `vulkan_chapter_03_drawing_a_triangle.md` in full
- [ ] Read all five pipeline sub-pages at `https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/02_Graphics_pipeline_basics/`
- [ ] Answer all Comprehension Questions below
- [ ] Draw (or describe in words) the complete pipeline stage sequence from vertex data to pixel output

#### Comprehension Questions

**Awareness — Why does this exist?**

1. Vulkan's graphics pipeline is described as "almost completely immutable." What does this mean, and what is the performance benefit of this immutability compared to OpenGL's mutable state machine? What must you do if you need a different blend function at runtime?

2. The tutorial uses dynamic rendering instead of a traditional render pass. What two objects does dynamic rendering eliminate, and what does it replace them with? Name the Vulkan version that made dynamic rendering part of the core spec.

3. SPIR-V is a bytecode format, not source code. Why does Vulkan require bytecode rather than accepting shader source directly (like OpenGL accepts GLSL strings)?

**Conceptual — What is it and how does it work?**

4. The vertex shader in this chapter has no vertex buffer input — vertex data is hardcoded in the shader. Looking at the implementation plan's `createPipeline()` notes, how is this reflected in the `vk::PipelineVertexInputStateCreateInfo` struct?

5. The viewport and scissor are listed as dynamic states. What does marking them as dynamic mean concretely — where are their actual values set, and when? What would break if they were baked into the pipeline instead?

6. Fragment colour interpolation: the three triangle vertices have colours red, green, and blue. The interior of the triangle shows a smooth colour gradient. Explain exactly when and where this interpolation happens — which pipeline stage performs it, and does your fragment shader code do anything to produce the gradient?

7. `vk::PipelineRenderingCreateInfo` is placed in the `pNext` chain of `vk::GraphicsPipelineCreateInfo`. What information does it contain, and why must the pipeline know the attachment format at creation time rather than at draw time?

**Dependency / flow**

8. `GraphicsPipeline` takes `vk::Format color_format` as a constructor parameter. Trace where this value comes from: starting from `SwapChain::create()`, identify the chain of calls and member accesses that delivers the format into `GraphicsPipeline`'s constructor in `Application::initVulkan()`.

---

### Session B — Implementation (Shaders + Layout)

#### Session Checklist
- [ ] M4 complete and all tests passing
- [ ] M5 theory questions answered
- [ ] All items below complete

#### Implementation Checklist

- [ ] `shaders/triangle.slang` — vertex entry point (`vertexMain`): hardcoded positions array, hardcoded colours array, index by `SV_VertexID`, outputs `SV_Position` and interpolated `COLOR`
- [ ] `shaders/triangle.slang` — fragment entry point (`fragmentMain`): receives interpolated `COLOR`, outputs to `SV_Target`
- [ ] `CMakeLists.txt` — shader compilation `add_custom_command` + `add_custom_target` + `add_dependencies` for both `.spv` outputs
- [ ] Shaders compile without errors via `slangc.exe`
- [ ] `utils/FileUtils.h` + `FileUtils.cpp` — `readSpirv()` opens binary, checks 4-byte alignment, returns `std::vector<uint32_t>`
- [ ] `GraphicsPipeline.h` — class skeleton, member declarations, all method signatures
- [ ] `GraphicsPipeline.cpp` — constructor calls `createPipelineLayout()` then `createPipeline(color_format)`
- [ ] `createShaderModule()` — calls `readSpirv()`, builds `vk::ShaderModuleCreateInfo`, returns `vk::raii::ShaderModule`
- [ ] `createPipelineLayout()` — empty layout (zero descriptor sets, zero push constants)

---

### Session C — Implementation (Fixed Functions + Pipeline + record())

#### Session Checklist
- [ ] Session B complete, shaders compiling and `createPipelineLayout()` working
- [ ] All items below complete
- [ ] All Verification Tests pass

#### Implementation Checklist

- [ ] `createPipeline()` — all seven fixed-function structs built in order (vertex input, input assembly, viewport state, dynamic state, rasterizer, multisample, colour blend attachment + blend state)
- [ ] `vk::PipelineRenderingCreateInfo` populated with `color_format` and placed in pipeline create info `pNext` chain
- [ ] `vk::GraphicsPipelineCreateInfo` assembled, `renderPass = nullptr`
- [ ] `vk::raii::Pipeline` created via `ctx_.getDevice().createGraphicsPipeline(nullptr, create_info)`
- [ ] `transitionImageLayout()` static helper implemented using `vk::ImageMemoryBarrier2` + `vk::DependencyInfo` + `pipelineBarrier2()`
- [ ] `record()` — full sequence: transition to `eColorAttachmentOptimal`, fill `RenderingAttachmentInfo` (clear black, store), `beginRendering`, bind pipeline, set viewport, set scissor, `draw(3,1,0,0)`, `endRendering`, transition to `ePresentSrcKHR`
- [ ] `Application::initVulkan()` — constructs `pipeline_` via `make_unique<GraphicsPipeline>(*context_, swapChain_->getFormat())`

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Shaders compile | Run CMake build | No `slangc` errors, `.spv` files appear in build/shaders/ | [ ] |
| T2 | Pipeline creates | Run app | No crash on pipeline construction | [ ] |
| T3 | Validation layers silent | Run with layers enabled | Zero errors about pipeline state | [ ] |

---

## Milestone M6 — Command Buffers, FrameData & Synchronisation

> **Session type:** Theory + Implementation
> **Estimated time:** Session A (Theory ~1.5h) + Session B (Implementation ~2h)
> **Goal:** Understand the command buffer recording model and the difference between semaphores and fences. Implement `FrameData`, the command pool, and per-frame resource creation inside `Renderer`.
> **Source:** §03.03.00–§03.03.01 | §03.03.03 (frames in flight) | `vulkan_chapter_03_drawing_a_triangle.md` §14 and §16

---

### Session A — Theory

#### Session Checklist
- [ ] Read §14 and §16 in `vulkan_chapter_03_drawing_a_triangle.md` in full
- [ ] Read `https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/01_Command_buffers.html`
- [ ] Read `https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/03_Frames_in_flight.html`
- [ ] Answer all Comprehension Questions below

#### Comprehension Questions

**Awareness — Why does this exist?**

1. Vulkan uses command buffers to record work and then submit it to the GPU. Why is this "record then submit" model preferred over the immediate-mode model (where each API call directly triggers GPU work)? Name one concrete performance benefit.

2. A semaphore and a fence both involve waiting. Explain the fundamental difference: who waits on a semaphore versus who waits on a fence? Which one blocks the CPU, and which one is purely GPU-side?

3. `MAX_FRAMES_IN_FLIGHT = 2` means two frames can be processed at the same time. Describe concretely what the CPU is doing while frame N is on the GPU — what work is it doing for frame N+1?

**Conceptual — What is it and how does it work?**

4. The command pool is created with the `eResetCommandBuffer` flag. What does this flag allow? What would you have to do instead to rerecord a command buffer if this flag were NOT set?

5. `FrameData` groups four objects: `commandBuffer`, `imageAvailable`, `renderFinished`, `inFlightFence`. For each one, describe in one sentence its exact role in the per-frame synchronisation sequence.

6. The `inFlightFence` is created with `eFenceCreateFlagBits::eSignaled`. Why? What would happen on the very first frame if it were created unsignalled?

**Dependency / flow**

7. `Renderer` takes `const VulkanContext& ctx` in its constructor. Inside `createCommandPool()`, exactly what data does it read from `ctx` — and what would break if `Renderer` tried to cache the queue family index itself rather than reading it from `ctx`?

---

### Session B — Implementation

#### Session Checklist
- [ ] M5 complete (pipeline creates without errors)
- [ ] M6 theory questions answered
- [ ] All items below complete
- [ ] All Verification Tests pass

#### Implementation Checklist

- [ ] `FrameData.h` — struct with four null-initialised RAII members
- [ ] `Renderer.h` — class skeleton, `MAX_FRAMES_IN_FLIGHT` constant, `frames_` array, `commandPool_`, `currentFrame_`, `drawFrame()` signature
- [ ] `Renderer.cpp` — constructor calls `createCommandPool()` then `createFrameData()`
- [ ] `createCommandPool()` — `eResetCommandBuffer` flag, queue family index from `ctx_.getQueueFamilyIndex()`
- [ ] `createFrameData()` — for each of the two frames: allocate primary command buffer from pool, create `imageAvailable` semaphore, create `renderFinished` semaphore, create `inFlightFence` with `eSignaled`
- [ ] `Application::initVulkan()` — constructs `renderer_` via `make_unique<Renderer>(*context_)`

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| T1 | Renderer constructs | Run app | No crash on renderer construction | [ ] |
| T2 | Command pool created | Run with validation layers | No errors related to command pool | [ ] |
| T3 | Sync objects created | Run with validation layers | No errors related to semaphores or fences | [ ] |
| T4 | Clean shutdown | Close window | No validation errors on destruction | [ ] |

---

## Milestone M7 — Render Loop & Swap Chain Recreation

> **Session type:** Implementation
> **Estimated time:** Session A (drawFrame ~2h) + Session B (resize + Application wiring + verification ~2h)
> **Goal:** Implement `Renderer::drawFrame()` and the complete resize/recreation path. Wire everything together in `Application`. See a triangle on screen.
> **Depends on:** All previous milestones complete and all tests passing.
> **Source:** §03.03.02 | §03.04 | `vulkan_chapter_03_drawing_a_triangle.md` §15 and §17

> **Note:** This milestone has no theory session — all required concepts (semaphores, fences, swap chain recreation) were covered in M4 and M6. However, re-read §15 "Code Walkthrough" carefully before implementing `drawFrame` — the fence reset ordering is the single most critical correctness detail in the chapter.

---

### Session A — Implementation (drawFrame)

#### Pre-implementation questions (answer before writing any code)

1. `drawFrame` resets the fence with `resetFences` and then calls `acquireNextImage`. Or does it? Read the implementation plan's `drawFrame` sequence again carefully — in what order do fence reset and image acquisition happen, and why does the order matter? What exact failure mode does the wrong order create?

2. `vk::SubmitInfo2` specifies a wait stage of `eColorAttachmentOutput`. Explain why this specific stage is chosen rather than `eTopOfPipe`. What GPU work can proceed during the wait?

#### Session Checklist
- [ ] Pre-implementation questions answered before writing `drawFrame`
- [ ] All items in the Implementation Checklist below complete

#### Implementation Checklist

- [ ] `Renderer::drawFrame()` — step 1: `waitForFences` with `UINT64_MAX` timeout
- [ ] `Renderer::drawFrame()` — step 2: `acquireNextImage` with `imageAvailable` semaphore; if `eErrorOutOfDateKHR` return `true` immediately (fence NOT reset yet)
- [ ] `Renderer::drawFrame()` — step 3: `resetFences` (only after confirmed acquisition)
- [ ] `Renderer::drawFrame()` — step 4: reset command buffer, call `pipeline.record(commandBuffer, extent, imageView)`
- [ ] `Renderer::drawFrame()` — step 5: `vk::SubmitInfo2` — wait on `imageAvailable` at `eColorAttachmentOutput`, signal `renderFinished`, signal `inFlightFence`; submit via `ctx_.getQueue().submit2()`
- [ ] `Renderer::drawFrame()` — step 6: `vk::PresentInfoKHR` — wait on `renderFinished`, present `image_index`; if result is `eErrorOutOfDateKHR` or `eSuboptimalKHR` return `true`
- [ ] `Renderer::drawFrame()` — step 7: `currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT`, return `false`

---

### Session B — Implementation (Resize + Wiring + Verification)

#### Session Checklist
- [ ] Session A complete
- [ ] All items below complete
- [ ] All seven Verification Tests pass
- [ ] Triangle visible on screen — chapter considered complete

#### Implementation Checklist

- [ ] `Application::mainLoop()` — event loop: `glfwPollEvents`, `renderer_->drawFrame(*swapChain_, *pipeline_)`, check return value + `framebufferResized_`, call `onResize()` if needed; `context_->getDevice().waitIdle()` after loop
- [ ] `Application::onResize()` — `waitIdle`, `swapChain_->recreate()` (pipeline unchanged — format stable, extent is dynamic)
- [ ] `framebufferResizeCallback()` — retrieve `Application*` via `glfwGetWindowUserPointer`, set `framebufferResized_ = true`
- [ ] `Application::initWindow()` — register `glfwSetFramebufferSizeCallback` and `glfwSetWindowUserPointer`
- [ ] Minimised window handled — in `onResize()`, loop polling `glfwWaitEvents()` until `glfwGetFramebufferSize` returns non-zero dimensions
- [ ] `scene/.gitkeep` file created (empty directory placeholder for future ECS layer)
- [ ] `CMakeLists.txt` updated — all new `.cpp` files in `add_executable`, `src/` in `target_include_directories`

#### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|-------------|-------------|-----------------|-------|
| V1 | Application launches | Run the app | Window opens, no crash | [ ] |
| V2 | Triangle visible | Look at the window | Smooth RGB-shaded triangle on black background | [ ] |
| V3 | Validation layers silent | Run with `VK_LAYER_KHRONOS_validation` | Zero errors or warnings in stderr during runtime | [ ] |
| V4 | Window resize | Drag window corner to resize | Triangle redraws correctly at new size, no crash, no validation errors | [ ] |
| V5 | Window minimise | Minimise then restore | App pauses, resumes correctly, triangle still renders | [ ] |
| V6 | Clean exit | Close the window normally | No validation errors on shutdown, clean process exit | [ ] |
| V7 | GPU name logged | Check stdout | Physical device name printed on startup | [ ] |

**Chapter 03 is complete when all seven tests pass with zero validation layer errors.**

---

## Chapter Progress Tracker

| Milestone | Theory ✓ | Impl ✓ | Tests Pass ✓ |
|-----------|----------|--------|--------------|
| M1 — Instance & Validation Layers | [ ] | [ ] | [ ] |
| M2 — Surface, Physical Device & Queue Families | [ ] | [ ] | [ ] |
| M3 — Logical Device & Queues | N/A | [ ] | [ ] |
| M4 — Swap Chain & Image Views | [ ] | [ ] | [ ] |
| M5 — Graphics Pipeline | [ ] | [ ] | [ ] |
| M6 — Command Buffers, FrameData & Synchronisation | [ ] | [ ] | [ ] |
| M7 — Render Loop & Swap Chain Recreation | N/A | [ ] | [ ] |

**Chapter complete when all rows are fully ticked.**

---

## Session Log

Fill in after each session.

| Session # | Date | Milestone(s) | What was covered | Blockers / open questions |
|-----------|------|--------------|------------------|--------------------------|
| 1 | | | | |
| 2 | | | | |
| 3 | | | | |
| 4 | | | | |
| 5 | | | | |
| 6 | | | | |
| 7 | | | | |
| 8 | | | | |
| 9 | | | | |
| 10 | | | | |
| 11 | | | | |
| 12 | | | | |
| 13 | | | | |
| 14 | | | | |
| 15 | | | | |
