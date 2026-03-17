# Learning Plan — Chapter 00-02: Foundations

> **Estimated total time:** 4 sessions × ~1.5 hours = ~6 hours
> **Prerequisites:** None — this is the starting point.
> **Goal:** A working, verified Vulkan build environment. A blank window opens, the
> Vulkan instance initialises, available extensions are printed to stdout, and validation
> layers run silently. Chapter 03 can begin without any environment interruption.

---

## Chapter Milestones Overview

| # | Milestone | Session Type | Est. Time |
|---|-----------|--------------|-----------|
| M1 | Why Vulkan & The Rendering Mental Model | Theory | ~1.5h |
| M2 | Build Environment | Implementation | ~1h |
| M3 | Application Skeleton & Window | Implementation | ~1.5h |
| M4 | Vulkan Instance & Validation Layers | Implementation | ~2h |

---

## Milestone M1 — Why Vulkan & The Rendering Mental Model

> **Session type:** Theory
> **Estimated time:** ~1.5 hours
> **Goal:** Understand the contract Vulkan makes with the developer — why it is explicit,
> what that costs, and what it returns. Be able to trace the full 8-step bootstrap sequence
> from memory and explain the role of each step.
> **Source:** §00 Introduction | §01 Overview
> **Docs:** `docs/vulkan_chapter_00-02_foundations.md` — Parts 1 and 2

### Session Checklist

- [ ] Read Part 1 (Introduction) of the chapter markdown in full
- [ ] Read Part 2 (Overview) of the chapter markdown in full
- [ ] Read §00 and §01 of the official tutorial at https://docs.vulkan.org/tutorial/latest/
- [ ] Answer all Comprehension Questions below — write your answers, do not just read
- [ ] Write a one-paragraph summary of "what Vulkan's contract with the developer is" in your own words

---

### Comprehension Questions

Answer each question by consulting the tutorial sections listed. Do not guess — look it up.

---

**Awareness — Why does this exist?**

**Q1.** The Overview states: *"The problem with most of these APIs is that the era in which they were designed featured graphics hardware mostly limited to configurable fixed functionality."*
Read the Overview's "Why Vulkan exists" section and answer:
- What specifically changed about GPU hardware over time that made old API designs a liability?
- What did GPU drivers have to do to compensate for this mismatch?
- Why was that driver behaviour problematic for developers?

*→ Hint: §01 Overview, opening paragraphs before "Step 1"*

---

**Q2.** The Introduction states: *"every detail related to the graphics API needs to be set up by your application, including initial frame buffer creation and memory management."*
This sounds like more work — and it is. What does Vulkan give you in return?
Name **two concrete benefits** the tutorial explicitly states you gain from this verbosity.

*→ Hint: §00 Introduction, first three paragraphs*

---

**Conceptual — What is it and how does it work?**

**Q3.** The Overview lists 8 steps to render a triangle. Choose **any two consecutive steps**
(e.g. Step 2 and Step 3, or Step 5 and Step 6) and explain in your own words:
- What does each step produce?
- Why can't those two steps be merged into one?
- What does the separation give the developer?

*→ Hint: §01 Overview, Steps 1–8*

---

**Q4.** Validation layers are described as *"pieces of code that can be inserted between
the API and the graphics driver."* This is fundamentally different from the driver doing
the same checks.
- Why does Vulkan put this choice in the developer's hands instead of always validating?
- What would be the cost of always-on validation in a shipped application?
- If you ran your application without validation layers during development, what category
  of bugs would be hardest to catch?

*→ Hint: §01 Overview, "Validation layers" section*

---

**Dependency / flow — How does information arrive here from earlier work?**

**Q5.** The swap chain (Step 3) is described as *"a collection of render targets."*
Without looking at any code, trace this dependency forward:
- Why must the swap chain exist before you can record draw commands (Step 7)?
- What specific thing does a command buffer reference that originates from the swap chain?
- What is the name of the object that sits between a raw swap chain image and the pipeline
  that renders into it?

*→ Hint: §01 Overview, Steps 3, 4, and 7*

---

**Q6.** The implementation plan specifies that in the `Application` class, `context_` must
be declared **before** `instance_`, and `instance_` **before** `debugMessenger_`.
The reason is C++ RAII destruction order (members are destroyed in reverse declaration order).

Without looking at any code, describe in plain language:
- What would happen at program shutdown if `instance_` were declared before `context_`?
- What Vulkan rule would that violate?
- Which of the three members has no Vulkan parent object at all — and why does that make
  it the natural first member?

*→ Hint: Chapter markdown Part 2, "API usage pattern" section + your knowledge of RAII*

---

## Milestone M2 — Build Environment

> **Session type:** Implementation
> **Estimated time:** ~1 hour
> **Goal:** CMake configures and builds a minimal project. The Vulkan SDK is verified.
> All dependencies are fetched automatically. A stub `main()` compiles and links.
> **Depends on:** M1 complete. All M1 questions answered before starting.
> **Source:** `docs/vulkan_chapter_00-02_foundations.md` — Part 3

