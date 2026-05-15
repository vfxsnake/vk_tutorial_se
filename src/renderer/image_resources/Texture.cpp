#include "Texture.h"

#include <utility>

Texture::Texture(
    vk::raii::Image image,
    vk::raii::DeviceMemory device_memory,
    vk::raii::ImageView image_view,
    vk::raii::Sampler sampler
) : image_(std::move(image)), 
    deviceMemory_(std::move(device_memory)), 
    imageView_(std::move(image_view)), 
    sampler_(std::move(sampler))
{

}

const vk::raii::ImageView& Texture::getImageView() const
{
    return imageView_; 
}

const vk::raii::Sampler& Texture::getSampler() const
{
    return sampler_;
}
