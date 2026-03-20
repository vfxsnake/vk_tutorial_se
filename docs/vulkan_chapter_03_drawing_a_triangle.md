# Chapter 03: Drawing a Triangle

> **Source:** https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/
> **Vulkan Version:** 1.4 | **Language:** C++20 | **Shading:** Slang | **Bindings:** Vulkan-Hpp RAII

---

## Overview

Chapter 03 is the heart of the foundational Vulkan tutorial. It takes you from a blank window to a rendered, coloured triangle on screen — and in doing so walks through nearly every major subsystem of the Vulkan API. The chapter is large by design: Vulkan requires explicit control over every layer of the graphics stack, and this chapter introduces all of them at once.

The journey proceeds in five major phases:

1. **Setup** — Base application structure, Vulkan instance, validation layers, physical device selection, and logical device creation.
2. **Presentation** — Window surface, swap chain, and image views.
3. **Graphics Pipeline** — Shader modules (Slang/SPIR-V), all fixed-function states, and the pipeline object itself (using dynamic rendering, no legacy render pass).
4. **Drawing** — Command pools, command buffers, synchronisation primitives, and the render loop.
5. **Swap Chain Recreation** — Handling window resize and surface invalidation gracefully.

After completing this chapter the application opens a window, initialises every required Vulkan object, and renders a smooth-shaded RGB triangle at real-time frame rates.

---

## Section 1 — Base Code (§03.00.00)

### Concepts

The base skeleton establishes the structural contract for the entire application. Everything else builds on top of it. Three methods define the lifecycle:

- `initVulkan()` — constructs every Vulkan object in dependency order
- `mainLoop()` — pumps window events and triggers one draw per iteration
- `cleanup()` — releases resources (or, with RAII, lets destructors handle it)

The tutorial enables `VULKAN_HPP_NO_STRUCT_CONSTRUCTORS`, which forces C++20 designated initialisers for all Vulkan structs. This is a deliberate choice: every field must be named explicitly, making it obvious what you are setting and impossible to silently skip a required field.

GLFW window creation requires two important flags: telling GLFW not to create an OpenGL context (`GLFW_NO_API`), and initially disabling window resizing (we handle that explicitly in §03.04). The main loop calls `glfwPollEvents()` each iteration and exits when `glfwWindowShouldClose()` returns true.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::raii::Context` | Class | Root RAII context; entry point for Vulkan-Hpp RAII |
| `vk::raii::Instance` | Class | Owns the Vulkan instance; destroyed automatically |
| `glfwInit()` / `glfwTerminate()` | Function | GLFW lifecycle |
| `glfwCreateWindow()` | Function | Creates the OS window |
| `glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API)` | Configuration | Disables OpenGL context |
| `glfwPollEvents()` | Function | Processes pending window events |
| `glfwWindowShouldClose()` | Function | Signals the main loop to exit |
| `VULKAN_HPP_NO_STRUCT_CONSTRUCTORS` | Macro | Forces designated initialiser syntax |

### Code Walkthrough

The class holds a raw `GLFWwindow*` pointer and RAII Vulkan handles as members. The run function calls `initWindow()`, then `initVulkan()`, then enters `mainLoop()`, and finally `cleanup()`. Because RAII handles are members of the class, they are destroyed in reverse declaration order when the class is destroyed — so no explicit cleanup code is needed for Vulkan objects as long as the member declaration order respects the dependency graph (later objects declared after earlier ones).

The `GLFW_NO_API` hint must be set *before* calling `glfwCreateWindow`, not after. GLFW reads hints at window creation time.

### Common Pitfalls

- Forgetting `GLFW_NO_API` causes GLFW to create an OpenGL context that conflicts with Vulkan.
- Declaring RAII members in the wrong order causes destruction in the wrong order, triggering validation errors about objects being destroyed while still in use.
- Not calling `glfwPollEvents()` causes the window to freeze (the OS considers it unresponsive).

---

## Section 2 — Vulkan Instance (§03.00.01)

### Concepts

The Vulkan instance is the connection between your application and the Vulkan library. It stores per-application state and is the first Vulkan object you create. Everything else — physical devices, logical devices, surfaces — is enumerated or created relative to the instance.

Instance creation has two inputs: `vk::ApplicationInfo` (metadata about your application: name, version, engine name, required Vulkan API version) and `vk::InstanceCreateInfo` (which extensions and layers to enable). The `applicationInfo` feeds into driver heuristics and crash reports; it has no functional effect on Vulkan behaviour itself.

Extensions must be explicitly requested. The minimum required set comes from GLFW: the function `glfwGetRequiredInstanceExtensions` returns a list of extension names that GLFW needs to create a surface. Before requesting them, you should validate that they are actually supported by calling `context.enumerateInstanceExtensionProperties()`.

The `vk::raii::Context` is the Vulkan-Hpp RAII entry point — it wraps the loader and provides the enumeration and instance creation methods. It has no equivalent in raw Vulkan; it is a library abstraction.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::raii::Context` | Class | Library entry point; wraps the Vulkan loader |
| `vk::ApplicationInfo` | Struct | App metadata; sets required API version |
| `vk::InstanceCreateInfo` | Struct | Extensions and layers to enable at instance level |
| `vk::raii::Instance` | Class | Owns the Vulkan instance handle |
| `context.enumerateInstanceExtensionProperties()` | Method | Lists all extensions the driver supports |
| `glfwGetRequiredInstanceExtensions()` | Function | Returns extensions GLFW needs for surface creation |

### Code Walkthrough

`createInstance` populates an `ApplicationInfo` with the application name, version 1.0.0, no engine, and the target Vulkan API version (1.3 or higher). It then collects required extensions from GLFW and validates them against the supported list. The `InstanceCreateInfo` references the `ApplicationInfo` and the extension list. In debug builds, the validation layer name is also added here (see §03.00.02). Finally, `vk::raii::Instance` is constructed from the context and the create info.

### Common Pitfalls

- Requesting an extension that the driver does not support causes instance creation to fail with `VK_ERROR_EXTENSION_NOT_PRESENT`.
- Setting the API version too high (e.g., 1.4 when the driver only supports 1.3) causes instance creation to fail.
- On macOS with MoltenVK, `VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME` must be added to extensions and `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` to the instance create flags, or instance creation fails.

---

## Section 3 — Validation Layers (§03.00.02)

### Concepts

Vulkan's design philosophy prioritises driver performance over safety. By default the driver performs almost no error checking. Validation layers exist to close this gap during development: they are optional software components that intercept every Vulkan call, verify its correctness against the specification, and report any violations via a configurable callback.

The standard validation layer is `VK_LAYER_KHRONOS_validation`. It is a composite of many individual checks: parameter validation, object tracking (detecting leaks), thread safety, and best-practice warnings. It is conditionally enabled using the `NDEBUG` preprocessor macro — present in debug builds, stripped in release builds.

A debug messenger (`VkDebugUtilsMessengerEXT`) is the mechanism by which the validation layer reports problems. You configure it with a severity filter (verbose, info, warning, error) and a message type filter (general, validation, performance), and provide a static callback function that receives the message text and can decide what to do with it (typically, print to stderr).

The debug messenger itself has a lifecycle: it must be created after the instance and destroyed before the instance. With Vulkan-Hpp RAII, `vk::raii::DebugUtilsMessengerEXT` handles this automatically.

