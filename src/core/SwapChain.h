#pragma once

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>
#include <vector>

// forward declaring classes
class VulkanContext;

class SwapChain
{
public:
    SwapChain(const VulkanContext& context, GLFWwindow* window);

    // deleting copy constructures
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    // public member functions
    void recreate();
    
    // accessor functions
    vk::Format getFormat() const;
    vk::Extent2D getExtent() const;
    uint32_t getImageCount() const;

    auto get() const -> const vk::raii::SwapchainKHR&;
    auto getImageViews() const -> const std::vector<vk::raii::ImageView>&;

private:
    // private member functions
    void create();
    void createImageViews();
    void cleanup();

    // swap chain create helper functions
    auto chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) const -> vk::SurfaceFormatKHR;
    auto choosePresentMode(const std::vector<vk::PresentModeKHR>& modes) const -> vk::PresentModeKHR;
    auto chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities) const -> vk::Extent2D;
    uint32_t getImageCountFrom(const vk::SurfaceCapabilitiesKHR& capabilities) const;

    // private member variables
    const VulkanContext& context_;
    GLFWwindow* window_;

    vk::raii::SwapchainKHR swapChain_ = nullptr;
    std::vector<vk::Image> images_;
    std::vector<vk::raii::ImageView> imageViews_;
    vk::SurfaceFormatKHR surfaceFormat_ = {};
    vk::Extent2D extent_ = {};
};