#pragma once

#include <vulkan/vulkan.h>

struct Texture;
struct Material;
struct Model;
struct Scene;
struct State;

VkResult allocateDescriptorSetsWithResize(State* state, const VkDescriptorSetAllocateInfo* allocInfo, VkDescriptorSet* sets);

// set 0: global UBO
void globalSetLayoutCreate(State* state);
void globalSetLayoutDestroy(State* state);
void globalDescriptorPoolCreate(State* state);
void globalDescriptorPoolDestroy(State* state);
void globalSetsCreate(State* state);

// set 1: texture (for now, just baseColor at binding 0)
void materialSetLayoutCreate(State* state);
void materialSetLayoutDestroy(State* state);
void materialDescriptorPoolCreate(State* state);
void materialDescriptorPoolDestroy(State* state);
void materialSetsCreate(State* state);

// set 2: present pass (sceneColor at binding 2, sceneDepth at binding 3)
void presentSetLayoutCreate(State* state);
void presentDescriptorSetAllocate(State* state);
void presentDescriptorSetUpdate(State* state);
void presentSamplerCreate(State* state);
void destroySceneColorSampler(State* state);

// Scene samplers (bound in global set at bindings 2 and 3)
void sceneDepthSamplerCreate(State* state);

// Uniform buffers (global UBO at set 0 binding 0)
void uniformBuffersCreate(State* state);
void uniformBuffersUpdate(State* state);
void uniformBuffersDestroy(State* state);