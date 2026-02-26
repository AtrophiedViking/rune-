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

	uint32_t mipLevels;

	VkImage colorImage;
	VkDeviceMemory colorImageMemory;
	VkImageView colorImageView;

	VkImage transAccumImage;
	VkDeviceMemory transAccumImageMemory;
	VkImageView transAccumImageView;
	
	VkImage transRevealImage;
	VkDeviceMemory transRevealImageMemory;
	VkImageView transRevealImageView;

	VkImage msaaDepthImage;
	VkDeviceMemory msaaDepthImageMemory;
	VkImageView msaaDepthImageView;
	
	VkImage			singleDepthImage;
	VkDeviceMemory	singleDepthImageMemory;
	VkImageView		singleDepthImageView;

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
