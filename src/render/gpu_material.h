#pragma once
#include <vulkan/vulkan.h>
#include "core/math.h"

struct TextureTransform;

struct TexTransformGPU {
	glm::vec4 offset_scale;    // xy = offset, zw = scale
	glm::vec4 rot_center_tex;  // x = rotation, y/z = center, w = texCoord (float)
};

struct MaterialGPU {
	TexTransformGPU baseColorTT;
	TexTransformGPU mrTT;
	TexTransformGPU normalTT;
	TexTransformGPU occlusionTT;
	TexTransformGPU emissiveTT;

	// Transmission + Volume
	float thicknessFactor;
	float attenuationDistance;
	glm::vec4 attenuationColor;
};

TexTransformGPU toGPU(const TextureTransform t);