There is a subtle bootstrapping problem: validation layers can report errors during instance creation and destruction, but the debug messenger requires an existing instance. The tutorial solves this by passing a `VkDebugUtilsMessengerCreateInfoEXT` in the `pNext` chain of `VkInstanceCreateInfo` — this covers the instance creation/destruction window.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `VK_LAYER_KHRONOS_validation` | Layer name | The standard meta-validation layer |
| `vk::DebugUtilsMessengerCreateInfoEXT` | Struct | Configures severity filter, type filter, callback |
| `vk::raii::DebugUtilsMessengerEXT` | Class | Owns the debug messenger; auto-destroyed |
| `PFN_vkDebugUtilsMessengerCallbackEXT` | Function pointer type | Signature for the user callback |
| `vk::DebugUtilsMessageSeverityFlagBitsEXT` | Enum | `eVerbose`, `eInfo`, `eWarning`, `eError` |
| `vk::DebugUtilsMessageTypeFlagBitsEXT` | Enum | `eGeneral`, `eValidation`, `ePerformance` |
| `context.enumerateInstanceLayerProperties()` | Method | Lists all layers supported by the loader |
| `VK_EXT_debug_utils` | Extension name | Required to use the debug messenger |

### Code Walkthrough

A helper function builds the `DebugUtilsMessengerCreateInfoEXT` with warning and error severity enabled, validation and performance message types selected, and a static callback function as the `pfnUserCallback`. This helper is called both when populating `InstanceCreateInfo.pNext` (for coverage during instance creation) and again to create the actual `vk::raii::DebugUtilsMessengerEXT` after the instance exists.

The static callback receives four parameters: severity flags, message type flags, a pointer to `VkDebugUtilsMessengerCallbackDataEXT` (which contains `pMessage` — the human-readable error string), and a user data pointer. The callback prints the message and returns `VK_FALSE` (returning `VK_TRUE` would abort the triggering Vulkan call, which is only useful for testing the validation layer itself).

Layer availability is checked by enumerating instance layer properties and searching for `VK_LAYER_KHRONOS_validation` by name. If the layer is requested but not found, the tutorial throws an exception.

### Common Pitfalls

- Forgetting to add `VK_EXT_debug_utils` to the instance extension list — the debug messenger requires it.
- Declaring the `DebugUtilsMessengerEXT` member after the `Instance` member causes it to be destroyed after the instance, which the validation layer correctly reports as an error.
- The `pNext` bootstrapping trick is required; without it, instance creation errors are silent.

---

## Section 4 — Physical Devices and Queue Families (§03.00.03)

### Concepts

A physical device represents a GPU (or other Vulkan-capable processor) in the system. Vulkan does not pick one automatically — you enumerate all available devices and select the one that meets your requirements. This separation is intentional: in multi-GPU systems or systems where integrated and discrete GPUs coexist, your application needs to reason about which device to use.

A queue family is a group of queues on a physical device that all support the same set of operations. Queues are the mechanism through which you submit work to the GPU. Different families support different command types: graphics, compute, transfer, sparse binding, and video operations. For a simple rendering application, you need at minimum one queue that supports graphics operations — and for presentation, you also need at least one queue that can present to the window surface.

The tutorial uses four selection criteria for a suitable device:
1. Vulkan API version 1.3 or higher (checked via `physicalDevice.getProperties().apiVersion`)
2. A queue family that supports graphics operations (`vk::QueueFlagBits::eGraphics`)
3. The `VK_KHR_swapchain` device extension
4. Required features: dynamic rendering and extended dynamic state

The `isDeviceSuitable` function encapsulates all four checks. The first device that passes all checks is selected.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `instance.enumeratePhysicalDevices()` | Method | Returns all Vulkan-capable GPUs |
| `vk::raii::PhysicalDevice` | Class | Handle to a physical device |
| `physicalDevice.getProperties()` | Method | Returns name, type, Vulkan API version, limits |
| `physicalDevice.getFeatures()` | Method | Returns supported optional feature flags |
| `physicalDevice.getQueueFamilyProperties()` | Method | Returns capabilities for each queue family |
| `vk::QueueFlagBits::eGraphics` | Enum value | Bit flag indicating graphics support |
| `physicalDevice.enumerateDeviceExtensionProperties()` | Method | Lists extensions the device supports |
| `vk::PhysicalDeviceFeatures2` | Struct | Feature query struct; chained with extension features |

### Code Walkthrough

`pickPhysicalDevice` enumerates all devices and calls `isDeviceSuitable` on each. `isDeviceSuitable` first reads device properties to check the API version. It then iterates over queue family properties, looking for a family index where `queueFlags` contains `eGraphics` and `getSurfaceSupportKHR` returns true for our surface. It then enumerates device extensions to confirm `VK_KHR_swapchain` is present. Finally it chains `vk::PhysicalDeviceVulkan13Features` and `vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT` off `PhysicalDeviceFeatures2` and calls `physicalDevice.getFeatures2` to check that dynamic rendering and extended dynamic state are both supported.

The selected queue family index is stored as a member variable — it will be needed again when creating the logical device and when creating the surface.

### Common Pitfalls

- Not checking the Vulkan API version before querying features that only exist in newer versions causes undefined behaviour or crashes.
- Checking graphics support alone is not sufficient — a queue family must explicitly support presentation via `getSurfaceSupportKHR`, which is surface-dependent and cannot be determined from the queue flags alone.
- If you forget to store the queue family index and try to re-derive it later without the same device/surface context, you may get a different index.

---

## Section 5 — Logical Device and Queues (§03.00.04)

### Concepts

A logical device is your application's interface to a physical device. While the physical device represents the hardware, the logical device is where you specify which features you want to use, which extensions to enable, and how many queues of each type to create. Multiple logical devices can be created from the same physical device (though this is unusual).

Queue creation happens at logical device creation time. You specify one `vk::DeviceQueueCreateInfo` per queue family you want to use, stating how many queues from that family you need and their relative scheduling priorities (a float in [0.0, 1.0]).

Feature enablement in modern Vulkan uses a `pNext` chain of feature structs. The root is `vk::PhysicalDeviceFeatures2`; off its `pNext` you chain `vk::PhysicalDeviceVulkan13Features` (to enable dynamic rendering), `vk::PhysicalDeviceVulkan12Features`, and any extension-specific feature structs. `vk::StructureChain<>` is a Vulkan-Hpp helper template that manages this chain safely — it automatically links the structures and prevents type mismatches.

