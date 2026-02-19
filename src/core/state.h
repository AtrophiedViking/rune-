#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <csignal>
#include "core/math.h"



#define PANIC(ERROR, FORMAT,...){int macroErrorCode = ERROR; if(macroErrorCode){fprintf(stderr, "%s -> %s -> %i -> Error(%i):\n\t" FORMAT "\n", __FILE__, __func__, __LINE__, macroErrorCode, ##__VA_ARGS__); raise(SIGABRT);}};

// Forward declarations only
struct Config;
struct Context;
struct Renderer;
struct Buffers;
struct Scene;
struct Texture;
struct Mesh;
struct Gui;

// UBO 
struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;

	glm::vec4 lightPositions[4];
	glm::vec4 lightColors[4];
	glm::vec4 camPos;

	float exposure = 2.2f;
	float gamma = 1.0f;
	float prefilteredCubeMipLevels = 1.0f;
	float scaleIBLAmbient = 1.0f;
};
//push constants
struct PushConstantBlock {
	glm::mat4 nodeMatrix;
	glm::vec4 baseColorFactor;
	float metallicFactor;
	float roughnessFactor;
	int baseColorTextureSet;
	int physicalDescriptorTextureSet;
	int normalTextureSet;
	int occlusionTextureSet;
	int emissiveTextureSet;
	float alphaMask;
	float alphaMaskCutoff;
};

struct Swapchain{
	VkSwapchainKHR handle;

	uint32_t imageCount;
	std::vector<VkImage> images;
	VkImageView* imageViews;

	VkFormat format;
	VkColorSpaceKHR colorSpace;
	VkExtent2D imageExtent;
};

struct Window{
	GLFWwindow* handle;
	VkSurfaceKHR surface;
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	Swapchain swapchain;
	bool framebufferResized;
};

struct State {
	Config *config;
	Window window;
	Context *context;
	Renderer *renderer;
	Buffers *buffers;
	Scene *scene;
	Texture *texture;
	Mesh *mesh;
	Gui *gui;
};

enum SwapchainBuffering {
	SWAPCHAIN_SINGLE_BUFFERING = 1,
	SWAPCHAIN_DOUBLE_BUFFERING = 2,
	SWAPCHAIN_TRIPPLE_BUFFERING = 3,
};