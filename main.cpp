#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <algorithm>
#include <limits>

// #include <unordered_set> // For suggested performance improvement
// #include <string_view>   // For suggested performance improvement

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

const std::vector validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif


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
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    
    // adding the surface
    vk::raii::SurfaceKHR surface = nullptr;

    // here we add the physical device instance
    vk::raii::PhysicalDevice physicalDevice = nullptr;
    // adding the logic device
    vk::raii::Device device = nullptr;
    
    // adding graphics queue 
    vk::raii::Queue graphicsQueue = nullptr;
    
    // adding presenter queue
    vk::raii::Queue presentQueue = nullptr;

    // swap chain definition
    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    vk::Format swapChainImageFormat = vk::Format::eUndefined;
    vk::Extent2D swapChainExtent;
    std::vector<vk::raii::ImageView> swapChainImageViews;

    std::vector<const char*> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName
    };
    
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
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
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

        // Get the required layers
        std::vector<char const*> requiredLayers;
        if (enableValidationLayers)
        {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        // check if the required layers are suported by the vulkan implementation.
        /*
            // Gemini Code Assist Suggestion:
            // The following code block offers a more performant and arguably more readable
            // way to check for layer support. It uses a hash set for O(1) average
            // time complexity lookups, compared to the current O(N*M) approach.
            // This can be a significant improvement if the lists of layers are long.
            // To use this, you would also need to include <unordered_set> and <string_view>.

            if (!requiredLayers.empty())
            {
                auto layerProperties = context.enumerateInstanceLayerProperties();
                std::unordered_set<std::string_view> availableLayers;
                for (const auto& prop : layerProperties)
                {
                    availableLayers.insert(prop.layerName);
                }

                for (const auto& requiredLayer : requiredLayers)
                {
                    // Note: .contains() is a C++20 feature. For C++17, use: if (availableLayers.find(requiredLayer) == availableLayers.end())
                    if (!availableLayers.contains(requiredLayer))
                    {
                        throw std::runtime_error("Required layer not supported: " + std::string(requiredLayer));
                    }
                }
            }
        */
        auto layerProperties = context.enumerateInstanceLayerProperties();
        for (auto const& requiredLayer : requiredLayers)
        {
            if (
                std::ranges::none_of(
                    layerProperties,
                    [requiredLayer](auto const& layerProperty)
                    {return strcmp(layerProperty.layerName, requiredLayer)==0;}

                )
            )
            {
                throw std::runtime_error("Required layer not supported: " + std::string(requiredLayer));
            }
        }
        
        // get required extensions
        auto requiredExtensions = getRequiredExtensions();
        // check if required extensions are suported by the Vulkan implementation
        /*
            // Gemini Code Assist Suggestion:
            // The following code block offers a more performant and arguably more readable
            // way to check for extension support. It uses a hash set for O(1) average
            // time complexity lookups, compared to the current O(N*M) approach.
            // This can be a significant improvement if the lists of extensions are long.
            // To use this, you would also need to include <unordered_set> and <string_view>.

            if (!requiredExtensions.empty())
            {
                auto extensionProperties = context.enumerateInstanceExtensionProperties();
                std::unordered_set<std::string_view> availableExtensions;
                for (const auto& prop : extensionProperties)
                {
                    availableExtensions.insert(prop.extensionName);
                }

                for (const auto& requiredExtension : requiredExtensions)
                {
                    // Note: .contains() is a C++20 feature. For C++17, use: if (availableExtensions.find(requiredExtension) == availableExtensions.end())
                    if (!availableExtensions.contains(requiredExtension))
                    {
                        throw std::runtime_error("Required extension not supported: " + std::string(requiredExtension));
                    }
                }
            }
        */
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        for (auto const& requiredExtension : requiredExtensions)
        {
            if (
                std::ranges::none_of(
                    extensionProperties,
                    [requiredExtension](auto const& extensionProperty)
                    {return strcmp(extensionProperty.extensionName, requiredExtension) == 0;}
                )
            )
            {
                throw std::runtime_error("Required extension not supported:" + std::string(requiredExtension));
            }
            
        }

        vk::InstanceCreateInfo createInfo{
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = static_cast<uint32_t>(requiredLayers.size()),
            .ppEnabledLayerNames = requiredLayers.data(),
            .enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size()),
            .ppEnabledExtensionNames = requiredExtensions.data()
        };
        instance = vk::raii::Instance(context, createInfo);
    }

    void setupDebugMessenger()
    {
        if(!enableValidationLayers)
        {
            return;
        }        

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose|
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
        );

        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlag(
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
        );

        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
            .messageSeverity = severityFlags,
            .messageType = messageTypeFlag,
            .pfnUserCallback = &debugCallback
        };

        debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }

    void createSurface()
    {
        VkSurfaceKHR _surface;
        if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
        {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);
    }


    void pickPhysicalDevice() 
    {
        std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
        const auto devIter = std::ranges::find_if(
          devices,
          [&]( auto const & device )
          {
            // Check if the device supports the Vulkan 1.3 API version
            bool supportsVulkan1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

            // Check if any of the queue families support graphics operations
            auto queueFamilies = device.getQueueFamilyProperties();

            /* Gemini Code Assist Suggestion for future development:
            // The `!!` (double negation) in the lambda below is a C-style idiom to convert a
            // value to a strict boolean (`true`/`false`). It works by first negating the result
            // of the bitwise AND (a non-zero value becomes `false`, zero becomes `true`), and then
            // negating it back (`false` becomes `true`, `true` becomes `false`).
            // A more explicit and arguably more readable C++ approach would be:
            // return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags();*/

            bool supportsGraphics = std::ranges::any_of( 
                queueFamilies, 
                []( auto const & qfp ) 
                { return !!( qfp.queueFlags & vk::QueueFlagBits::eGraphics ); } 
            );

            // Check if all required device extensions are available
            auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
            bool supportsAllRequiredExtensions = std::ranges::all_of( 
                requiredDeviceExtension,
                [&availableDeviceExtensions]( auto const & requiredDeviceExtension )
                {
                    return std::ranges::any_of( 
                        availableDeviceExtensions,
                        [requiredDeviceExtension]( auto const & availableDeviceExtension )
                        { 
                            return strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0; 
                        } 
                    );
                } 
            );

            // Gemini Code Assist Suggestion for future development:
            // The check for `extendedDynamicState` is redundant on a Vulkan 1.3 device,
            // as this feature is mandatory according to the specification. The query can be
            // simplified to only check for `dynamicRendering`, which is an optional 1.3 feature.
            //
            // Simplified code would look like this:
            // auto features = device.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features>();
            // bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering;

            auto features = device.template getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
            bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering && 
                                            features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

            return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
          } 
        );

        if ( devIter != devices.end() )
        {
            physicalDevice = *devIter;
        }
        else
        {
            throw std::runtime_error( "failed to find a suitable GPU!" );
        }
    }

    void createLogicalDevice()
    {
        // find the index of the first queue family that supports graphics
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        // get the first index into queueFamilyProperties which supports graphics
        auto graphicsQueueFamilyProperty = std::ranges::find_if(
            queueFamilyProperties,
            [](auto const & qfp)
            {
                return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
            }
        );
        
        auto graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
        
        // determine if a queue family Index that supports present
        // checking if the grahicsIndex is good enough
        auto presentIndex = physicalDevice.getSurfaceSupportKHR(graphicsIndex, *surface)
                                            ? graphicsIndex
                                            : ~0; // ~0 is an invalid value it can be declared like
                                                  // constexpr uint32_t INVALID_QUEUE_INDEX = ~0u;
        if (presentIndex == queueFamilyProperties.size())
        {
            // the graphicsIndex doesn't support present then look for another family index..
            for (size_t i = 0; i < queueFamilyProperties.size(); i++)
            {
                if (
                    (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                    physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface)
                )
                {
                    graphicsIndex = static_cast<uint32_t>(i);
                    presentIndex = graphicsIndex;
                    break;
                }
            }

            if (presentIndex == queueFamilyProperties.size())
            {
                // there's nothing like a single family index that supports both graphics and presenter, look for another
                for (size_t i = 0; i < queueFamilyProperties.size(); i++)
                {
                    if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surface))
                    {
                        presentIndex = static_cast<uint32_t>(i);
                        break;
                    }
                }
            }
        }

        if ((graphicsIndex == queueFamilyProperties.size()) || (presentIndex == queueFamilyProperties.size()))
        {
            throw std::runtime_error("could not find a queue for graphics or present -> Terminating");
        }

        // query for Vulkan 1.3 features:
        vk::StructureChain<
                vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
            > featureChain = {
                {}, // empty vk::PhysicalDeviceFeatures2
                {.dynamicRendering = true}, // vk::PhysicalDeviceVulkan13Features with dynamic rendering
                {.extendedDynamicState = true} // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT with dynamic state 
            };

        // creating the device:
        float queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
            .queueFamilyIndex = graphicsIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };

        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtension.size()),
            .ppEnabledExtensionNames = requiredDeviceExtension.data() 
        };

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
        presentQueue = vk::raii::Queue(device, presentIndex, 0);
    }

    void createSwapChain()
    {
        auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
        swapChainImageFormat = chooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(surface));
        swapChainExtent = chooseSwapExtent(surfaceCapabilities);
        auto minImageCount =std::max(3u, surfaceCapabilities.minImageCount);
        minImageCount = (
                surfaceCapabilities.maxImageCount > 0 && 
                minImageCount > surfaceCapabilities.maxImageCount
            ) ? surfaceCapabilities.maxImageCount: minImageCount;
        vk::SwapchainCreateInfoKHR swapChainCreateInfo{
            .surface = surface,
            .minImageCount = minImageCount,
            .imageFormat = swapChainImageFormat,
            .imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
            .imageExtent = swapChainExtent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surfaceCapabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = chooseSwapPresentMode(physicalDevice.getSurfacePresentModesKHR(surface)),
            .clipped = true
        };
        swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
        swapChainImages = swapChain.getImages();
    }

    static vk::Format chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        const auto formatIt = std::ranges::find_if(
            availableFormats,
            [](const auto& format)
            {
                return format.format == vk::Format::eB8G8R8A8Srgb && 
                       format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; 
            }
        );
        return formatIt != availableFormats.end() ? formatIt->format : availableFormats[0].format;
    }

    static vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
    {
        return std::ranges::any_of(
            availablePresentModes,
            [](const vk::PresentModeKHR value)
            {
                return vk::PresentModeKHR::eMailbox == value;
            }
        ) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        return {
            std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }

    std::vector<const char*> getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        
        std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers)
        {
            extensions.push_back(vk::EXTDebugUtilsExtensionName);
        }
        return extensions;
    }

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void*
    )
    {
        if (
            severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
            severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
        )
        {
            std::cerr << "validation layer: type " << to_string(type) << " msg " << pCallbackData->pMessage << "\n";
        }
        return vk::False;
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