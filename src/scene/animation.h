#pragma once
#pragma once

#include <vector>
#include <string>
#include <limits>
#include "core/math.h"
struct Node;
// Structure for animation keyframes
struct AnimationChannel {
	enum PathType { TRANSLATION, ROTATION, SCALE };
	PathType path;
	Node* node = nullptr;
	uint32_t samplerIndex;
};

// Structure for animation interpolation
struct AnimationSampler {
	enum InterpolationType { LINEAR, STEP, CUBICSPLINE };
	InterpolationType interpolation;
	std::vector<float> inputs;  // Key frame timestamps
	std::vector<glm::vec4> outputsVec4;  // Key frame values (for rotations)
	std::vector<glm::vec3> outputsVec3;  // Key frame values (for translations and scales)
};

// Structure for animation
struct Animation {
	std::string name;
	std::vector<AnimationSampler> samplers;
	std::vector<AnimationChannel> channels;
	float start = std::numeric_limits<float>::max();
	float end = std::numeric_limits<float>::min();
	float currentTime = 0.0f;
};