#include "VulkanContext.h"
#include <stdexcept>
#include <iostream>
#include <cstring>

const std::vector<const char*> VulkanContext::REQUIRED_DEVICE_EXTENSIONS = {vk::KHRSwapchainExtensionName};
const std::vector<const char*> VulkanContext::VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};

VulkanContext::VulkanContext(GLFWwindow* window)
{
    createInstance(window);
    setupDebugMessenger();
    createSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
}

void VulkanContext::createInstance(GLFWwindow* window)
{
    constexpr vk::ApplicationInfo app_info{
        .pApplicationName = "Vulkan Engine",
        .applicationVersion = VK_MAKE_VERSION(1,0,0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1,0,0),
        .apiVersion = vk::ApiVersion14
    };

    // geting required extesnions
    const std::vector<const char*> required_extensions = getRequiredInstanceExtensions();  
    std::vector<vk::ExtensionProperties> extension_properties = context_.enumerateInstanceExtensionProperties();

     std::vector<vk::LayerProperties> layer_properties = context_.enumerateInstanceLayerProperties();

    // checking extensions and validation layers support
    checkExtensionSupport(required_extensions, extension_properties);
    
    if (ENABLE_VALIDATION_LAYERS)
    {
        checkValidationLayersSupport(VALIDATION_LAYERS, layer_properties);
    }
    
    vk::InstanceCreateInfo create_info{
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = static_cast<uint32_t>(required_extensions.size()),
        .ppEnabledExtensionNames = required_extensions.data()
    };

    if (ENABLE_VALIDATION_LAYERS)
    {
        create_info.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = makeDebugMessengerCreateInfo();
        create_info.pNext = &debug_messenger_create_info;
    }

    instance_ = vk::raii::Instance(context_, create_info);
    
}

std::vector<const char*> VulkanContext::getRequiredInstanceExtensions()
{
    uint32_t extension_count = 0;
    const char** extension_array = glfwGetRequiredInstanceExtensions(&extension_count); // const char** is a c style array.
    // converting const char* array into a vector using constructor std::vector<T>(first, last)
    std::vector<const char*> extensions(extension_array, extension_array + extension_count); 
    
    if (ENABLE_VALIDATION_LAYERS)
    {
        extensions.push_back(vk::EXTDebugUtilsExtensionName);
    }
    
    return extensions;
}

bool VulkanContext::checkExtensionSupport(
    const std::vector<const char*>& required_extensions, 
    const std::vector<vk::ExtensionProperties>& extension_properties
)
{
    // checking that extensions match the extension property names
    for (auto required_extension : required_extensions)
    {
        bool extensions_found = false;
        for (const auto &extension_property : extension_properties)
        {
            if (strcmp(required_extension, extension_property.extensionName) == 0)
            {
                extensions_found = true;
                break;
            }    
        }

        if (!extensions_found)
        {
            throw std::runtime_error("required extension not supported: " + std::string(required_extension));
        }
    }

    return true;
}

bool VulkanContext::checkValidationLayersSupport(
    const std::vector<const char*>& required_layers, 
    const std::vector<vk::LayerProperties>& validation_layer_properties
)
{
    // checking that validation layers match the validation layer property names
    for (auto required_layer : required_layers)
    {
        bool validation_layer_found = false;
        for (const auto &validation_layer_property : validation_layer_properties)
        {
            if (strcmp(required_layer, validation_layer_property.layerName) == 0)
            {
                validation_layer_found = true;
                break;
            }    
        }

        if (!validation_layer_found)
        {
            throw std::runtime_error("required validation layer not supported: " + std::string(required_layer));
        }
    }

    return true;
    
}

vk::DebugUtilsMessengerCreateInfoEXT VulkanContext::makeDebugMessengerCreateInfo()
{
    vk::DebugUtilsMessageSeverityFlagsEXT  severity_flags(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | 
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
    );

    vk::DebugUtilsMessageTypeFlagsEXT message_type_flags(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
    );

    vk::DebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{
        .messageSeverity = severity_flags,
        .messageType = message_type_flags,
        .pfnUserCallback = debugCallback
    };

    return debug_messenger_create_info;
}

 VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
    vk::DebugUtilsMessageTypeFlagsEXT  message_type,
    const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data
)
{
    if (
        message_severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || 
        message_severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
    )

    {
        std::cerr << "validation layer: type " <<  vk::to_string(message_type) << "msg: " << callback_data->pMessage << "\n";
    }

    return vk::False;
}