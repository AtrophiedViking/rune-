#pragma once

#include <string>
#include <vulkan/vulkan.h>
struct Config{
	const std::string windowTitle;
	const std::string engineName;
	bool windowResizable;
	uint32_t windowWidth;
	uint32_t windowHeight;
	uint32_t applicationVersion;
	uint32_t engineVersion;
	uint32_t apiVersion;
	uint32_t swapchainBuffering;
	uint32_t MAX_OBJECTS;
	VkAllocationCallbacks allocator;
	VkComponentMapping swapchainComponentsMapping;
	VkClearValue backgroundColor;
	VkSampleCountFlagBits msaaSamples;
	const std::string KOBOLD_TEXTURE_PATH;
	const std::string KOBOLD_MODEL_PATH;
	const std::string HOVER_BIKE_MODEL_PATH;
	const std::string MODEL_PATH;

};
