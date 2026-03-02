#pragma once 
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

struct State;

void copyBufferToImageRegions(State* state, VkBuffer buffer, VkImage image, const std::vector<VkBufferImageCopy>& regions);
   
void cubeImageCreate(
    State* state,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    uint32_t mipLevels,
    VkImageUsageFlags usage,
    VkImage& image,
    VkDeviceMemory& imageMemory);
void createCubeImageView(State* state,
    VkImage image,
    VkFormat format,
    uint32_t mipLevels,
    VkImageView& imageView);
void textureCubeImageCreate(
    State* state,
    const std::string& texturePath,
    VkImage& outImage,
    VkDeviceMemory& outImageMemory,
    VkFormat& outFormat,
    uint32_t& outMipLevels,
    uint32_t* outWidth = nullptr,
    uint32_t* outHeight = nullptr);
void computeCubeImageCreate(
    State* state,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    uint32_t mipLevels,
    VkImage& outImage,
    VkDeviceMemory& outMemory);
void createCubeSampler(State* state, VkSampler& sampler, uint32_t mipLevels);