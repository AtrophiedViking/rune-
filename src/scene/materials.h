#pragma once
#include "scene/texture.h"
struct Material {
	// glTF indices into scene.textures
	int baseColorTextureIndex = -1;
	int metallicRoughnessTextureIndex = -1;
	int normalTextureIndex = -1;
	int occlusionTextureIndex = -1;
	int emissiveTextureIndex = -1;

	// Per-texture transforms + texCoord selection
	TextureTransform baseColorTransform;
	TextureTransform metallicRoughnessTransform;
	TextureTransform normalTransform;
	TextureTransform occlusionTransform;
	TextureTransform emissiveTransform;

	// Existing stuff you already have
	glm::vec4 baseColorFactor = glm::vec4(1.0f);
	float     metallicFactor = 1.0f;
	float     roughnessFactor = 1.0f;
	glm::vec3 emissiveFactor = glm::vec3(0.0f);

	std::string alphaMode = "OPAQUE";  // "OPAQUE", "MASK", "BLEND"
	float alphaCutoff = 0.5f;          // Only used for MASK
	bool doubleSided = false;

	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkBuffer       materialBuffer = VK_NULL_HANDLE;
	VkDeviceMemory materialMemory = VK_NULL_HANDLE;
};
