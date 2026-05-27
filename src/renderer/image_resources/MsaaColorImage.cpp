#include "MsaaColorImage.h"
#include <utility>

MsaaColorImage::MsaaColorImage(
        vk::raii::Image image, 
        vk::raii::DeviceMemory device_memory,
        vk::raii::ImageView image_view
    ) : image_(std::move(image)), 
        deviceMemory_(std::move(device_memory)), 
        imageView_(std::move(image_view))
{

}


const vk::raii::ImageView& MsaaColorImage::getImageView() const
{
    return imageView_;
}

