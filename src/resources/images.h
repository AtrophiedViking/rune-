#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
struct State;
//Utility
VkFormat findSupportedFormat(State* state, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
VkFormat findDepthFormat(State* state);
bool hasStencilComponent(VkFormat format);

//Textures
void imageCreate(State* state, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, uint32_t mipLevels, VkSampleCountFlagBits numSamples);
VkImageView imageViewCreate(State* state, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

void transitionImageLayout(State* state, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
void transitionSwapchainImagesToPresent(State* state);
void copyBufferToImage(State* state, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
