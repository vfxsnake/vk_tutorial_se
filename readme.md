<!-- installing vkp libs to the project using the vcpkg manifest -->
vcpkg install --triplet=x64-windows --x-manifest-root=. --feature-flags=binarycaching,manifest

<!-- for building  -->
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\DEV\vcpkg\scripts\buildsystems\vcpkg.cmake -DCMAKE_PREFIX_PATH=C:\DEV\vk_tutorial_se\vcpkg_installed\x64-windows -DGLFW_DLL_DIR=C:\DEV\Vulkan-vk_tutorial_se\vcpkg_installed\x64-windows\bin -A x64

cmake --build build  // for creating the executable in bin directory

building the shaders:
slangc.exe shaders/shader.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o shaders/slang.spv