#pragma once
#include "vulkan/vulkan.h"
struct State;
void opaqueFrameBuffersCreate(State* state);
void opaqueFrameBuffersDestroy(State* state);

void transparentFrameBuffersCreate(State* state);
void transparentFrameBuffersDestroy(State* state);

void presentFramebuffersCreate(State* state);
void presentFramebuffersDestroy(State* state);