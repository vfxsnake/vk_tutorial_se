// smoke test

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>

int main()
{
    // initialize window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow *window = glfwCreateWindow(800, 600, "Vulkan Smoke Test", nullptr, nullptr);

    // querying extension:
    uint32_t extension_count = 0;

    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

    std::cout << "extension count: " << extension_count << "\n";


    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

}