After device creation, you retrieve queue handles using `vk::raii::Queue`. The queue is not created at retrieval time — it was created with the device. You are simply getting a handle to an already-existing resource.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::DeviceQueueCreateInfo` | Struct | Specifies family index, count, and priorities |
| `vk::DeviceCreateInfo` | Struct | Aggregates queue infos, extensions, feature chain |
| `vk::raii::Device` | Class | Owns the logical device |
| `vk::StructureChain<T...>` | Template | Safely links `pNext` chains of feature structs |
| `vk::PhysicalDeviceFeatures2` | Struct | Root of the feature chain |
| `vk::PhysicalDeviceVulkan13Features` | Struct | Enables `dynamicRendering`, `synchronization2` |
| `vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT` | Struct | Enables extended dynamic state |
| `device.getQueue()` | Method | Retrieves a handle to a created queue |
| `vk::raii::Queue` | Class | Handle to a queue for submitting work |

### Code Walkthrough

`createLogicalDevice` first builds the `StructureChain`, setting `dynamicRendering = true` and `extendedDynamicState = true` in the appropriate feature structs. It then creates a single `DeviceQueueCreateInfo` referencing the stored queue family index with a priority of 1.0 and a queue count of 1. The `DeviceCreateInfo` sets `pNext` to the address of the root feature struct from the chain, lists the required extensions (just `VK_KHR_swapchain`), and references the queue create info. The `vk::raii::Device` is constructed from the physical device and this create info. Finally `device.getQueue(queueFamilyIndex, 0)` retrieves the queue handle.

Because graphics and presentation share the same queue family index in this tutorial, only one queue is needed and `imageSharingMode` on the swap chain will be `eExclusive`.

### Common Pitfalls

- Enabling a feature in the feature chain without first verifying the physical device supports it causes device creation to fail.
- Forgetting to include an extension in `ppEnabledExtensionNames` even if the physical device exposes it — the device only activates extensions you explicitly request.
- Requesting `queueCount` greater than the number of queues in that family (from `queueFamilyProperties.queueCount`) causes device creation to fail.

---

## Section 6 — Window Surface (§03.01.00)

### Concepts

Vulkan is designed to be platform-agnostic, but presenting rendered images to an OS window requires platform-specific integration. The `VkSurfaceKHR` object is the abstraction that bridges the gap: it represents a renderable surface tied to an OS window, abstracting away the platform differences behind a common interface.

The WSI (Window System Integration) extension family provides this abstraction. At the instance level, `VK_KHR_surface` is the base extension; on Windows, `VK_KHR_win32_surface` provides the platform-specific creation function. GLFW wraps all of this behind `glfwCreateWindowSurface`, which selects the correct platform extension automatically — this is why we prefer GLFW for cross-platform work.

An important subtlety: the ability of a queue family to perform graphics operations does not guarantee it can also present to a surface. These capabilities are independent. You must query each queue family explicitly using `physicalDevice.getSurfaceSupportKHR(familyIndex, surface)` to confirm it supports presentation to your specific surface. On most desktop hardware, the same queue family handles both — but this is not guaranteed by the spec.

This is why the physical device selection step (§03.00.03) must happen after surface creation: `getSurfaceSupportKHR` requires both the physical device and the surface to be valid.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `VK_KHR_surface` | Extension | Base WSI extension; required at instance level |
| `VK_KHR_win32_surface` | Extension | Windows-specific surface creation |
| `vk::raii::SurfaceKHR` | Class | RAII wrapper for the surface object |
| `glfwCreateWindowSurface()` | Function | Cross-platform surface creation via GLFW |
| `physicalDevice.getSurfaceSupportKHR()` | Method | Checks if a queue family can present to a surface |

### Code Walkthrough

`createSurface` calls `glfwCreateWindowSurface`, passing the raw Vulkan instance handle (obtained via `*instance`), the GLFW window pointer, and a null allocator. The raw `VkSurfaceKHR` returned is then wrapped in `vk::raii::SurfaceKHR`. Surface creation must happen after instance creation and before physical device selection. In the `initVulkan` sequence it therefore sits between `setupDebugMessenger` and `pickPhysicalDevice`.

The presentation support check is done inside `isDeviceSuitable` — for each queue family that supports `eGraphics`, we additionally verify `getSurfaceSupportKHR` returns true. The index of the qualifying family is stored for later use.

### Common Pitfalls

- Creating the surface before adding `VK_KHR_surface` (and the platform-specific surface extension) to the instance extension list causes `glfwCreateWindowSurface` to fail.
- Using the raw `VkSurfaceKHR` after the RAII wrapper has been destroyed is a use-after-free bug; let RAII manage the lifetime.
- Assuming that the graphics queue family also supports presentation without querying — this is implementation-defined behaviour, not a spec guarantee.

---

## Section 7 — Swap Chain (§03.01.01)

### Concepts

Vulkan has no concept of a default framebuffer. To present rendered images to the screen, you must explicitly create a swap chain — a collection of images that cycle between "being rendered to" and "being displayed". The swap chain is owned by the device and tied to a surface.

Three families of properties govern swap chain configuration:

**Surface capabilities** define hard constraints: minimum and maximum image counts, minimum and maximum image dimensions, and the set of supported usage flags and transforms.

**Surface formats** define what you can store in each image: the pixel component format (e.g., `B8G8R8A8Srgb`) and the colour space (e.g., `SrgbNonlinear`). SRGB is preferred because it matches how monitors interpret colour data and avoids perceptual banding.

**Presentation modes** define how completed frames reach the display:
- `eImmediate`: Images are shown as soon as they are ready, potentially mid-scanline (tearing).
- `eFifo`: A queue of frames; the display consumes one per vertical blank. This is the only mode guaranteed to exist on all implementations. Analogous to V-Sync.
- `eFifoRelaxed`: Like FIFO, but if the queue empties (app too slow), the next image is shown immediately (may tear).
- `eMailbox`: Like FIFO, but if the queue is full, new images replace the queued one rather than blocking. This achieves the lowest latency without tearing — sometimes called triple buffering.

The swap extent is the resolution of the swap chain images. On most platforms this must match the window framebuffer size in pixels (not screen coordinates). On high-DPI displays these differ; always query `glfwGetFramebufferSize` and clamp to the surface's min/max extent rather than using the GLFW window size in screen coordinates.

Image count is how many images the swap chain manages. The tutorial requests at least 3 (triple buffering) and clamps to the surface's maximum.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `VK_KHR_swapchain` | Device extension | Required for swap chain support |
| `vk::SwapchainCreateInfoKHR` | Struct | All swap chain configuration |
| `vk::raii::SwapchainKHR` | Class | Owns the swap chain |
| `physicalDevice.getSurfaceCapabilitiesKHR()` | Method | Returns extent, image count limits, transforms |
| `physicalDevice.getSurfaceFormatsKHR()` | Method | Returns available pixel format + colour space combos |
| `physicalDevice.getSurfacePresentModesKHR()` | Method | Returns available presentation modes |
| `swapChain.getImages()` | Method | Returns the vector of `vk::Image` handles |
| `vk::SurfaceFormatKHR` | Struct | `.format` + `.colorSpace` pair |
| `vk::PresentModeKHR` | Enum | Presentation mode selection |
| `vk::Extent2D` | Struct | Width and height in pixels |

### Code Walkthrough

`createSwapChain` begins by querying all three property categories. Three helper functions select the best option from each: `chooseSwapSurfaceFormat` scans for `B8G8R8A8Srgb` + `SrgbNonlinear` and falls back to the first available format; `chooseSwapPresentMode` prefers `eMailbox` and falls back to `eFifo`; `chooseSwapExtent` uses `currentExtent` if the surface has locked it to the window size, otherwise queries `glfwGetFramebufferSize` and clamps.

The `SwapchainCreateInfoKHR` is populated with the selected format, colour space, extent, presentation mode, and image count. Key fields: `imageArrayLayers = 1` (not stereo), `imageUsage = eColorAttachment` (we render directly to these images), `imageSharingMode = eExclusive` (one queue family owns the images), `preTransform = surfaceCapabilities.currentTransform` (no additional rotation), `compositeAlpha = eOpaque` (ignore the window alpha channel), `clipped = true` (don't store pixels obscured by other windows). `oldSwapchain` is null for initial creation but will be used during recreation (§03.04).

After construction, `swapChain.getImages()` returns the image handles. These images are owned by the swap chain — do not destroy them manually.

The selected format and extent are stored as member variables because they are needed when creating image views, the pipeline, and during rendering.

### Common Pitfalls

- Using `glfwGetWindowSize` instead of `glfwGetFramebufferSize` for the swap extent produces wrong results on high-DPI displays.
- Not clamping the requested image count to `maxImageCount` causes swap chain creation to fail (when `maxImageCount > 0`, it is a hard upper limit).
- Setting `imageUsage` to a flag the surface does not support (check `surfaceCapabilities.supportedUsageFlags`) causes creation to fail.
- Forgetting to store the format and extent as members means you must re-query them later, which is an error because the swap chain's configuration is fixed.

---

## Section 8 — Image Views (§03.01.02)

### Concepts

A `VkImage` is raw memory on the GPU. To actually use an image — to read from it in a shader, to render into it, to sample it as a texture — you need an image view: a descriptor that tells Vulkan how to interpret the image data. The image view specifies the format, the aspect (colour, depth, stencil), which mip levels are accessible, and which array layers are accessible.

Every swap chain image needs its own image view. These views are what the pipeline actually references when it writes colour output. For a standard 2D rendering setup, the view type is `e2D`, the aspect mask is `eColor`, base mip level is 0, level count is 1, base array layer is 0, and layer count is 1.

The `components` field allows channel swizzling (remapping R→G, etc.), which is useful for single-channel texture data but should be left at the identity mapping for swap chain images.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::ImageViewCreateInfo` | Struct | Configures the view: format, aspect, mip/layer range |
| `vk::raii::ImageView` | Class | Owns an image view; auto-destroyed |
| `vk::ImageViewType::e2D` | Enum | Treat the image as a 2D texture |
| `vk::ImageAspectFlagBits::eColor` | Enum | Access the colour aspect of the image |
| `vk::ComponentMapping` | Struct | Channel swizzle; use `eIdentity` for each channel |
| `vk::ImageSubresourceRange` | Struct | Selects which mip levels and array layers the view covers |

