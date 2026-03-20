# Session Log

---

## Session 1 — 2026-03-16

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
