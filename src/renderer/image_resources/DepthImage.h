#pragma once

#include <vulkan/vulkan_raii.hpp>

class DepthImage
{
public:
    DepthImage(
        vk::raii::Image image, 
        vk::raii::DeviceMemory device_memory,
        vk::raii::ImageView image_view
    );

    DepthImage(const DepthImage&) = delete;
    DepthImage& operator =(const DepthImage&) = delete;

    DepthImage(DepthImage&&) = default;
    DepthImage& operator =(DepthImage&&) = default;

    auto getImageView() const -> const vk::raii::ImageView&;

private:
    vk::raii::Image image_;
    vk::raii::DeviceMemory deviceMemory_;
    vk::raii::ImageView imageView_;

};