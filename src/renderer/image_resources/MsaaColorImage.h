#pragma once

#include <vulkan/vulkan_raii.hpp>

class MsaaColorImage
{
public:
    MsaaColorImage(
        vk::raii::Image image, 
        vk::raii::DeviceMemory device_memory,
        vk::raii::ImageView image_view
    );

    MsaaColorImage(const MsaaColorImage&) = delete;
    MsaaColorImage& operator =(const MsaaColorImage&) = delete;

    MsaaColorImage(MsaaColorImage&&) = default;
    MsaaColorImage& operator =(MsaaColorImage&&) = default;

    auto getImageView() const -> const vk::raii::ImageView&;

private:
    vk::raii::Image image_;
    vk::raii::DeviceMemory deviceMemory_;
    vk::raii::ImageView imageView_;

};