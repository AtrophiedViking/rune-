#pragma once
#include "vulkan/vulkan.h"
struct State;
//Graphics Pipeline
void renderPassCreate(State* state);
void renderPassDestroy(State* state);

VkSampleCountFlagBits getMaxUsableSampleCount(State* state);

void colorResourceCreate(State* state);
void colorResourceDestroy(State* state);

void depthResourceCreate(State* state);
void depthBufferDestroy(State* state);