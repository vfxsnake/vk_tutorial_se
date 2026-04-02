# Session Log

---

## Session 1 — 2026-03-16

**Duration:** unknown

**Covered:**
- Read and clarified all ambiguities in `CLAUDE.md`
- Agreed on workflow: Markdown → Architecture discussion → Implementation plan → Learning plan → Study → Implement → Code review → Verify
- Agreed on architecture: Option C (layer-based), folders emerge from real need chapter by chapter
- Ch00-02 files will be: `CMakeLists.txt`, `src/main.cpp`, `src/Application.h`, `src/Application.cpp`
- `core/`, `renderer/`, `resources/` folders appear from chapter 03 onward
- Produced `docs/vulkan_chapter_00-02_foundations.md`
- Produced `docs/vulkan_learning_plan_00-02_foundations.md`
- Updated `CLAUDE.md` with: combined ch00-02 note, explicit code review rules, revised study session workflow, session management instructions

**Left off:**
M1 theory — user has not yet read the chapter markdown or answered the comprehension questions.

**Next session starts at:**
M1 — user reads `docs/vulkan_chapter_00-02_foundations.md` Parts 1 & 2, reads official §00 and §01 tutorial pages, then answers the 6 comprehension questions in the learning plan. Return here to review answers or move to M2 if M1 is done.

**Open questions / notes:**
- None.

---

## Session 2 — 2026-03-17

**Duration:** unknown

**Covered:**
- M1 theory session completed in full
- Reviewed and corrected all 6 comprehension questions (Q3 inversion, Q4 portability/undefined behaviour, Q5 ImageView, Q6 RAII destruction order)
- User wrote final one-paragraph Vulkan contract summary in their own words
- M1 marked complete in progress tracker

**Left off:**
M1 complete. M2 not yet started — Windows environment checks not yet run.

**Next session starts at:**
M2 — verify four prerequisites in x64 Native Tools Command Prompt: `echo %VULKAN_SDK%`, run `vkcube.exe`, `cmake --version`, `git --version`. Once all four pass, write `CMakeLists.txt` and stub `src/main.cpp`.

**Open questions / notes:**
- None.

---

## Session 3 — 2026-03-18

**Duration:** unknown

**Covered:**
- Verified all four Windows prerequisites: VULKAN_SDK, vkcube.exe, cmake 4.1.0-rc1, git 2.50.0
- Wrote `CMakeLists.txt` (FetchContent setup) and `src/main.cpp` (smoke test)
- Fixed two bugs in main.cpp: undeclared `window` variable, wrong argument count on `vkEnumerateInstanceExtensionProperties`
- Built and ran successfully — window opened, 19 extensions reported
- M2 verification test passed. Chapter 00–02 complete.

**Left off:**
Chapter 00–02 fully complete. Ready to begin Chapter 03.

**Next session starts at:**
Chapter 03 — request the markdown for chapter 03, then proceed with the architecture discussion before the learning plan.

**Open questions / notes:**
- None.

---

## Session 4 — 2026-03-19

**Duration:** unknown

**Covered:**
- Generated `docs/vulkan_chapter_03_drawing_a_triangle.md` (18 sub-pages fetched and synthesised)
- Full architecture discussion for Chapter 03 — all structural decisions agreed
- Code style agreed and saved to `CLAUDE.md § Project-Wide Agreements` and `memory/feedback_code_style.md`
- Generated `docs/vulkan_implementation_plan_03_drawing_a_triangle.md` — ground truth for all implementation sessions
- Generated `docs/vulkan_learning_plan_03_drawing_a_triangle.md` — 7 milestones, 15 sessions

**Key decisions made this session:**
- `VulkanContext` by const reference; owns surface; non-copyable
- `Renderer` calls `pipeline.record(commandBuffer, extent, imageView)`
- Manual swap chain recreation in `Application`
- Composition over inheritance; `IRecordable` deferred
- ECS: `struct Entity { uint32_t id; }` in `scene/` layer (empty until Ch08)
- Naming: `UpperCamelCase` classes, `camelCase()` methods, `camelCase_` members, `under_score` params/locals
- Allman braces, Black-style struct init, `#pragma once`, west const, trailing return types for complex types
- `auto`: required for lambdas/structured bindings/trailing placeholders; explicit when type carries meaning; deferred for construction RHS and iterators

