#pragma once 
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

struct State;

void copyBufferToImageRegions(State* state, VkBuffer buffer, VkImage image, const std::vector<VkBufferImageCopy>& regions);
   
void imageCreateCube(State* state, uint32_t width, uint32_t height, VkFormat format, uint32_t mipLevels, VkImage& image, VkDeviceMemory& imageMemory, VkMemoryPropertyFlags properties);

void createCubeImageView(State* state, VkImage image, VkFormat format, uint32_t mipLevels, VkImageView& imageView);

void textureCubeImageCreate(State* state, const std::string& texturePath);

void createCubeSampler(State* state, VkSampler& sampler);