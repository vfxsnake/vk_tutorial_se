#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <string>
#include <optional>

class VulkanContext
{
public:
    explicit VulkanContext(GLFWwindow* window);

    // removing class default methods for copying and move semantics
    
    /* VulkanContext(const VulkanContext&) =  delete;  
        deletes VulkanContext b(a); where a is a created vulkan context (copy constructor).
        deletes VulkanContext b = a; where b doesn't exists but a does (copy constructor).
        deletes foo(a); where foo is any function trying to pass by copy an existing context (passing by copy)
    */
    VulkanContext(const VulkanContext&) =  delete;  


    /* VulkanContext& operator=(const VulkanContext&) = delete; 
        deletes  b = a; where b is an existing context, a exists too and is trying to pass the values from right to left.
    */
    VulkanContext& operator=(const VulkanContext&) = delete;

    /* VulkanContext(VulkanContext&&) = delete;
        deletes VulkanContext b = std::move(a); where b doesn't exists but a does and trys to move a into the new b;
    */
    VulkanContext(VulkanContext&&) = delete;
    
    /* VulkanContext& operator=(VulkanContext&&) = delete;
        deletes b = std::move(a); where a and b exists as context but trys to move the contents of a to b;
    */
    VulkanContext& operator=(VulkanContext&&) = delete;

    // Accessors methods
    auto getLogicalDevice() const -> const vk::raii::Device&; // retruns the logical device.
    auto getPhysicalDevice() const -> const vk::raii::PhysicalDevice&;
    auto getQueue() -> vk::raii::Queue&; // this method wont be constat as we plan to edit the queue with submit call later
    auto getSurface() const -> const vk::raii::SurfaceKHR&;
    uint32_t getQueueFamilyIndex() const;

private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createLogicalDevice();

    // Device suitability helpers
    bool isDeviceSuitable(const vk::raii::PhysicalDevice& physical_device) const;
    auto findQueueFamily(const vk::raii::PhysicalDevice& physical_device) const -> std::optional<uint32_t>; // returns uint32_t or an empty value.
    bool checkDeviceExtensionSupport(const vk::raii::PhysicalDevice& physical_device) const;

    // Instance creation helpers
    static auto getRequiredInstanceExtensions() -> std::vector<const char*>;
    static bool checkExtensionSupport(
        const std::vector<const char*>& required_extensions, 
        const std::vector<vk::ExtensionProperties>& extension_properties
    );
    static bool checkValidationLayersSupport(
        const std::vector<const char*>& required_layers, 
        const std::vector<vk::LayerProperties>& validation_layer_properties
    );

    // Debug messenger
    static auto makeDebugMessengerCreateInfo() -> vk::DebugUtilsMessengerCreateInfoEXT;
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
        vk::DebugUtilsMessageTypeFlagsEXT message_type,
        const vk::DebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data
    );

    // Required device extensions
    static const std::vector<const char*> REQUIRED_DEVICE_EXTENSIONS;

    // Enabling validation layers only in debug mode
#ifdef NDEBUG
    static constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
    static constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif
    static const std::vector<const char*> VALIDATION_LAYERS;


    // Private member variables, order matters as it dictates the order of destruction (in backwards direction)
    vk::raii::Context context_;
    vk::raii::Instance instance_ = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger_ = nullptr;
    vk::raii::SurfaceKHR surface_ = nullptr;
    vk::raii::PhysicalDevice physicalDevice_ = nullptr;
    vk::raii::Device logicalDevice_ = nullptr;
    vk::raii::Queue graphicsQueue_ = nullptr;
    uint32_t queueFamilyIndex_ = 0;

};
