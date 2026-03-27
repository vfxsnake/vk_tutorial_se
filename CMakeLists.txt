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
add_executable(VulkanTutorial src/main.cpp src/core/VulkanContext.cpp)

target_include_directories(VulkanTutorial PRIVATE
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
    VULKAN_HPP_NO_CONSTRUCTORS      # use designated initializers
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
)