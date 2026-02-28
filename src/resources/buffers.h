#pragma once

#include <vector>
#include <vulkan/vulkan.h>


struct State;
struct Mesh;
struct Vertex;
struct Buffers{
	VkCommandBuffer* commandBuffer;
	VkFramebuffer* opaqueFramebuffers;
	VkFramebuffer* transparencyFramebuffers;
	VkFramebuffer* presentFramebuffers;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VkDeviceMemory* uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;
};

//utility
uint32_t findMemoryType(State* state, VkMemoryRequirements memRequirements, VkMemoryPropertyFlags properties);
//Buffers
void createBuffer(State* state, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
void createBufferForMaterial(State* state, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer* outBuffer, VkDeviceMemory* outMemory);
void copyBuffer(State* state, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

void vertexBufferCreateForMesh(State* state, const std::vector<Vertex>& vertices, VkBuffer& vertexBuffer, VkDeviceMemory& vertexMemory);

void vertexBufferDestroy(State* state);

void indexBufferCreateForMesh(State* state, const std::vector<uint32_t>& indices, VkBuffer& indexBuffer, VkDeviceMemory& indexMemory);

void indexBufferDestroy(State* state);

void createSkyboxVbo(State* state);
void destroySkyboxVbo(State* state);