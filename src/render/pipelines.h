#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include <fstream>

struct State;
//Compute Pipelines
void iblPipelineCreate(State* state);
void brdfLutPipelineCreate(State* state);
//Graphics Pipelines
void skyboxPipelineCreate(State* state);
void skyboxPipelineDestroy(State * state);

void opaquePipelineCreate(State* state);
void opaquePipelineDestroy(State* state);

void transparencyPipelineCreate(State* state);
void transparencyPipelineDestroy(State* state);

void presentPipelineCreate(State* state);
void presentPipelineDestroy(State* state);

//Dispatch
void createComputeMipViews(State* state);
void iblPrefilterDispatch(State* state, uint32_t width);
void brdfLutGenerate(State* state);