### Code Walkthrough

`createImageViews` iterates over the vector of swap chain images returned by `swapChain.getImages()`. For each image, an `ImageViewCreateInfo` is populated: the image handle, view type `e2D`, the stored swap chain format, identity component mapping, and a subresource range covering the first (and only) mip level and first (and only) array layer with `eColor` aspect. The resulting `vk::raii::ImageView` objects are stored in a member vector. The vector is sized to match the number of swap chain images.

Image views are order-sensitive: image view `i` corresponds to swap chain image `i`. This correspondence is maintained throughout the rendering code.

### Common Pitfalls

- Creating an image view with a format that doesn't match the image's actual format causes undefined behaviour.
- Accessing the image view after the swap chain has been destroyed — the swap chain owns the images, and the image views reference them. Destroy image views before the swap chain.
- Forgetting to store the image views (letting them go out of scope) causes immediate destruction — you need them to persist for the lifetime of the swap chain.

---

## Section 9 — Graphics Pipeline Introduction (§03.02.00)

### Concepts

The graphics pipeline is the sequence of stages the GPU executes to transform 3D geometry into 2D pixels. In Vulkan, the entire pipeline is described by a single immutable object (`VkPipeline`) created upfront. This is one of Vulkan's most distinctive design choices: by baking all state into a single object, the driver can fully compile and optimise the GPU program at creation time, eliminating the per-draw overhead that plagues OpenGL's implicit state machine.

The pipeline stages in order:

1. **Input Assembler** — reads raw vertex and index data from buffers, assembles primitives (triangles, lines, points).
2. **Vertex Shader** — programmable; runs once per vertex; transforms positions from model space to clip space.
3. **Tessellation** — optional programmable stages; subdivides geometry for smooth surfaces.
4. **Geometry Shader** — optional programmable stage; can emit additional primitives; rarely used on modern hardware.
5. **Rasterisation** — fixed-function; converts primitives into fragments (potential pixels), performs depth testing setup, clips to view frustum.
6. **Fragment Shader** — programmable; runs once per surviving fragment; computes final colour and depth.
7. **Colour Blending** — fixed-function; combines fragment output with existing framebuffer content (for transparency, etc.).

Green stages in the tutorial diagram are fixed-function (configured via structs). Orange stages are programmable (supplied as SPIR-V shader modules). Disabling unused programmable stages (e.g., tessellation, geometry) is valid and common.

