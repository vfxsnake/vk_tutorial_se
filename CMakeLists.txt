cmake_minimum_required(VERSION 3.29)

project(VulkanProject)

# Enable C++ module dependency scanning
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Set the build output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find all necessary packages once at the top level
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)
find_package(tinyobjloader REQUIRED)
find_package(TinyGLTF REQUIRED)
find_package(KTX REQUIRED)
find_package(stb REQUIRED)

if(WIN32)
  # The find_package(glfw3) command creates an imported target 'glfw'
  # We can inspect this target to find the path to the DLL
  get_target_property(GLFW3_LIBRARY_PATH glfw IMPORTED_LOCATION_RELEASE)
  get_filename_component(GLFW3_LIB_DIR ${GLFW3_LIBRARY_PATH} DIRECTORY)
  get_filename_component(GLFW3_BIN_DIR "${GLFW3_LIB_DIR}/../bin" ABSOLUTE)
  find_file(GLFW3_DLL_PATH NAMES glfw3.dll HINTS "${GLFW3_BIN_DIR}" REQUIRED)
endif()

# set up Vulkan C++ module
add_library(VulkanCppModule)
add_library(Vulkan::cppm ALIAS VulkanCppModule)

target_compile_definitions(VulkanCppModule
        PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
      VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)
target_include_directories(VulkanCppModule
        PRIVATE
        "${Vulkan_INCLUDE_DIR}"
)
target_link_libraries(VulkanCppModule
        PUBLIC
        Vulkan::Vulkan
)

set_target_properties(VulkanCppModule PROPERTIES CXX_STANDARD 20)

target_sources(VulkanCppModule
        PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS
        "${Vulkan_INCLUDE_DIR}"
        FILES
        "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
)


# Add the vulkan.cppm file directly as a source file
target_sources(VulkanCppModule
        PRIVATE
        "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
)

# --- Add our custom main project ---
add_executable(main_project main.cpp)

set_target_properties(main_project PROPERTIES CXX_STANDARD 20)

target_link_libraries(main_project PRIVATE
  Vulkan::cppm
  glfw
  glm::glm
  tinyobjloader::tinyobjloader
  tinygltf::tinygltf
  KTX::ktx
)

target_include_directories(main_project PRIVATE
  ${stb_INCLUDE_DIRS}
)

if(WIN32)
  add_custom_command(
    TARGET main_project POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GLFW3_DLL_PATH}"
    $<TARGET_FILE_DIR:main_project>
    COMMENT "Copying glfw3.dll to main_project output directory"
  )
endif()