**Left off:**
M1 theory session not yet started — learning plan is ready, user has not begun reading.

**Next session starts at:**
M1 Theory — read §03.00.01 and §03.00.02 in `vulkan_chapter_03_drawing_a_triangle.md`, then the two corresponding tutorial pages, then answer the 7 M1 comprehension questions.

**Open questions / notes:**
- None.

---

## Session 5 — 2026-03-20

**Duration:** unknown

**Covered:**
- M1 Theory session completed in full (conversational one-question-at-a-time format)
- Q1: vk::raii::Context vs vk::raii::Instance — bootstrapping role, dispatch tables, Maya MFn analogy discussed
- Q2: Minimal driver overhead — undefined behaviour on bad inputs, not silent failure; layers run on CPU
- Q3: Validation layers enabled via ENABLE_VALIDATION_LAYERS (derived from NDEBUG), static constexpr bool
- Q4: apiVersion too high → instance creation fails (not device selection)
- Q5: Bootstrapping problem — pNext trick attaches debug messenger create info to InstanceCreateInfo to cover the creation call itself
- Q6: Callback must be static — non-static has hidden `this` parameter, incompatible with C function pointer signature
- Q7: GLFWwindow* passed in constructor for createSurface (not createInstance); surface needed before physical device selection
- User wrote one-paragraph summary; reviewed and corrected
- M1 Theory marked complete

**Left off:**
M1 Theory complete. Ready to begin M1 Session B — Implementation.

**Next session starts at:**
M1 Implementation — create folder structure, write VulkanContext.h skeleton, then VulkanContext.cpp, Application.h, Application.cpp, main.cpp per the implementation checklist.

**Open questions / notes:**
- CLAUDE.md trimmed from 42.1k to 24.3k — Tutorial Structure Reference moved to `docs/reference_tutorial_structure.md`, Chapter 02 build setup moved to `docs/reference_chapter02_build_setup.md`.

---

## Session 6 — 2026-03-23

**Duration:** unknown

**Covered:**
- Recapped M1 Implementation scope and agreed build order
- Created full folder structure: `core/`, `renderer/`, `scene/`, `utils/`, `scene/.gitkeep`
- Discussed `.gitkeep` convention (zero-byte file to track empty directories in git)
- Discussed copy/move delete semantics in depth — copy constructor vs copy assignment, move constructor vs move assignment, and why both pairs must be deleted
- Discussed `const unique_ptr` vs deleted specials — they solve different problems, work together
- Agreed on bottom-up build order: VulkanContext → SwapChain → GraphicsPipeline → Renderer → Application (saved to memory and implementation plan)
- Started `VulkanContext.h` — public section complete (constructor, deleted specials with doc comments, accessor declarations, private init method stubs)

**Left off:**
`VulkanContext.h` partially written — public section and private init stubs done, helper methods and member variables not yet written.

**Next session starts at:**
Continue `VulkanContext.h` — add private helper methods (`isDeviceSuitable`, `findQueueFamily`, `checkDeviceExtensionSupport`, `getRequiredInstanceExtensions`, `checkValidationLayerSupport`, `makeDebugMessengerCreateInfo`, `debugCallback`) and member variables (context_, instance_, debugMessenger_, surface_, physicalDevice_, device_, graphicsQueue_, queueFamilyIndex_, static constants).

**Open questions / notes:**
- `getSurface()` is declared without `const` on the return reference — check against implementation plan at next review (plan says `const vk::raii::SurfaceKHR&`).

---

## Session 7 — 2026-03-24

**Duration:** unknown

**Covered:**
- Reviewed completed `VulkanContext.h` — found and fixed four issues: `getQueue()` missing `const`, `makeDebugMessangerCreateInfo` typo, `findQueueFamily` changed from `uint32_t` to `std::optional<uint32_t>` (added `<optional>` include), `getSurface()` const confirmed correct
- Discussed `std::optional<uint32_t>` semantics — `.has_value()`, `*opt` access, why it's cleaner than `UINT32_MAX` sentinel
- Agreed to defer helper function refactor (extract to free functions in anonymous namespace) until `VulkanContext` is running and validated
- Started `VulkanContext.cpp` — includes, static member definitions, constructor body written
- Fixed `VALIDATION_LAYERS` definition — missing string quotes around `VK_LAYER_KHRONOS_validation`
- Identified that `instance_ = nullptr` initialiser is missing in the header (causes deleted default constructor error)

