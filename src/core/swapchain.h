#pragma once
#include <vulkan/vulkan.h>

struct State;
//Swapchain
VkSurfaceCapabilitiesKHR surfaceCapabilitiesGet(State* state);
VkSurfaceFormatKHR surfaceFormatSelect(State* state);
VkPresentModeKHR presentModeSelect(State* state);

void swapchainImageGet(State* state);
void imageViewsCreate(State* state);
void imageViewsDestroy(State* state);

void swapchainCreate(State* state);
void swapchainDestroy(State* state);

void swapchainCleanup(State* state);
void swapchainRecreate(State* state);