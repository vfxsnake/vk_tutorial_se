#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <memory>

#ifdef __INTELLISENSE__
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <vulkan/vk_platform.h>

#define GLFW_INCLUDE_VULKAN // this is required only for creating the windows surface
#include <GLFW/glfw3.h>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

class HelloTriangleApplication 
{
public:
    void run()
    {
        // initialize and run the entire application from initialize to clean
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    // pointer to GLFWwindow
    GLFWwindow* window = nullptr;

    vk::raii::Context context;
    vk::raii::Instance instance = nullptr;
    
    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);    
    }

    void initVulkan()
    {
        createInstance();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }

    void cleanup()
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void createInstance()
    {
        constexpr vk::ApplicationInfo appInfo{
            .pApplicationName = "Hello Triangle",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion =  VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = vk::ApiVersion14
        };

        // get required instance from GLFW
        uint32_t glfwExtensionCount = 0;
        // this will list and set the number of extensions to glfwExtensionCount.
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        // check if the required GLFW extension are supported by the Vulkan implementation
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        for (uint32_t i=0; i < glfwExtensionCount; i++)
        {
            if (
                std::ranges::none_of(
                        extensionProperties, // this is the list of propreties
                        // this will set the looking into the glfwExtension i index, 
                        // and  extensionProperty is the value looping in the inner loop of ranges::none_of               
                        [glfwExtension = glfwExtensions[i]](auto const& extensionProperty)
                        {return strcmp(extensionProperty.extensionName, glfwExtension) == 0;}
                    )
                )
            {
                throw std::runtime_error("Required GLFW extension not supported: " + std::string(glfwExtensions[i]));
            }
        }
        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,
            .enabledExtensionCount = glfwExtensionCount,
            .ppEnabledExtensionNames = glfwExtensions
        };
        instance = vk::raii::Instance(context, createInfo);
    }
};


int main()
{
    try
    {
        HelloTriangleApplication app;
        app.run();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}