Because the pipeline is immutable after creation, changing shaders, blending modes, or the render target format requires creating a new pipeline object. Pipeline derivatives and pipeline caches exist to make this less expensive.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::GraphicsPipelineCreateInfo` | Struct | Assembles all pipeline state into one creation struct |
| `vk::raii::Pipeline` | Class | Owns the compiled graphics pipeline |
| `vk::raii::PipelineLayout` | Class | Specifies push constants and descriptor set layouts |
| `vk::PipelineRenderingCreateInfo` | Struct | Dynamic rendering: declares colour/depth attachment formats |

---

## Section 10 — Shader Modules (§03.02.01)

### Concepts

Vulkan does not accept GLSL or HLSL shader source directly. It requires SPIR-V, a binary bytecode format designed to be compact, unambiguous, and fast for drivers to consume. You compile your shading language source to SPIR-V offline (before runtime), load the binary, and hand it to Vulkan.

The tutorial uses **Slang**, a modern C-style shading language developed by NVIDIA and now an official Khronos project. Slang compiles to SPIR-V via `slangc.exe` (bundled with the Vulkan SDK) and supports features like generics, interfaces, and automatic SPIR-V semantics that make it more ergonomic than raw GLSL.

For this chapter's triangle, two shaders are needed:

**Vertex shader** — receives a vertex index as input (since vertex data is hardcoded in the shader as a compile-time array rather than read from a buffer), computes clip-space position and a per-vertex colour output.

**Fragment shader** — receives the interpolated colour from the vertex shader as input and outputs it directly as the fragment colour. The GPU automatically interpolates per-vertex attributes across the triangle surface (Gouraud shading), which is why the corners are red, green, and blue but the interior is a smooth gradient.

Compiled SPIR-V is loaded from disk as a binary file (a `std::vector<uint32_t>`). A `vk::ShaderModuleCreateInfo` wraps this data. The shader module (`vk::raii::ShaderModule`) is a thin wrapper — it can be destroyed after pipeline creation because the pipeline bakes the shader code internally.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `slangc.exe` | Tool | Compiles Slang source to SPIR-V bytecode |
| `vk::ShaderModuleCreateInfo` | Struct | Wraps SPIR-V bytecode for module creation |
| `vk::raii::ShaderModule` | Class | Owns a SPIR-V shader module |
| `vk::PipelineShaderStageCreateInfo` | Struct | Binds a shader module to a pipeline stage |
| `vk::ShaderStageFlagBits::eVertex` | Enum | Vertex shader stage |
| `vk::ShaderStageFlagBits::eFragment` | Enum | Fragment shader stage |

### Code Walkthrough

A helper function `readFile` opens a file in binary mode, seeks to the end to determine file size, reads the entire contents into a `std::vector<char>`, and returns it. The shader module creation function casts this buffer to `uint32_t*` as required by the Vulkan API.

Two `PipelineShaderStageCreateInfo` structs are created — one for the vertex shader, one for the fragment shader — each specifying the stage flag, the shader module handle, and the entry point name (conventionally `"main"` in Slang). These two structs form a small array that is passed to the pipeline create info.

Slang compilation is added to the CMake build via `add_custom_command` + `add_custom_target`, ensuring shaders are recompiled whenever the `.slang` source changes. The output `.spv` files are placed in the build directory alongside the executable.

### Common Pitfalls

- Loading SPIR-V files with `std::ios::binary` omitted causes text-mode translation on Windows to corrupt the binary data.
- The `pName` entry point string must match exactly what the shader declares. Slang uses `main` by default for each entry point, but explicit attribute syntax is also supported.
- Shader modules can be destroyed as soon as the pipeline is created — holding onto them indefinitely wastes GPU memory.
- Forgetting to recompile shaders after editing `.slang` files — add shader compilation to the CMake build to avoid this class of error.

---

## Section 11 — Fixed Functions (§03.02.02)

### Concepts

The fixed-function stages are configured via a set of `VkPipeline*StateCreateInfo` structs, each controlling one aspect of how geometry flows through the non-programmable parts of the pipeline. Understanding each one is essential because mistakes here cause rendering artefacts that are difficult to diagnose.

**Vertex Input State** (`VkPipelineVertexInputStateCreateInfo`) describes the format of vertex data arriving from buffers: binding descriptions (stride, input rate) and attribute descriptions (location, format, offset). For this chapter the vertex data is hardcoded in the shader, so this struct is empty — no bindings, no attributes.

**Input Assembly State** (`VkPipelineInputAssemblyStateCreateInfo`) selects the primitive topology. `eTriangleList` means every three vertices form an independent triangle. `primitiveRestartEnable` allows special index values to restart primitive strips; not needed here.

**Viewport and Scissor** — The viewport defines the transformation from clip-space to window-space coordinates (including depth range). The scissor is a pixel-level clip rectangle; fragments outside it are discarded. The tutorial makes both dynamic (changeable at draw time without recreating the pipeline) by listing them in `VkPipelineDynamicStateCreateInfo`. Dynamic viewport/scissor is strongly recommended for any application where the window can resize.

**Rasterizer State** (`VkPipelineRasterizationStateCreateInfo`) controls polygon mode (fill, wireframe, point), face culling (which triangle winding order is front-facing), line width, depth bias (for shadow mapping), and depth clamping. The tutorial uses back-face culling with counter-clockwise front faces. `depthBiasEnable` is false for now; `rasterizerDiscardEnable` is false (we want fragments).

**Multisample State** (`VkPipelineMultisampleStateCreateInfo`) configures MSAA. Disabled for this chapter: `rasterizationSamples = e1`, `sampleShadingEnable = false`.

**Depth/Stencil State** — not yet configured; `nullptr` is passed. Depth testing is added in chapter 07.

**Colour Blend Attachment State** (`VkPipelineColorBlendAttachmentState`) controls how a fragment's output colour is combined with what is already in the framebuffer. Setting `blendEnable = false` means the fragment output is written directly, overwriting whatever was there. Alpha blending can be enabled here by specifying appropriate source/destination blend factors and blend operations.

**Colour Blend State** (`VkPipelineColorBlendStateCreateInfo`) is the container for all attachment blend states and controls global blend constants and the optional bitwise blend operation. `logicOpEnable = false` for normal alpha blending.

**Pipeline Layout** (`VkPipelineLayout`) declares what descriptor set layouts (for uniforms and textures) and push constants the pipeline uses. Even if the pipeline uses neither, a layout object must exist. The layout is created separately and referenced by the pipeline create info.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::PipelineVertexInputStateCreateInfo` | Struct | Vertex buffer binding/attribute descriptions |
| `vk::PipelineInputAssemblyStateCreateInfo` | Struct | Primitive topology and restart |
| `vk::PipelineViewportStateCreateInfo` | Struct | Viewport and scissor count (values set dynamically) |
| `vk::PipelineRasterizationStateCreateInfo` | Struct | Polygon mode, culling, depth bias |
| `vk::PipelineMultisampleStateCreateInfo` | Struct | MSAA configuration |
| `vk::PipelineColorBlendAttachmentState` | Struct | Per-attachment blend equation |
| `vk::PipelineColorBlendStateCreateInfo` | Struct | Global blend config; references attachment states |
| `vk::PipelineDynamicStateCreateInfo` | Struct | Lists which states are dynamic |
| `vk::PipelineLayoutCreateInfo` | Struct | Descriptor layouts and push constant ranges |
| `vk::raii::PipelineLayout` | Class | Owns the layout object |
| `vk::DynamicState::eViewport` | Enum | Marks viewport as dynamic |
| `vk::DynamicState::eScissor` | Enum | Marks scissor as dynamic |

### Code Walkthrough

The function `createGraphicsPipeline` builds each state struct in order. The vertex input state has zero bindings and zero attributes (data is in the shader). The input assembly selects `eTriangleList`. The viewport state declares one viewport and one scissor but leaves their values unset (they will be set via commands at draw time). The dynamic state struct lists `eViewport` and `eScissor`. The rasterizer enables back-face culling, sets polygon mode to `eFill`, sets line width to 1.0, and disables depth bias and clamping. The multisample state uses one sample. The colour blend attachment disables blending and writes all four channels (`eR | eG | eB | eA`). The colour blend state references this single attachment. An empty `PipelineLayoutCreateInfo` produces a valid but empty layout.

All of these structs are then assembled into the `GraphicsPipelineCreateInfo` in the next section.

### Common Pitfalls

- Forgetting to list viewport/scissor in the dynamic state but not providing values in the static structs causes pipeline creation to fail.
- Enabling back-face culling without verifying your vertex winding order causes the triangle to not render (it is considered a back face and culled).
- Setting `colorWriteMask` to zero means no colour is written — the framebuffer stays black.
- The pipeline layout must outlive the pipeline — do not destroy it while the pipeline exists.

---

## Section 12 — Dynamic Rendering (§03.02.03)

### Concepts

Traditional Vulkan (pre-1.3) required a `VkRenderPass` object to describe the attachments (colour, depth, stencil) that a pipeline would render into. This was verbose, inflexible, and created a strong coupling between the pipeline and the exact set of render targets it could use. Changing render targets meant creating a new render pass and a new pipeline.

**Dynamic rendering** (core in Vulkan 1.3, extension in 1.2) eliminates this entirely. Instead of a render pass object, you declare attachment formats once at pipeline creation time (via `vk::PipelineRenderingCreateInfo`) and then specify the actual image views at command buffer recording time (via `vk::RenderingInfo` and `vk::RenderingAttachmentInfo`). The pipeline knows the formats; it doesn't care which specific images those formats describe.

`vk::PipelineRenderingCreateInfo` must be in the `pNext` chain of `vk::GraphicsPipelineCreateInfo`. It declares the number of colour attachments and their formats. When using dynamic rendering, `renderPass` in `GraphicsPipelineCreateInfo` is set to `nullptr`.