### Session Checklist

- [ ] M1 theory session complete and all questions answered in writing
- [ ] Understand what each FetchContent dependency is for before writing CMakeLists.txt
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests for this milestone pass

---

### Implementation Checklist

- [ ] `VULKAN_SDK` environment variable confirmed in x64 Native Tools Command Prompt (`echo %VULKAN_SDK%`)
- [ ] `vkcube.exe` runs from `%VULKAN_SDK%\Bin` — a spinning cube appears
- [ ] `cmake --version` returns a version in the x64 Native Tools prompt
- [ ] `git --version` returns a version in the x64 Native Tools prompt
- [ ] `CMakeLists.txt` written at project root with:
  - `cmake_minimum_required(VERSION 3.25)` and `project(VulkanTutorial LANGUAGES CXX)`
  - `CMAKE_CXX_STANDARD 20` enforced
  - `find_package(Vulkan REQUIRED)`
  - FetchContent declarations for: GLFW (3.4), GLM (1.0.1), Vulkan-Hpp (main), stb (master), tinyobjloader (release)
  - `add_executable(VulkanTutorial src/main.cpp)`
  - `target_include_directories` for stb and tinyobjloader source dirs
  - `target_link_libraries` for Vulkan::Vulkan, glfw, glm::glm, Vulkan::Headers
  - `target_compile_definitions` for `VULKAN_HPP_NO_CONSTRUCTORS`, `GLM_FORCE_RADIANS`, `GLM_FORCE_DEPTH_ZERO_TO_ONE`
- [ ] `src/main.cpp` stub created (minimal `int main() { return 0; }`)
- [ ] `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` completes without errors
- [ ] `cmake --build build --config Debug` compiles and links successfully

---

### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|---|---|---|---|
| T1 | VULKAN_SDK is set | `echo %VULKAN_SDK%` in x64 Native Tools prompt | A valid path, e.g. `C:\VulkanSDK\1.4.x.x` | [ ] |
| T2 | Vulkan SDK works | Run `%VULKAN_SDK%\Bin\vkcube.exe` | A window with a spinning textured cube | [ ] |
| T3 | CMake configure succeeds | Run cmake configure command above | No errors; `build/` folder created with `.sln` inside | [ ] |
| T4 | Build succeeds | Run cmake build command above | `build\Debug\VulkanTutorial.exe` exists | [ ] |
| T5 | Executable runs | Run `.\build\Debug\VulkanTutorial.exe` | Process exits cleanly with code 0 (no window yet) | [ ] |

---

## Milestone M3 — Application Skeleton & Window

> **Session type:** Implementation
> **Estimated time:** ~1.5 hours
> **Goal:** The `Application` class exists with the correct structure. GLFW is initialised,
> a window opens, and the main loop runs until the window is closed. No Vulkan yet.
> **Depends on:** M2 complete. All M2 verification tests pass.
> **Source:** `docs/vulkan_chapter_00-02_foundations.md` — Part 3, Implementation Plan

### Session Checklist

- [ ] M2 complete — CMake builds successfully
- [ ] Re-read the Implementation Plan's member declaration order and understand *why* before writing `Application.h`
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests for this milestone pass

---

### Implementation Checklist

- [ ] `src/Application.h` written with:
  - Class declaration for `Application`
  - Constants at file scope: `WIDTH`, `HEIGHT`, `validationLayers`, `enableValidationLayers` (with `NDEBUG` guard)
  - Public: default constructor, default destructor, `void run()`
  - Private methods: `initWindow()`, `initVulkan()`, `mainLoop()`, `createInstance()`, `setupDebugMessenger()`, `checkValidationLayerSupport()`, `getRequiredExtensions()`, `debugCallback()` (static)
  - Private members in correct order: `window_`, `context_`, `instance_`, `debugMessenger_`
- [ ] `src/Application.cpp` written with:
  - `run()` calling `initWindow()`, `initVulkan()`, `mainLoop()` in sequence
  - `initWindow()` calling `glfwInit()`, `glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API)`, `glfwCreateWindow()`
  - `mainLoop()` looping on `!glfwWindowShouldClose(window_)` with `glfwPollEvents()`
  - `initVulkan()` as an empty stub for now (filled in M4)
- [ ] `src/main.cpp` updated:
  - Constructs `Application`, calls `run()`, wraps in `try/catch(std::exception&)`, prints error to stderr, returns 1 on failure
- [ ] Project rebuilds cleanly after changes

---

### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|---|---|---|---|
| T1 | Window appears | Run `VulkanTutorial.exe` | An 800×600 blank window titled with your chosen name | [ ] |
| T2 | Main loop runs | Leave window open for a few seconds | Window remains open, does not freeze or crash | [ ] |
| T3 | Clean shutdown | Click the window's X button | Window closes, process exits cleanly (no crash, no hang) | [ ] |
| T4 | Build is clean | Rebuild from scratch | Zero compiler errors or warnings | [ ] |

