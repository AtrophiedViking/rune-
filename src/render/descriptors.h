#pragma once

#include <vulkan/vulkan.h>

struct Texture;
struct Material;
struct Model;
struct Scene;
struct State;

// set 0: global UBO
void createGlobalSetLayout(State* state);
// set 1: texture (for now, just baseColor at binding 0)
void createTextureSetLayout(State* state);
void descriptorSetLayoutDestroy(State* state);

void descriptorPoolCreate(State* state);
VkResult allocateDescriptorSetsWithResize(State* state, const VkDescriptorSetAllocateInfo* allocInfo, VkDescriptorSet* sets);
void descriptorSetsCreate(State* state);
void createMaterialDescriptorSets(State* state);
void descriptorPoolDestroy(State* state);

void uniformBuffersCreate(State* state);
void uniformBuffersUpdate(State* state);
void uniformBuffersDestroy(State* state);