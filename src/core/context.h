#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <cstdint>
struct State;
struct Config;
struct Window;

struct Context{
	uint32_t queueFamilyIndex;
	uint32_t presentFamilyIndex;

	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkQueue queue;
	VkQueue presentQueue;
};

void instanceCreate(State* state);
void instanceDestroy(State* state);

bool deviceSuitabilityCheck(VkPhysicalDevice device);
void physicalDeviceSelect(State* state);
void queueFamilySelect(State* state);

void deviceCreate(State* state);
void deviceDestroy(State* state);
