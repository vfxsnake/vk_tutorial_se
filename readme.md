# Vulkan Tutorial 2026 

My take implementing the vulkan tutorial into a well defined project, from the ground up it will be an extensible project to support the new section Building a simple Engine. 

for running it in windows, use:

rmdir /s /q build 

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 

cmake --build build --config Debug
cd \build
Debug\VulkanTutorial.exe