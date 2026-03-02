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

	VkImage cubeImage;
	VkDeviceMemory cubeImageMemory;
	VkImageView cubeImageView;
	VkSampler cubeSampler;
	uint32_t cubeMipLevels;


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

	VkImage        sceneDepthImage;
	VkDeviceMemory sceneDepthImageMemory;
	VkImageView    sceneDepthImageView;
	VkSampler	   sceneDepthSampler;
	
	VkImage        irradianceImage;
	VkDeviceMemory irradianceImageMemory;
	VkImageView    irradianceImageView;
	VkSampler	   irradianceSampler;
	uint32_t	   irradianceMipLevels;
	VkFormat	   irradianceFormat;

	
	VkImage        computeImage;
	VkDeviceMemory computeImageMemory;
	VkImageView    computeImageView;
	VkSampler	   computeSampler;
	uint32_t	   computeMipLevels;

	VkImage        specularImage;
	VkDeviceMemory specularImageMemory;
	VkImageView    specularImageView;
	VkSampler	   specularSampler;
	uint32_t       specularMipLevels;
	VkFormat	   specularFormat;

	
	VkImage        lutImage;
	VkDeviceMemory lutImageMemory;
	VkImageView    lutImageView;
	VkSampler	   lutSampler;
	uint32_t	   lutMipLevels;

	VkDescriptorSet descriptorSet;
	VkFormat format;
	
	VkFormat envFormat;
	uint32_t envMipLevels;


	VkFormat lutFormat;

	TextureRole role = TextureRole::BaseColor;
};

struct TextureTransform {
	glm::vec2 offset = { 0,0 };
	glm::vec2 scale = { 1,1 };
	float rotation = 0.0f;
	glm::vec2 center = { 0,0 };
	int texCoord = 0;
};
