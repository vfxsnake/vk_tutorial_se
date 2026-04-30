cmake_minimum_required(VERSION 3.25)
project(VulkanTutorial LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)    # adding this for vscode to recognize .pApplicationName ... notation for structs

# ── Vulkan SDK (found via VULKAN_SDK env var set by LunarG installer) ─────────
find_package(Vulkan REQUIRED)

# ── FetchContent dependencies ──────────────────────────────────────────────────
include(FetchContent)

# GLFW — window creation
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM — linear algebra (header-only)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# Vulkan-Hpp — C++ bindings with RAII (header-only, matches installed SDK version)
FetchContent_Declare(
    VulkanHpp
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Hpp.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(VulkanHpp)

# stb — image loading (header-only)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# tinyobjloader — OBJ model loading (header-only)
FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        release
)
FetchContent_MakeAvailable(tinyobjloader)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(VulkanTutorial
    src/main.cpp
    src/Application.cpp
    src/core/VulkanContext.cpp
    src/core/SwapChain.cpp
    src/renderer/GraphicsPipeline.cpp
    src/renderer/Renderer.cpp
    src/renderer/buffers/Mesh.cpp
    src/renderer/descriptors/FrameDescriptorLayout.cpp
)

target_include_directories(VulkanTutorial PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${stb_SOURCE_DIR}
    ${tinyobjloader_SOURCE_DIR}
)

target_link_libraries(VulkanTutorial PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    Vulkan::Headers          # provides vulkan/vulkan.hpp via the SDK
)

# Vulkan-Hpp RAII and C++20 modules flags
target_compile_definitions(VulkanTutorial PRIVATE
    VULKAN_HPP_NO_CONSTRUCTORS      # use designated initializers {.pValue = ..., .aVariable = ...}
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)

# Shaders block
if(WIN32)
    set(SLANG_C $ENV{VULKAN_SDK}/Bin/slangc.exe)
else()
    set(SLANG_C $ENV{VULKAN_SDK}/bin/slangc)
endif()

set(SHADERS_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_SOURCES ${SHADERS_DIR}/triangle.slang)
set(ENTRY_POINTS -entry vertMain -entry fragMain)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/shaders/triangle.spv 
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
    COMMAND ${SLANG_C} ${SHADER_SOURCES} -target spirv -profile spirv_1_4+spvRayQueryKHR -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${CMAKE_BINARY_DIR}/shaders/triangle.spv 
    WORKING_DIRECTORY ${SHADERS_DIR}
    DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
    COMMENT "compiling slang shaders"
    VERBATIM
)

add_custom_target(Shaders DEPENDS ${CMAKE_BINARY_DIR}/shaders/triangle.spv)
add_dependencies(VulkanTutorial Shaders)