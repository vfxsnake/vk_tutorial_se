#pragma once

#include <vulkan/vulkan_raii.hpp>

class Texture
{
public:
    Texture(
        vk::raii::Image image,
        vk::raii::DeviceMemory device_memory,
        vk::raii::ImageView image_view,
        vk::raii::Sampler sampler
    );

    // deleting copy constructures
    Texture(const Texture&) = delete;
    Texture& operator =(const Texture&) = delete;

    // move constructor as default
    Texture(Texture&&) = default;
    Texture& operator =(Texture&&) = default;

    // accessor functions
    auto getImageView() const -> const vk::raii::ImageView&;
    auto getSampler() const -> const vk::raii::Sampler&;

private:
    vk::raii::Image image_ = nullptr;
    vk::raii::DeviceMemory deviceMemory_ = nullptr;
    vk::raii::ImageView imageView_ = nullptr;
    vk::raii::Sampler sampler_ = nullptr;

};