The render pass object and framebuffer objects that the original tutorial created are completely absent from this implementation.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::PipelineRenderingCreateInfo` | Struct | Declares colour/depth/stencil attachment formats for dynamic rendering |
| `vk::RenderingAttachmentInfo` | Struct | Per-attachment info: image view, layout, load/store ops, clear value |
| `vk::RenderingInfo` | Struct | Aggregates attachments and rendering region for `beginRendering` |
| `commandBuffer.beginRendering()` | Method | Starts a dynamic rendering scope |
| `commandBuffer.endRendering()` | Method | Ends a dynamic rendering scope |

### Code Walkthrough

When building the pipeline, a `PipelineRenderingCreateInfo` is created with `colorAttachmentCount = 1` and a pointer to the stored swap chain image format. This struct is placed in the `pNext` chain of `GraphicsPipelineCreateInfo`. The `renderPass` field of the pipeline create info is `nullptr`.

At draw time in the command buffer, rather than calling `beginRenderPass`, you fill `vk::RenderingAttachmentInfo` for each attachment (specifying the image view, the `eColorAttachmentOptimal` image layout, load op `eClear` with a black clear value, and store op `eStore`), place it in a `vk::RenderingInfo` that also specifies the render area (the full swap chain extent), and call `commandBuffer.beginRendering(renderingInfo)`. Drawing commands follow, then `commandBuffer.endRendering()`.

### Common Pitfalls

- Setting `renderPass` to a non-null value while also including `PipelineRenderingCreateInfo` in pNext causes a validation error.
- Using a format in `PipelineRenderingCreateInfo` that doesn't match the image view's actual format at draw time causes a validation error.
- Forgetting to transition image layouts before `beginRendering` — the image must be in `eColorAttachmentOptimal` layout, not the presentation layout it arrives in.

---

## Section 13 — Pipeline Creation Conclusion (§03.02.04)

### Concepts

With all state structs prepared, the `vk::GraphicsPipelineCreateInfo` aggregates them. The creation function accepts an optional pipeline cache (for serialising and reusing compiled pipeline data across runs — not used initially) and can create multiple pipelines in a single call. Pipeline derivatives (`basePipelineHandle` or `basePipelineIndex`) allow creating new pipelines by inheriting from an existing one, which can speed up creation for pipelines that differ in only a few fields.

The resulting `vk::raii::Pipeline` is the compiled, ready-to-use graphics program. It can be bound in command buffers and is valid for the lifetime of the device.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::GraphicsPipelineCreateInfo` | Struct | Final assembly of all pipeline state |
| `vk::raii::Pipeline` | Class | The compiled pipeline object |
| `device.createGraphicsPipeline()` | Method | Creates a single pipeline; returns `vk::raii::Pipeline` |
| `vk::raii::PipelineCache` | Class | Optional; caches compiled shader data between runs |

### Code Walkthrough

The `GraphicsPipelineCreateInfo` references the shader stage array, all fixed-function state structs, the pipeline layout, and has `PipelineRenderingCreateInfo` in its `pNext`. `renderPass` is null. `basePipelineHandle` and `basePipelineIndex` are set to null/−1 (no derivative). The pipeline is created by calling `device.createGraphicsPipeline(nullptr, pipelineCreateInfo)` where `nullptr` is the cache. The returned pipeline is stored as a member.

The shader modules can be destroyed immediately after this call — the pipeline now owns the compiled shader internally. Destroying them ahead of time reduces peak GPU memory usage.

### Common Pitfalls

- Passing a pipeline cache that was created with a different device causes undefined behaviour.
- Destroying the pipeline layout before the pipeline is destroyed — the layout must outlive the pipeline.
- Mismatching the shader stage array count with the actual number of stage structs causes the driver to read garbage.

---

## Section 14 — Command Buffers (§03.03.00 + §03.03.01)

### Concepts

Command buffers are the mechanism by which you record a sequence of Vulkan commands (draw calls, pipeline binds, layout transitions, etc.) and then submit that sequence to a queue for GPU execution. Vulkan's command buffer model is designed for multi-threaded recording: multiple threads can build command buffers independently and all submit to the same queue.

**Command pools** are the memory allocators for command buffers. Each pool is tied to a specific queue family. Buffers allocated from a pool can only be submitted to queues of that family. The `eResetCommandBuffer` creation flag allows individual buffers to be reset and re-recorded without resetting the entire pool.

Command buffers come in two levels:
- **Primary** — can be submitted directly to a queue; can call secondary buffers.
- **Secondary** — cannot be submitted directly; are executed by primary buffers via `vkCmdExecuteCommands`. Useful for multi-threaded recording of sub-passes.

This tutorial uses only primary command buffers.

Recording begins with `commandBuffer.begin(beginInfo)`. Any previous recording is implicitly discarded. The recording ends with `commandBuffer.end()`. Between begin and end, you record every GPU command for that frame.

With dynamic rendering, the recording sequence for a draw call is:
1. Transition the swap chain image from presentation layout to colour attachment layout (pipeline barrier).
2. Fill `RenderingAttachmentInfo` with the image view, clear colour, and load/store ops.
3. Call `commandBuffer.beginRendering(renderingInfo)`.
4. Bind the graphics pipeline.
5. Set the dynamic viewport and scissor to match the swap chain extent.
6. Issue the draw call.
7. Call `commandBuffer.endRendering()`.
8. Transition the image from colour attachment layout to presentation layout (pipeline barrier).

