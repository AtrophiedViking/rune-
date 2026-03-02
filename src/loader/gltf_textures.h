#pragma once
#include <vulkan/vulkan.h>
#include "core/math.h"
#include <string>
namespace tinygltf {
	struct TextureInfo;
	struct NormalTextureInfo;
	struct OcclusionTextureInfo;
	class Value;
	struct Model;
};

struct State;
struct Texture;
struct TextureTransform;
enum TextureRole;
struct Model;

// Base type
void readTextureTransform(
	const tinygltf::TextureInfo& info,
	TextureTransform& out);

// NormalTextureInfo overload
void readTextureTransform(
	const tinygltf::NormalTextureInfo& info,
	TextureTransform& out);

// OcclusionTextureInfo overload
void readTextureTransform(
	const tinygltf::OcclusionTextureInfo& info,
	TextureTransform& out);
// Overload for extension-style texture objects (tinygltf::Value::Object)
void readTextureTransform(const tinygltf::Value& texObj, TextureTransform& out);

void generateMipmaps(State* state, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

void createTextureFromMemory(
	State* state,
	const unsigned char* pixels,
	size_t size, 
	int width,
	int height,
	int channels,
	Texture& outTex);
void destroyTextures(State* state);

void textureImageCreate(
	State* state,
	const std::string& texturePath,
	VkImage& outImage,
	VkDeviceMemory& outMemory,
	VkFormat& outFormat,
	uint32_t& outMipLevels
);
void textureImageDestroy(State* state);

void textureImageViewCreate(
	State* state,
	VkImage image,
	VkFormat format,
	VkImageAspectFlags aspectMask,
	uint32_t mipLevels,
	VkImageView& outView);
void textureImageViewDestroy(State* state);

void textureSamplerCreate(
	State* state,
	VkSampler& outSampler,
	float maxLod);

void textureSamplerDestroy(State* state);

void createModelTextures(State* state, Model* model, const tinygltf::Model& gltf, std::unordered_map<int, TextureRole>& textureRoles);
