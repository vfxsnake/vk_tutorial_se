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

    extent_ = chooseExtent(surface_capabilities);
    uint32_t image_count = getImageCountFrom(surface_capabilities);
    surfaceFormat_ = chooseSurfaceFormat(available_formats);
    vk::PresentModeKHR present_mode = choosePresentMode(available_present_modes);
    
    vk::SwapchainCreateInfoKHR swap_chain_create_info{
        .surface = *context_.getSurface(), // '*' is an overloaded operator of vk::raii::surface for geting the wrapped vk::SurfaceKHR handle
        .minImageCount = image_count,
        .imageFormat = surfaceFormat_.format,
        .imageColorSpace = surfaceFormat_.colorSpace,
        .imageExtent = extent_,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surface_capabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = present_mode,
        .clipped = true
    };

    swapChain_ = vk::raii::SwapchainKHR(context_.getLogicalDevice(), swap_chain_create_info);
    images_ = swapChain_.getImages();
}


vk::Extent2D SwapChain::chooseExtent(const vk::SurfaceCapabilitiesKHR &capabilities) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    int width; 
    int height;
    glfwGetFramebufferSize(window_, &width, &height);

    return {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
    };

}


uint32_t SwapChain::getImageCountFrom(const vk::SurfaceCapabilitiesKHR& capabilities) const
{
    uint32_t target_image_count = capabilities.minImageCount + 1;
    if (
        (capabilities.maxImageCount > 0) &&  // check if maxImage is valid.
        (capabilities.maxImageCount < target_image_count)  // checks if the target count can be achievable.
    )
    {
        return capabilities.maxImageCount;  // returning the maximum count supported by in the capabilities
    }

    return target_image_count;  // returning the expected count.
}


vk::SurfaceFormatKHR SwapChain::chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) const
{
    assert(!formats.empty());
    
    for (const auto& format : formats)
    {
        if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return format;
        }
    }

    return formats[0];
    
}


vk::PresentModeKHR SwapChain::choosePresentMode(const std::vector<vk::PresentModeKHR>& modes) const
{
    // check at least eFifo is available
    assert(
        std::ranges::any_of( // is any of the elements fifo(true) 
            modes, // vector to iterate from
            [](const auto present_mode) // present_mode represents an element of the vector in each iteration inside the lambda function.
            {
                return present_mode == vk::PresentModeKHR::eFifo;
            }
        )
    );


    bool is_mailbox_available = std::ranges::any_of( // is any of the elements mailbox(true)
        modes,
        [](const auto present_mode)
        {
            return present_mode == vk::PresentModeKHR::eMailbox;
        }
    );
    
    if (is_mailbox_available) return vk::PresentModeKHR::eMailbox;

    return vk::PresentModeKHR::eFifo;
}