The pipeline barrier for layout transitions uses `vk::ImageMemoryBarrier2` with `synchronization2` — specifying the source and destination pipeline stages and access masks precisely, rather than relying on the coarser `vkCmdPipelineBarrier` flags. This is the modern approach in Vulkan 1.3.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::CommandPoolCreateInfo` | Struct | Specifies queue family and creation flags |
| `vk::raii::CommandPool` | Class | Owns and allocates command buffers |
| `vk::CommandBufferAllocateInfo` | Struct | Level (primary/secondary) and count |
| `vk::raii::CommandBuffers` | Class | RAII vector of command buffers allocated from a pool |
| `commandBuffer.begin()` | Method | Starts recording |
| `commandBuffer.end()` | Method | Stops recording |
| `vk::CommandBufferBeginInfo` | Struct | One-time submit flags, inheritance info |
| `vk::ImageMemoryBarrier2` | Struct | Layout transition with explicit stage/access masks |
| `vk::DependencyInfo` | Struct | Wraps barriers for `cmdPipelineBarrier2` |
| `commandBuffer.pipelineBarrier2()` | Method | Records a synchronisation barrier |
| `commandBuffer.bindPipeline()` | Method | Selects the graphics pipeline for subsequent draws |
| `commandBuffer.setViewport()` | Method | Sets dynamic viewport |
| `commandBuffer.setScissor()` | Method | Sets dynamic scissor rectangle |
| `commandBuffer.draw()` | Method | Issues a non-indexed draw call |

### Code Walkthrough

`createCommandPool` creates a pool for the stored queue family index with `eResetCommandBuffer` set. `createCommandBuffers` allocates one primary command buffer per frame-in-flight from this pool; the results are stored in a vector.

`recordCommandBuffer(commandBuffer, imageIndex)` is called each frame. It begins the command buffer with no flags. It issues a pipeline barrier transitioning the swap chain image at `imageIndex` from `eUndefined`/`ePresentSrcKHR` layout to `eColorAttachmentOptimal`. It populates the rendering attachment info with the swap chain image view at `imageIndex`, layout `eColorAttachmentOptimal`, load op `eClear` (with `{0, 0, 0, 1}` clear colour), and store op `eStore`. It begins rendering, binds the pipeline, sets viewport and scissor to `{0, 0, swapChainExtent.width, swapChainExtent.height}`, and calls `commandBuffer.draw(3, 1, 0, 0)` — 3 vertices, 1 instance, starting at vertex 0 and instance 0. It ends rendering, then issues another barrier transitioning from `eColorAttachmentOptimal` to `ePresentSrcKHR`.

### Common Pitfalls

- Not transitioning image layout before rendering into it — the image starts in `eUndefined` and the attachment expects `eColorAttachmentOptimal`.
- Not transitioning image layout after rendering — the presentation engine expects `ePresentSrcKHR`; presenting an image in the wrong layout is undefined behaviour caught by validation.
- Submitting a command buffer that was not properly ended (missing `end()` call) causes GPU errors.
- Re-using a command buffer before it has finished executing on the GPU — the fence ensures this doesn't happen in the render loop.

---

## Section 15 — Rendering and Presentation (§03.03.02)

### Concepts

The render loop's `drawFrame` function orchestrates the per-frame sequence of CPU and GPU coordination. Understanding synchronisation is the core challenge here.

**Semaphores** synchronise operations on the GPU. They are signalled by one GPU operation and waited on by another. The CPU never waits on a semaphore — it submits work that specifies semaphore dependencies, and the GPU handles the ordering. Two semaphores are needed: one that the swap chain signals when an image is ready to render into (`imageAvailableSemaphore`), and one that the render queue signals when rendering is complete (`renderFinishedSemaphore`).

**Fences** synchronise the CPU and GPU. The CPU can wait on a fence or query its state. The draw fence is used to prevent the CPU from issuing a second frame while the GPU is still processing the first.

The five steps of `drawFrame`:

1. **Wait for fence** — `device.waitForFences(drawFence, true, UINT64_MAX)` blocks the CPU until the previous frame's GPU work is complete.
2. **Acquire image** — `swapChain.acquireNextImage(timeout, imageAvailableSemaphore, nullptr)` asks the presentation engine for the index of the next available swap chain image. This call may block briefly if no images are available. The semaphore is signalled by the presentation engine when the image is truly ready.
3. **Reset fence** — `device.resetFences(drawFence)` puts the fence back into the unsignalled state so it can be used for this frame. Note: reset happens *after* acquiring the image, not before — if acquisition fails, the fence is never reset, preventing a deadlock.
4. **Submit command buffer** — `vk::SubmitInfo2` specifies: wait on `imageAvailableSemaphore` at `eColorAttachmentOutput` stage (allowing vertex processing to proceed before the image is ready), the command buffer to execute, and signal `renderFinishedSemaphore` when complete. The fence is signalled when this submission completes.
5. **Present** — `vk::PresentInfoKHR` specifies: wait on `renderFinishedSemaphore`, then present the image at `imageIndex` to the swap chain.

The fence is created with the `eSignaled` flag so the very first frame doesn't wait forever (there was no previous frame to signal it).

Before the main loop exits, `device.waitIdle()` blocks until all GPU operations complete, ensuring no resources are destroyed while in use.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::SemaphoreCreateInfo` | Struct | Creates a binary semaphore |
| `vk::raii::Semaphore` | Class | Owns a semaphore |
| `vk::FenceCreateInfo` | Struct | Creates a fence; use `eSignaled` for first-frame safety |
| `vk::raii::Fence` | Class | Owns a fence |
| `device.waitForFences()` | Method | CPU blocks until fence is signalled |
| `device.resetFences()` | Method | Returns fence to unsignalled state |
| `swapChain.acquireNextImage()` | Method | Gets next presentable image index |
| `vk::SubmitInfo2` | Struct | Describes work submission with semaphores and command buffers |
| `queue.submit2()` | Method | Submits work to the GPU queue |
| `vk::PresentInfoKHR` | Struct | Describes what to present and when |
| `queue.presentKHR()` | Method | Presents a swap chain image |
| `device.waitIdle()` | Method | Blocks CPU until all GPU work is done |

### Code Walkthrough

`createSyncObjects` creates the semaphores and fence. The fence's `FenceCreateInfo` includes `eFenceCreateFlagBits::eSignaled` so the first `waitForFences` returns immediately. The draw loop in `mainLoop` calls `drawFrame` each iteration and `device.waitIdle()` once after the loop exits.

`drawFrame` follows the five-step sequence above. When acquiring the image, the return value is checked: `eErrorOutOfDateKHR` triggers swap chain recreation (§03.04). The submit info specifies `eColorAttachmentOutput` as the pipeline stage at which to wait for the image-available semaphore — this is the latest safe point to inject the wait, allowing vertex shading to proceed speculatively. The fence is passed to `queue.submit2` so it is signalled when the batch completes.

### Common Pitfalls

- Resetting the fence before checking acquisition success creates a deadlock: if acquisition fails and the fence was already reset, the next `waitForFences` will block forever because nothing will ever signal it.
- Choosing too early a wait stage (e.g., `eTopOfPipe`) causes the GPU to stall unnecessarily before vertex processing.
- Not creating the fence with `eSignaled` causes the first `waitForFences` to stall indefinitely.
- Forgetting `device.waitIdle()` before resource destruction causes validation errors about in-use objects being freed.

---

## Section 16 — Frames in Flight (§03.03.03)

### Concepts

The single-frame approach from §03.03.02 causes the CPU to wait for the GPU to finish each frame before starting the next. This wastes CPU time and underutilises the GPU pipeline. The goal of "frames in flight" is to allow the CPU to prepare frame N+1 while the GPU is still executing frame N.

The key insight: you cannot share synchronisation objects (semaphores, fences) or command buffers between concurrent frames — each in-flight frame needs its own. The number of simultaneous frames is controlled by `MAX_FRAMES_IN_FLIGHT`, which the tutorial sets to 2. Three or more is rarely beneficial; the latency increase outweighs the throughput gain.

The current frame index cycles from 0 to `MAX_FRAMES_IN_FLIGHT - 1` using modulo arithmetic. `drawFrame` accesses `commandBuffers[currentFrame]`, `imageAvailableSemaphores[currentFrame]`, `renderFinishedSemaphores[currentFrame]`, and `inFlightFences[currentFrame]`.

The implementation doubles all per-frame resources:
- Command buffers: allocate `MAX_FRAMES_IN_FLIGHT` from the pool
- Semaphores: one image-available and one render-finished per frame
- Fences: one per frame, all created signalled

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `MAX_FRAMES_IN_FLIGHT` | Constant | Number of frames the CPU can be ahead of the GPU |
| `currentFrame` | Member | Tracks which frame's resources to use this draw call |

### Code Walkthrough

All per-frame resource creation functions (`createCommandBuffers`, `createSyncObjects`) are updated to create `MAX_FRAMES_IN_FLIGHT` instances. `drawFrame` indexes into the vectors using `currentFrame` and increments at the end: `currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT`.

The fence/semaphore logic is unchanged — it was already per-frame-correct — only the resource multiplicity changes.

### Common Pitfalls

- Using `currentFrame` to index into swap chain images — the swap chain image index is separate from the frame index and comes from `acquireNextImage`. These two indices are independent.
- Setting `MAX_FRAMES_IN_FLIGHT` higher than the swap chain image count doesn't help and can increase input latency.
- Forgetting to update one of the per-frame vectors (e.g., command buffers but not semaphores) causes frame N+1 to clobber frame N's synchronisation objects while they are still in use.

