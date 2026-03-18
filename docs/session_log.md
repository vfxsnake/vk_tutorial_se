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
