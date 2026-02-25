#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <unordered_map>
#include "core/math.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <chrono>
#include <vector>
#include <array>

enum TextureRole {
	BaseColor,
	MetallicRoughness,
	Normal,
	Tangent,
	Occlusion,
	Emissive
};

struct Texture{
	std::string name;
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;

	VkImage msaaDepthImage;
	VkDeviceMemory msaaDepthImageMemory;
	VkImageView msaaDepthImageView;
	
	uint32_t mipLevels;

	VkImage			singleDepthImage;
	VkDeviceMemory	singleDepthImageMemory;
	VkImageView		singleDepthImageView;

	VkImage colorImage;
	VkDeviceMemory colorImageMemory;
	VkImageView colorImageView;

	VkImage        sceneColorImage;
	VkDeviceMemory sceneColorImageMemory;
	VkImageView    sceneColorImageView;
	VkSampler	   sceneColorSampler;

	VkDescriptorSet descriptorSet;
	VkFormat format;

	TextureRole role = TextureRole::BaseColor;
};

struct TextureTransform {
	glm::vec2 offset = { 0,0 };
	glm::vec2 scale = { 1,1 };
	float rotation = 0.0f;
	glm::vec2 center = { 0,0 };
	int texCoord = 0;
};
