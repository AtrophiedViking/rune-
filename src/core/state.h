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

	float exposure = 1.0f;
	float gamma = 1.0f;
	float prefilteredCubeMipLevels = 1.0f;
	float scaleIBLAmbient = 1.0f;
};
//push constants

struct PushConstantBlock
{
	glm::mat4 nodeMatrix;            // 64
	glm::vec4 baseColorFactor;       // 16

	float metallicFactor;            // 4
	float roughnessFactor;           // 4
	int   baseColorTextureSet;       // 4
	int   physicalDescriptorTextureSet; // 4

	int   normalTextureSet;          // 4
	int   occlusionTextureSet;       // 4
	int   emissiveTextureSet;        // 4
	float alphaMask;                 // 4

	float alphaMaskCutoff;           // 4
	float transmissionFactor;        // 4
	int   transmissionTextureIndex;  // 4
	int   transmissionTexCoordIndex; // 4

	int   _padT;                     // 4

	float thicknessFactor;           // 4
	int   thicknessTextureIndex;     // 4
	int   thicknessTexCoordIndex;    // 4
	glm::vec4 attenuation;      // replaces vec3 + pad
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