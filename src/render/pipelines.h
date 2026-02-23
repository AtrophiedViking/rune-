#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include <fstream>

struct State;

void graphicsPipelineCreate(State* state);
void graphicsPipelineDestroy(State* state);

void tranparencyPipelineCreate(State* state);
void tranparencyPipelineDestroy(State* state);

void presentPipelineCreate(State* state);
void presentPipelineDestroy(State* state);