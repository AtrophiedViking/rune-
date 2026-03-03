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
void createCubeSampler(State* state, VkSampler& sampler, uint32_t mipLevels);

void specularCubeImageCreate(State* state, uint32_t baseSize);


// 2) Cube view for sampling in fragment shader
void specularCubeViewCreate(State* state);

// 3) Sampler for specular cubemap
void specularSamplerCreate(State* state);

// 4) Per-mip 2D array views for compute writes
void computeMipViewsCreate(State* state);


