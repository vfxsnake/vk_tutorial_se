# Tutorial Structure Reference

The full tutorial is located at `https://docs.vulkan.org/tutorial/latest/` and contains the following pages. Always use these exact URLs when fetching content.

## Main Tutorial

> **Chapters 00, 01, 02 are combined into a single chapter** for this study system.
> They are produced as one markdown doc (`vulkan_chapter_00-02_foundations.md`) and one learning plan.
> The combined goal is a verified, working build environment ready to start chapter 03 without interruption.

| # | Chapter | URL Path |
|---|---------|----------|
| 00 | Introduction | `00_Introduction.html` |
| 01 | Overview | `01_Overview.html` |
| 02 | Development Environment | `02_Development_environment.html` |
| **03** | **Drawing a Triangle** | |
| 03.00.00 | Setup → Base Code | `03_Drawing_a_triangle/00_Setup/00_Base_code.html` |
| 03.00.01 | Setup → Instance | `03_Drawing_a_triangle/00_Setup/01_Instance.html` |
| 03.00.02 | Setup → Validation Layers | `03_Drawing_a_triangle/00_Setup/02_Validation_layers.html` |
| 03.00.03 | Setup → Physical Devices & Queue Families | `03_Drawing_a_triangle/00_Setup/03_Physical_devices_and_queue_families.html` |
| 03.00.04 | Setup → Logical Device & Queues | `03_Drawing_a_triangle/00_Setup/04_Logical_device_and_queues.html` |
| 03.01.00 | Presentation → Window Surface | `03_Drawing_a_triangle/01_Presentation/00_Window_surface.html` |
| 03.01.01 | Presentation → Swap Chain | `03_Drawing_a_triangle/01_Presentation/01_Swap_chain.html` |
| 03.01.02 | Presentation → Image Views | `03_Drawing_a_triangle/01_Presentation/02_Image_views.html` |
| 03.02.00 | Graphics Pipeline → Introduction | `03_Drawing_a_triangle/02_Graphics_pipeline_basics/00_Introduction.html` |
| 03.02.01 | Graphics Pipeline → Shader Modules | `03_Drawing_a_triangle/02_Graphics_pipeline_basics/01_Shader_modules.html` |
| 03.02.02 | Graphics Pipeline → Fixed Functions | `03_Drawing_a_triangle/02_Graphics_pipeline_basics/02_Fixed_functions.html` |
| 03.02.03 | Graphics Pipeline → Render Passes | `03_Drawing_a_triangle/02_Graphics_pipeline_basics/03_Render_passes.html` |
| 03.02.04 | Graphics Pipeline → Conclusion | `03_Drawing_a_triangle/02_Graphics_pipeline_basics/04_Conclusion.html` |
| 03.03.00 | Drawing → Framebuffers | `03_Drawing_a_triangle/03_Drawing/00_Framebuffers.html` |
| 03.03.01 | Drawing → Command Buffers | `03_Drawing_a_triangle/03_Drawing/01_Command_buffers.html` |
| 03.03.02 | Drawing → Rendering & Presentation | `03_Drawing_a_triangle/03_Drawing/02_Rendering_and_presentation.html` |
| 03.03.03 | Drawing → Frames in Flight | `03_Drawing_a_triangle/03_Drawing/03_Frames_in_flight.html` |
| 03.04 | Swap Chain Recreation | `03_Drawing_a_triangle/04_Swap_chain_recreation.html` |
| **04** | **Vertex Buffers** | |
| 04.00 | Vertex Input Description | `04_Vertex_buffers/00_Vertex_input_description.html` |
| 04.01 | Vertex Buffer Creation | `04_Vertex_buffers/01_Vertex_buffer_creation.html` |
| 04.02 | Staging Buffer | `04_Vertex_buffers/02_Staging_buffer.html` |
| 04.03 | Index Buffer | `04_Vertex_buffers/03_Index_buffer.html` |
| **05** | **Uniform Buffers** | |
| 05.00 | Descriptor Layout & Buffer | `05_Uniform_buffers/00_Descriptor_set_layout_and_buffer.html` |
| 05.01 | Descriptor Pool & Sets | `05_Uniform_buffers/01_Descriptor_pool_and_sets.html` |
| **06** | **Texture Mapping** | |
| 06.00 | Images | `06_Texture_mapping/00_Images.html` |
| 06.01 | Image View & Sampler | `06_Texture_mapping/01_Image_view_and_sampler.html` |
| 06.02 | Combined Image Sampler | `06_Texture_mapping/02_Combined_image_sampler.html` |
| 07 | Depth Buffering | `07_Depth_buffering.html` |
| 08 | Loading Models | `08_Loading_models.html` |
| 09 | Generating Mipmaps | `09_Generating_Mipmaps.html` |
| 10 | Multisampling | `10_Multisampling.html` |
| 11 | Compute Shader | `11_Compute_Shader.html` |
| 12 | Ecosystem Utilities & GPU Compatibility | `12_Ecosystem_Utilities_and_Compatibility.html` |
| 13 | Vulkan Profiles | `13_Vulkan_Profiles.html` |
| 14 | Android | `14_Android.html` |
| 15 | Migrating to glTF & KTX2 | `15_GLTF_KTX2_Migration.html` |
| 16 | Rendering Multiple Objects | `16_Multiple_Objects.html` |
| 17 | Multithreading | `17_Multithreading.html` |
| **18** | **Ray Tracing** | |
| 18.00 | Overview | `courses/18_Ray_tracing/00_Overview.html` |
| 18.01 | Dynamic Rendering | `courses/18_Ray_tracing/01_Dynamic_rendering.html` |
| 18.02 | Acceleration Structures | `courses/18_Ray_tracing/02_Acceleration_structures.html` |
| 18.03 | Ray Query Shadows | `courses/18_Ray_tracing/03_Ray_query_shadows.html` |
| 18.04 | TLAS Animation | `courses/18_Ray_tracing/04_TLAS_animation.html` |
| 18.05 | Shadow Transparency | `courses/18_Ray_tracing/05_Shadow_transparency.html` |
| 18.06 | Reflections | `courses/18_Ray_tracing/06_Reflections.html` |
| 18.07 | Conclusion | `courses/18_Ray_tracing/07_Conclusion.html` |
| 90 | FAQ | `90_FAQ.html` |

## Building a Simple Engine
All paths are under `Building_a_Simple_Engine/`

| Section | Sub-pages |
|---------|-----------|
| Introduction | `introduction.html` |
| Engine Architecture | `Engine_Architecture/01_introduction.html` → `conclusion.html` (7 pages) |
| Camera Transformations | `Camera_Transformations/01_introduction.html` → `06_conclusion.html` (6 pages) |
| Lighting & Materials | `Lighting_Materials/01_introduction.html` → `06_conclusion.html` (7 pages) |
| GUI | `GUI/01_introduction.html` → `06_conclusion.html` (6 pages) |
| Loading Models | `Loading_Models/01_introduction.html` → `09_conclusion.html` (9 pages) |
| Subsystems | `Subsystems/01_introduction.html` → `06_conclusion.html` (6 pages) |
| Tooling | `Tooling/01_introduction.html` → `07_conclusion.html` (7 pages) |
| Mobile Development | `Mobile_Development/01_introduction.html` → `06_conclusion.html` (6 pages) |
| Advanced Topics | `Advanced_Topics/01_introduction.html` + 18 additional topic pages |
| Appendix | `Appendix/appendix.html` |
