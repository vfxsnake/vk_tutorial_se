#include "DepthImage.h"
#include <utility>

DepthImage::DepthImage(
        vk::raii::Image image, 
        vk::raii::DeviceMemory device_memory,
        vk::raii::ImageView image_view
    ) : image_(std::move(image)), 
        deviceMemory_(std::move(device_memory)), 
        imageView_(std::move(image_view))
{

}


const vk::raii::ImageView& DepthImage::getImageView() const
{
    return imageView_;
}

