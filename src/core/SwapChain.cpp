#include "SwapChain.h"
#include "VulkanContext.h"

SwapChain::SwapChain(const VulkanContext& context, GLFWwindow* window): context_(context), window_(window)
{
    create();
    createImageViews();
}


void SwapChain::create()
{
    vk::SurfaceCapabilitiesKHR surface_capabilities = context_.getPhysicalDevice().getSurfaceCapabilitiesKHR(context_.getSurface());
    std::vector<vk::SurfaceFormatKHR> available_formats = context_.getPhysicalDevice().getSurfaceFormatsKHR(context_.getSurface());
    std::vector<vk::PresentModeKHR> available_present_modes = context_.getPhysicalDevice().getSurfacePresentModesKHR(context_.getSurface());
}