---

## Section 17 — Swap Chain Recreation (§03.04)

### Concepts

The swap chain is tied to the surface's current dimensions and properties. When the window is resized, the surface changes and the existing swap chain becomes invalid. Vulkan communicates this in two ways:

- `vk::Result::eErrorOutOfDateKHR` — returned by `acquireNextImage` or `presentKHR` when the swap chain is definitely incompatible. The image cannot be used.
- `vk::Result::eSuboptimalKHR` — the swap chain works but doesn't match surface properties optimally. Rendering will still succeed but quality or efficiency may be degraded. This is treated as a recreation signal.

Recreation is also needed when the window is minimised (framebuffer size becomes zero). Attempting to create a zero-extent swap chain is an error. The solution is to poll `glfwGetFramebufferSize` in a loop and call `glfwWaitEvents()` until both dimensions are non-zero before proceeding.

`recreateSwapChain` calls `cleanupSwapChain`, then re-runs the swap chain, image view creation functions. Because RAII handles destruction automatically, `cleanupSwapChain` simply resets the RAII members to null/empty (or lets them be overwritten by the recreation functions directly).

GLFW resize detection uses `glfwSetFramebufferSizeCallback` — a static callback that sets a member flag (`framebufferResized`). The flag is checked in `drawFrame` after `presentKHR`, and if set, recreation is triggered. The flag approach is necessary because the GLFW resize callback and the Vulkan presentation code run at different points in the frame.

### Key API Types & Functions

| Symbol | Type | Purpose |
|--------|------|---------|
| `vk::Result::eErrorOutOfDateKHR` | Enum value | Swap chain incompatible; must recreate immediately |
| `vk::Result::eSuboptimalKHR` | Enum value | Swap chain works but should be recreated |
| `glfwSetFramebufferSizeCallback()` | Function | Registers a callback for window resize events |
| `glfwSetWindowUserPointer()` | Function | Stores a pointer retrievable inside GLFW callbacks |
| `glfwGetWindowUserPointer()` | Function | Retrieves the stored pointer (used in static callbacks) |
| `glfwGetFramebufferSize()` | Function | Returns framebuffer dimensions in pixels |
| `glfwWaitEvents()` | Function | Blocks until at least one window event arrives |

### Code Walkthrough

`recreateSwapChain` first calls `device.waitIdle()` to ensure no GPU work is in progress that references the current swap chain resources. Then it calls `cleanupSwapChain()` followed by `createSwapChain()` and `createImageViews()`. The command buffers and synchronisation objects do not need to be recreated because they are not tied to the swap chain dimensions.

In `drawFrame`, the result of `acquireNextImage` is checked: if `eErrorOutOfDateKHR`, call `recreateSwapChain()` and return early. If `eSuboptimalKHR`, continue with this frame but set `framebufferResized = true` so recreation happens at the end of the frame (after `presentKHR`). At the end of `drawFrame`, if `framebufferResized` is true (or `presentKHR` returns `eSuboptimalKHR`), call `recreateSwapChain()` and reset the flag.

The deadlock fix: the fence must be reset *after* `acquireNextImage` succeeds. If `acquireNextImage` returns `eErrorOutOfDateKHR` and we reset the fence first, we'd return early with the fence in an unsignalled state — then the next `waitForFences` would block forever. The tutorial explicitly addresses this with a careful code ordering.

### Common Pitfalls

- Not handling the minimised window case — creating a zero-extent swap chain fails with a validation error.
- Destroying image views that are still in use during recreation — always call `device.waitIdle()` first.
- Not resetting `framebufferResized` after recreation — causes unnecessary recreation every frame.
- The fence reset order issue described above — this is the most subtle deadlock risk in the entire chapter.

---

## Summary

After completing Chapter 03, the application:

- Creates a Vulkan instance with validation layers active in debug builds
- Selects a physical device that supports Vulkan 1.3+, graphics, presentation, and required features
- Creates a logical device with dynamic rendering and extended dynamic state enabled
- Creates a window surface and swap chain with triple buffering and mailbox presentation where available
- Creates an image view per swap chain image
- Loads and compiles Slang shaders to SPIR-V, creates shader modules
- Builds a complete graphics pipeline using dynamic rendering (no render pass or framebuffer objects)
- Allocates a command pool and per-frame command buffers
- Creates per-frame semaphores and fences for CPU-GPU and GPU-GPU synchronisation
- Renders a smooth RGB-shaded triangle at real-time frame rates with proper frames-in-flight management
- Handles window resize and minimisation gracefully via swap chain recreation

---

## Implementation Checklist

### Setup
- [ ] `initWindow()` — GLFW init, hints, 800×600 window, resize disabled
- [ ] `createInstance()` — ApplicationInfo, extension validation, RAII instance
- [ ] `setupDebugMessenger()` — DebugUtilsMessenger with pNext bootstrapping
- [ ] `createSurface()` — glfwCreateWindowSurface wrapped in RAII SurfaceKHR
- [ ] `pickPhysicalDevice()` — enumerate + isDeviceSuitable with all four criteria
- [ ] `createLogicalDevice()` — StructureChain features, queue, device extensions

### Presentation
- [ ] `createSwapChain()` — query caps/formats/modes, choose best, create + store format/extent
- [ ] `createImageViews()` — one ImageView per swap chain image

### Graphics Pipeline
- [ ] Slang vertex + fragment shaders written and compiling to SPIR-V
- [ ] CMake shader compilation integrated via `add_custom_command`
- [ ] `createGraphicsPipeline()` — all fixed-function structs, layout, dynamic rendering, pipeline object

### Drawing
- [ ] `createCommandPool()` — reset-capable pool for queue family
- [ ] `createCommandBuffers()` — MAX_FRAMES_IN_FLIGHT primary buffers
- [ ] `createSyncObjects()` — per-frame semaphore pairs and signalled fences
- [ ] `recordCommandBuffer()` — layout barriers, dynamic rendering, pipeline bind, draw(3,1,0,0)
- [ ] `drawFrame()` — five-step fence/acquire/record/submit/present sequence

### Swap Chain Recreation
- [ ] `framebufferResizeCallback()` registered and flag set
- [ ] `cleanupSwapChain()` / `recreateSwapChain()` implemented
- [ ] Window minimisation wait loop in recreate
- [ ] Fence reset ordering correct (after successful acquire)

---

## Further Reading

- [Vulkan Spec — VkSwapchainCreateInfoKHR](https://registry.khronos.org/vulkan/specs/latest/man/html/VkSwapchainCreateInfoKHR.html)
- [Vulkan Spec — VkGraphicsPipelineCreateInfo](https://registry.khronos.org/vulkan/specs/latest/man/html/VkGraphicsPipelineCreateInfo.html)
- [Vulkan Spec — Dynamic Rendering (VK_KHR_dynamic_rendering)](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_dynamic_rendering.html)
- [Vulkan Spec — VkSubmitInfo2](https://registry.khronos.org/vulkan/specs/latest/man/html/VkSubmitInfo2KHR.html)
- [Vulkan Synchronisation Examples — Khronos](https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples)
- [Slang Shading Language Documentation](https://shader-slang.com/slang/user-guide/)
- [Vulkan Hardware Database — gpuinfo.org](https://vulkan.gpuinfo.org/) (device feature/extension support)
