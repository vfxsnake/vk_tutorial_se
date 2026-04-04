// smoke test

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>

#include "core/VulkanContext.h"
#include "core/SwapChain.h"


int main()
{
    // initialize window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow *window = glfwCreateWindow(800, 600, "Vulkan Smoke Test", nullptr, nullptr);

    VulkanContext context = VulkanContext(window);
    SwapChain swap_chain = SwapChain(context, window);

    std::cout << "swap chain successfully created: \n" 
              << "\t Format: " << vk::to_string(swap_chain.getFormat()) << "\n"
              << "\t Extent: " << swap_chain.getExtent().width << ", " << swap_chain.getExtent().height << "\n"
              << "\t Image Count: " << swap_chain.getImageCount() << "\n";

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

}