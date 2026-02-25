#pragma once
#include "vulkan/vulkan.h"
struct State;
//Utility
VkSampleCountFlagBits getMaxUsableSampleCount(State* state);

//RenderPasses
void opaqueRenderPassCreate(State* state);
void opaqueRenderPassDestroy(State* state);

void transparentRenderPassCreate(State* state);
void transparentRenderPassDestroy(State* state);

void presentRenderPassCreate(State* state);
void presentRenderPassDestroy(State * state);

//Resources
void colorResourceCreate(State* state);
void colorResourceDestroy(State* state);

void sceneColorResourceCreate(State* state);
void sceneColorResourceDestroy(State* state);

void depthResourceCreate(State* state);
void depthResourceDestroy(State* state);