**Left off:**
Fixing `instance_` missing `= nullptr` in `VulkanContext.h`. `VulkanContext.cpp` has includes, static definitions, and constructor body — `createInstance` not yet written.

**Next session starts at:**
Fix `instance_ = nullptr` in `VulkanContext.h`, then write `createInstance()` in `VulkanContext.cpp`.

---

## Session 8 — 2026-03-25

**Duration:** unknown

**Covered:**
- Set up WSL2 IntelliSense build: added `CMAKE_EXPORT_COMPILE_COMMANDS ON` to CMakeLists.txt, created `build-wls/` Ninja build alongside existing `build/` Windows MSVC build
- Fixed IntelliSense false positive for designated initializers by adding `VULKAN_HPP_NO_CONSTRUCTORS` to `c_cpp_properties.json` defines
- Discussed `vk::raii::Context` as a bootstrap/loader object vs our `VulkanContext` module
- Discussed `const char**` C-style arrays and the `std::vector<T>(first, last)` range constructor
- Discussed `strcmp` return value semantics
- Wrote `createInstance()` — `ApplicationInfo`, GLFW extension gathering, extension support check with range-based loops
- Reviewed and fixed extension check: wrong `!= 0` condition corrected to `== 0` with `break`
- Discussed `std::ranges::none_of` vs nested loop vs range-based loop — user chose range-based loops
- Side discussions: windowing library abstraction (future), audio libraries (OpenAL Soft / miniaudio noted)

**Left off:**
`createInstance()` is partially complete — extension check and `ApplicationInfo` done, but validation layer block (ENABLE_VALIDATION_LAYERS check, layer names, VK_EXT_DEBUG_UTILS_EXTENSION_NAME, pNext debug messenger chain) not yet written.

**Next session starts at:**
Continue `createInstance()` — add validation layer support: call `checkValidationLayersSupport()` when enabled, add `VK_EXT_DEBUG_UTILS_EXTENSION_NAME` to extensions, set `ppEnabledLayerNames` in `create_info`, attach `makeDebugMessengerCreateInfo()` to `create_info.pNext`. Then implement `checkValidationLayersSupport()` and `getRequiredInstanceExtensions()`.

**Open questions / notes:**
- `getRequiredInstanceExtensions()` is declared in the header but not yet used — decide whether to keep extension logic inline in `createInstance` or move it to the helper.
- Future: make `VulkanContext` window-agnostic (abstract surface creation + extension gathering) — noted for "Building a Simple Engine" phase.

---

## Session 9 — 2026-03-26

**Duration:** unknown

**Covered:**
- Refactored inline extension logic into `getRequiredInstanceExtensions()` — GLFW extensions + `vk::EXTDebugUtilsExtensionName` when validation enabled
- Discussed why `static` and explicit return type (not `auto`) are used in `.cpp` definitions
- Discussed why static functions can't access member variables (`context_`)
- Redesigned helper functions: four focused statics — `getRequiredInstanceExtensions()`, `checkExtensionSupport()`, `checkValidationLayerSupport()`, dropped `getRequiredValidationLayers()` (VALIDATION_LAYERS static member is sufficient)
- `checkExtensionSupport(required, available)` and `checkValidationLayerSupport(required, available)` — both take const refs, throw on missing entry, return true on success
- Completed `createInstance()` — validation layer block added: `enabledLayerCount`, `ppEnabledLayerNames`, `debug_messenger_create_info` local + `pNext = &debug_messenger_create_info`
- Fixed lifetime bug: debug messenger create info stored as local variable before taking its address

**Left off:**
`createInstance()` is complete and reviewed. `makeDebugMessengerCreateInfo()` and `debugCallback()` not yet written.

**Next session starts at:**
Implement `makeDebugMessengerCreateInfo()` and `debugCallback()`, then `setupDebugMessenger()`.

**Open questions / notes:**
- Future: make `VulkanContext` window-agnostic — noted for "Building a Simple Engine" phase.

