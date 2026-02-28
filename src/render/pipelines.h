#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include <fstream>

struct State;

void skyboxPipelineCreate(State* state);
void skyboxPipelineDestroy(State * state);

void opaquePipelineCreate(State* state);
void opaquePipelineDestroy(State* state);

void transparencyPipelineCreate(State* state);
void transparencyPipelineDestroy(State* state);

void presentPipelineCreate(State* state);
void presentPipelineDestroy(State* state);