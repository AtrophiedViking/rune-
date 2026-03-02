#pragma once

#include <vulkan/vulkan.h>

struct Texture;
struct Material;
struct Model;
struct Scene;
struct State;

VkResult allocateDescriptorSetsWithResize(State* state, const VkDescriptorSetAllocateInfo* allocInfo, VkDescriptorSet* sets);
// Descriptor sets are organized into 3 sets (set = descriptor set index in shader):
// - set 0: global UBO (binding 0), sceneColor (binding 2), sceneDepth (binding 3)
// - set 1: material textures (baseColor at binding 0, metallicRoughness at binding 1, etc.)
// - set 2: present pass (sceneColor at binding 2, sceneDepth at binding 3)/
// Note: the present pass could also use the global set for sceneColor and sceneDepth, but I wanted to keep it separate to avoid conflicts with the global UBO bindings. This is a design choice and can be changed if desired.
// Note: the global UBO could also be split into a separate set from the scene samplers, but I think it's simpler to keep them together since they are both bound at set 0 and won't have many bindings.


// IBL descriptors (set = 0)
void iblSetLayoutCreate(State* state);
void iblSetLayoutDestroy(State* state);
void iblDescriptorPoolCreate(State* state);
void iblDescriptorPoolDestroy(State* state);
void iblSetCreate(State* state);

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