---

## Session 10 — 2026-03-27

**Duration:** unknown

**Covered:**
- Implemented `makeDebugMessengerCreateInfo()` — severity/type flags, `pfnUserCallback = debugCallback`
- Implemented `debugCallback()` — using `vk::` types in signature (valid with modern Vulkan-Hpp), `VKAPI_ATTR`/`VKAPI_CALL` macros, `vk::False` return
- Discussed `VKAPI_ATTR`/`VKAPI_CALL` — calling convention macros (not type-related), expand to `__stdcall` on Windows
- Discussed `VkXxx` vs `vk::Xxx` types — same values, different C++ types (plain enum vs enum class); mixing causes comparison errors
- Added `VulkanContext.cpp` to `CMakeLists.txt`
- Commented out `createSurface`, `pickPhysicalDevice`, `createLogicalDevice` calls in constructor (not yet implemented)
- Successful build: `cmake --build build-wsl` linked cleanly

**Left off:**
`makeDebugMessengerCreateInfo()` and `debugCallback()` complete and building. `setupDebugMessenger()` not yet written.

**Next session starts at:**
Implement `setupDebugMessenger()` — call `makeDebugMessengerCreateInfo()`, create `debugMessenger_` RAII handle from it.

**Open questions / notes:**
- Future: make `VulkanContext` window-agnostic — noted for "Building a Simple Engine" phase.

---

## Session 11 — 2026-03-30


**Duration:** unknown

**Covered:**
- Implemented and reviewed `setupDebugMessenger()` — `if constexpr (ENABLE_VALIDATION_LAYERS)` guard, `makeDebugMessengerCreateInfo()` + `instance_.createDebugUtilsMessengerEXT()`
- Discussed `if constexpr` vs regular `if` for compile-time constants — dead branch not compiled with `if constexpr`; early-return pattern does not work with `if constexpr`
- Fixed `createInstance()` — removed unused `GLFWwindow* window` parameter from signature (header + cpp)
- Implemented and reviewed `createSurface()` — `VK_NULL_HANDLE` init, `glfwCreateWindowSurface(*instance_, ...)`, `VK_SUCCESS` check, `vk::raii::SurfaceKHR(instance_, surface)` wrap
- Implemented and reviewed `checkDeviceExtensionSupport()` — fixed three issues: wrong extensions source (`getRequiredInstanceExtensions` → `REQUIRED_DEVICE_EXTENSIONS`), wrong device queried (`physicalDevice_` → `physical_device` param), `strcasecmp` → `strcmp`
- Build and test passed after `setupDebugMessenger()` and after `createSurface()`

**Left off:**
`checkDeviceExtensionSupport()` complete and reviewed. `findQueueFamily()` not yet written.

**Next session starts at:**
Implement `findQueueFamily()` — enumerate queue families on the physical device, find one that supports both `eGraphics` and present to `surface_`.

**Open questions / notes:**
- Future: make `VulkanContext` window-agnostic — noted for "Building a Simple Engine" phase.

---

## Session 12 — 2026-03-31

**Start time:** 07:19 EDT
**End time:** 09:13 EDT
**Duration:** 1 hour 54 minutes

**Covered:**
- Discussed `!!` (bool coercion) and `&` (bitwise AND) operators used in queue flag checks
- Implemented `findQueueFamily()` — iterates queue families, checks `eGraphics` bit + `getSurfaceSupportKHR`, returns `std::optional<uint32_t>`
- Discussed operator precedence bug (`!x >= y` vs `!(x >= y)`)
- Implemented `isDeviceSuitable()` — short-circuit checks: API version ≥ 1.4, queue family, device extensions, `dynamicRendering` + `extendedDynamicState` features via `getFeatures2` struct chain
- Explained `.template` disambiguation keyword in C++
- Implemented `pickPhysicalDevice()` — enumerates devices, stores first suitable one, throws if none found
- Added `std::cout` GPU name print via `getProperties().deviceName`

**Left off:**
`pickPhysicalDevice()` complete. `createLogicalDevice()` not yet written.

