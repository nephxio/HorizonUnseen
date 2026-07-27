#pragma once

// GPU texture backed by an image file (or a generated fallback).
//
// The sprite renderer only ever needs one of these -- the atlas -- but keeping
// it a standalone type means the upload path is testable and reusable.

#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>

class VulkanContext;

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Loads an RGBA image from disk. Returns false and leaves the texture empty
    // if the file is missing or undecodable; callers are expected to fall back
    // to createWhite().
    bool loadFromFile(VulkanContext& context, const std::string& path);

    // 1x1 opaque white texel. Used when the atlas cannot be loaded so the game
    // still renders tinted quads instead of nothing.
    bool createWhite(VulkanContext& context);

    void cleanup();

    bool isValid() const { return m_imageView != VK_NULL_HANDLE && m_sampler != VK_NULL_HANDLE; }
    VkImageView getImageView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }
    std::uint32_t getWidth() const { return m_width; }
    std::uint32_t getHeight() const { return m_height; }

private:
    bool createFromPixels(VulkanContext& context, const unsigned char* pixels,
                          std::uint32_t width, std::uint32_t height);
    bool createSampler();
    void transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout);

    VulkanContext* m_context = nullptr;
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
};
