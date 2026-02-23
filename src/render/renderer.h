#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "render/gpu_material.h"
#include "render/gpu_mesh.h"
// Forward declarations
struct State;
struct Scene;
struct Node;
struct Mesh;
struct Material;
struct DrawItem;

struct Renderer {
	//Sorting
	std::vector<DrawItem*> opaqueDrawItems;
	std::vector<DrawItem*> transparentDrawItems;

	uint32_t imageAquiredIndex;
	VkSemaphore* imageAvailableSemaphore;
	VkSemaphore* renderFinishedSemaphore;
	VkFence* inFlightFence;
	uint32_t frameIndex;

	uint32_t descriptorPoolMultiplier = 2;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout textureSetLayout;
	std::vector<VkDescriptorSet> descriptorSets;

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;

	//Shaders
	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;
	VkShaderModule presentFragShaderModule;
	VkShaderModule presentVertShaderModule;

	//graphicsPipeline
	VkPipeline graphicsPipeline;
	VkDescriptorPool descriptorPool;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;
	VkCommandPool commandPool;
	//transparencyPipeline
	VkPipeline transparencyPipeline;
	VkDescriptorPool transparencyDescriptorPool;
	VkPipelineLayout transparencyPipelineLayout;
	VkRenderPass transparencyRenderPass;

	VkPipeline presentPipeline;
	VkDescriptorSetLayout presentSetLayout;
	VkDescriptorSet presentSet;
	VkDescriptorPool presentDescriptorPool;
	VkPipelineLayout presentPipelineLayout;
	VkRenderPass presentRenderPass;

	std::vector<MaterialGPU> materialsGPU;
	std::vector<MeshGPU> meshesGPU;
};

void drawMesh(State* state, VkCommandBuffer cmd, const Mesh* mesh, const glm::mat4& nodeMatrix, const glm::mat4& modelTransform);