**Next session starts at:**
Implement `createLogicalDevice()` — feature chain (`PhysicalDeviceFeatures2` + `Vulkan13Features` + `ExtendedDynamicStateFeaturesEXT`), single `DeviceQueueCreateInfo`, retrieve queue via `device_.getQueue(queueFamilyIndex_, 0)`.

**Open questions / notes:**
- Future: make `VulkanContext` window-agnostic — noted for "Building a Simple Engine" phase.

---

## Session 14 — 2026-04-02

**Start time:** 07:29 EDT
**End time:** 09:22 EDT
**Duration:** 1 hour 53 minutes

**Covered:**
- Discussed move semantics for `SwapChain` vs `VulkanContext` — concluded move can be left defaulted on `SwapChain` because no other object stores a persistent `const SwapChain&` reference; `unique_ptr` ownership in `Application` is sufficient
- Discussed `KHR` suffix — Khronos extension naming convention explained
- Wrote `SwapChain.h` — constructor, deleted copy semantics, all public/private methods, member variables
- Fixed two issues in header: wrong return type on `chooseExtent` (`PresentModeKHR` → `Extent2D`), `u_int32_t` → `uint32_t`
- Discussed member initialiser list vs assignment in constructor body — references must use initialiser list, value types benefit from it too
- Started `SwapChain.cpp` — includes, constructor (member initialiser list + `create()`/`createImageViews()` calls)
- Discussed Law of Demeter and chained accessor calls — concluded Option A (chain through context accessors) is correct for this stage
- Brief discussion on production engine abstraction layers (split Instance/PhysicalDevice/Device/Surface classes)
- Wrote the three surface queries in `create()`: `getSurfaceCapabilitiesKHR`, `getSurfaceFormatsKHR`, `getSurfacePresentModesKHR`

**Left off:**
`create()` has the three surface queries written. The four `choose*` calls and `SwapchainCreateInfoKHR` not yet written.

**Next session starts at:**
Continue `create()` — call the four chooser helpers (`chooseFormat`, `choosePresentMode`, `chooseExtent`, `chooseImageCount`), then build `vk::SwapchainCreateInfoKHR` and create `swapChain_`.

**Open questions / notes:**
- Future: make `VulkanContext` window-agnostic — noted for "Building a Simple Engine" phase.

---

## Session 13 — 2026-04-01

**Start time:** 07:22 EDT
**End time:** 09:28 EDT
**Duration:** 2 hours 6 minutes

**Covered:**
- Implemented `createLogicalDevice()` — `vk::StructureChain` feature chain, `DeviceQueueCreateInfo`, `DeviceCreateInfo`, created `logicalDevice_` and retrieved `graphicsQueue_`
- Discussed `queueFamilyIndex_` not being set in `pickPhysicalDevice()` — fixed by storing `*findQueueFamily(device)` there
- Discussed double-call to `findQueueFamily()` — decided acceptable at startup, not a runtime performance concern
- Discussed `vk::StructureChain` and why `PhysicalDeviceVulkan13Features` (not `Vulkan14Features`) is used for `dynamicRendering`
- Discussed `PhysicalDeviceFeatures2` — the `2` suffix explained (pNext chain support added in revision)
- Renamed `device_` → `logicalDevice_` throughout header and cpp
- Implemented all accessor functions: `getLogicalDevice()`, `getPhysicalDevice()`, `getQueue()` (non-const), `getSurface()`, `getQueueFamilyIndex()`
- Fixed `graphics_queue_` naming inconsistency → `graphicsQueue_` (trailing underscore camelCase style)
- Fixed `getQueue()` — explained why `const` method cannot return non-const reference; made it non-const
- Clean WSL2 compile, then Windows build and run — RTX 2070 found, queue index 0, window opens, OBS warning explained (harmless)
- Knowledge check Q&A — all five questions answered correctly: const ref rationale, `vk::raii::Context` role, `if constexpr` vs `if`, `pNext` debug messenger gap, `findQueueFamily` dual check

**Left off:**
`VulkanContext` fully complete and verified on Windows. Knowledge check passed.

**Next session starts at:**
Begin `SwapChain` — write `SwapChain.h` skeleton per the implementation plan, then implement `SwapChain.cpp`.

**Open questions / notes:**
- Future: make `VulkanContext` window-agnostic — noted for "Building a Simple Engine" phase.