---

## Milestone M4 — Vulkan Instance & Validation Layers

> **Session type:** Implementation
> **Estimated time:** ~2 hours
> **Goal:** Vulkan initialises successfully. The instance is created with the correct
> extensions and validation layers. The debug messenger is active. Extension count is
> printed to stdout. Validation layers run silently (no errors on stderr).
> **Depends on:** M3 complete. Window opens and closes cleanly before starting this milestone.
> **Source:** §03.00.01 Instance | §03.00.02 Validation Layers (preview of ch03 concepts,
> applied here to the smoke test)

### Session Checklist

- [ ] M3 complete — blank window opens and closes cleanly
- [ ] Read §03.00.01 (Instance) and §03.00.02 (Validation Layers) of the tutorial in full
  before writing any code — these sections explain *why* each field in the structs exists
- [ ] Understand what `vk::raii::Context` does before constructing it (it loads Vulkan
  function pointers — it has no arguments)
- [ ] All items in the Implementation Checklist below are complete
- [ ] All Verification Tests pass

---

### Implementation Checklist

- [ ] `checkValidationLayerSupport()` implemented:
  - Calls `context_.enumerateInstanceLayerProperties()`
  - Checks that every name in `validationLayers` appears in the result
  - Returns `false` (and should log a warning) if any requested layer is missing
- [ ] `getRequiredExtensions()` implemented:
  - Queries GLFW: `glfwGetRequiredInstanceExtensions()`
  - Appends `VK_EXT_DEBUG_UTILS_EXTENSION_NAME` when `enableValidationLayers` is true
  - Returns the combined list as `std::vector<const char*>`
- [ ] `debugCallback()` static method implemented:
  - Signature matches `PFN_vkDebugUtilsMessengerCallbackEXT`
  - Prints message severity prefix and `pCallbackData->pMessage` to `std::cerr`
  - Returns `VK_FALSE`
- [ ] `createInstance()` implemented:
  - Fills `vk::ApplicationInfo` with app name, engine name, `VK_API_VERSION_1_4`
  - Calls `getRequiredExtensions()` to get extension list
  - If `enableValidationLayers`: calls `checkValidationLayerSupport()`, throws if false; adds layer names to `InstanceCreateInfo`
  - Constructs `instance_` via `vk::raii::Instance(context_, createInfo)`
  - Prints available extension count to stdout (enumerate via `context_.enumerateInstanceExtensionProperties()`)
- [ ] `setupDebugMessenger()` implemented:
  - Returns immediately if `!enableValidationLayers`
  - Fills `vk::DebugUtilsMessengerCreateInfoEXT` with all severity flags and all message type flags, and the `debugCallback` function pointer
  - Constructs `debugMessenger_` via `instance_.createDebugUtilsMessengerEXT(createInfo)`
- [ ] `initVulkan()` updated to call `createInstance()` then `setupDebugMessenger()`

---

### Verification Tests

| # | What to test | How to test | Expected output | Pass? |
|---|---|---|---|---|
| T1 | Extension count printed | Run `VulkanTutorial.exe`, check stdout | A line such as `7 extensions supported` (exact count varies by system) | [ ] |
| T2 | Window still opens | Run `VulkanTutorial.exe` | 800×600 window appears as before | [ ] |
| T3 | No validation errors | Run with validation layers enabled (Debug build), check stderr | No output on stderr during normal window open → close | [ ] |
| T4 | Validation layer missing | Temporarily request a non-existent layer (e.g. `"VK_LAYER_DOES_NOT_EXIST"`), run, then revert | `checkValidationLayerSupport()` returns false; application throws or prints an error before crashing | [ ] |
| T5 | Debug callback fires | Deliberately pass an invalid struct to any Vulkan call (e.g. set `apiVersion` to `0`), run, then revert | Validation layer prints a message to stderr via your `debugCallback` | [ ] |
| T6 | Clean shutdown | Close the window | Application exits cleanly; no Vulkan errors on shutdown | [ ] |

---

## Chapter Progress Tracker

Use this table to track your overall progress.

| Milestone | Theory ✓ | Impl ✓ | Tests Pass ✓ |
|-----------|----------|--------|--------------|
| M1 — Why Vulkan & Mental Model | [ ] | N/A | N/A |
| M2 — Build Environment | N/A | [ ] | [ ] |
| M3 — Application Skeleton & Window | N/A | [ ] | [ ] |
| M4 — Vulkan Instance & Validation Layers | N/A | [ ] | [ ] |

**Chapter complete when all rows are fully ticked.**

---

## Session Log

Fill this in after each session.

| Session # | Date | Milestone(s) | What was covered | Blockers / open questions |
|-----------|------|--------------|------------------|--------------------------|
| 1 | | M1 | | |
| 2 | | M2 | | |
| 3 | | M3 | | |
| 4 | | M4 | | |
