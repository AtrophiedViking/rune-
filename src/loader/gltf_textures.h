#pragma once
#include "vulkan/vulkan.h"
#include <string>
namespace tinygltf {
	struct TextureInfo;
	struct NormalTextureInfo;
	struct OcclusionTextureInfo;
}

struct State;
struct Texture;
struct TextureTransform;
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

void textureImageCreate(State* state, std::string texturePath);
void textureImageDestroy(State* state);

void textureImageViewCreate(State* state);
void textureImageViewDestroy(State* state);

void textureSamplerCreate(State* state);
void textureSamplerDestroy(State* state);

