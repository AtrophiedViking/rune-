#pragma once
#include <vulkan/vulkan.h>
struct State;
void commandPoolCreate(State* state);
void commandPoolDestroy(State* state);

VkCommandBuffer beginSingleTimeCommands(State* state, VkCommandPool commandPool);
void endSingleTimeCommands(State* state, VkCommandBuffer commandBuffer);

void commandBufferGet(State* state);
void commandBufferRecord(State* state);
