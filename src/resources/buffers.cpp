#include "resources/buffers.h"
#include "render/command_buffers.h"
#include "render/renderer.h"
#include "core/context.h"
#include "core/state.h"
#include "scene/skybox.h"
#include "scene/mesh.h"
#include <stdexcept>

//Utility
uint32_t findMemoryType(State* state, VkMemoryRequirements memRequirements, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(state->context->physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((memRequirements.memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}
//Buffers
void createBuffer(State* state, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(state->context->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(state->context->device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(state, memRequirements, properties);

	if (vkAllocateMemory(state->context->device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(state->context->device, buffer, bufferMemory, 0);
}

void copyBuffer(State* state, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands(state, state->renderer->commandPool);

	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(state, commandBuffer);
};

void createBufferForMaterial(
	State* state,
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkBuffer* outBuffer,
	VkDeviceMemory* outMemory)
{
	// This is identical to how you create your UBO buffer
	createBuffer(
		state,
		size,
		usage | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		*outBuffer,
		*outMemory
	);
}
void vertexBufferCreateForMesh(State* state, const std::vector<Vertex>& vertices, VkBuffer& vertexBuffer, VkDeviceMemory& vertexMemory) {

	if (vertices.empty()) return;

	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(state, bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(state->context->device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(state->context->device, stagingBufferMemory);

	createBuffer(state, bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBuffer, vertexMemory);

	copyBuffer(state, stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(state->context->device, stagingBuffer, nullptr);
	vkFreeMemory(state->context->device, stagingBufferMemory, nullptr);
}

void vertexBufferDestroy(State* state) {
	vkDestroyBuffer(state->context->device, state->buffers->vertexBuffer, nullptr);
	vkFreeMemory(state->context->device, state->buffers->vertexBufferMemory, nullptr);
};

void indexBufferCreateForMesh(State* state, const std::vector<uint32_t>& indices, VkBuffer& indexBuffer, VkDeviceMemory& indexMemory) {

	if (indices.empty()) return;

	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(state, bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(state->context->device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(state->context->device, stagingBufferMemory);

	createBuffer(state, bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBuffer, indexMemory);

	copyBuffer(state, stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(state->context->device, stagingBuffer, nullptr);
	vkFreeMemory(state->context->device, stagingBufferMemory, nullptr);
}

void indexBufferDestroy(State* state) {
	vkDestroyBuffer(state->context->device, state->buffers->indexBuffer, nullptr);
	vkFreeMemory(state->context->device, state->buffers->indexBufferMemory, nullptr);
};

void createSkyboxVbo(State* state)
{
	VkDeviceSize bufferSize = sizeof(skyboxVertices[0]) * skyboxVertices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	// 1. Create staging buffer
	createBuffer(
		state,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer,
		stagingBufferMemory
	);

	// 2. Copy vertex data into staging buffer
	void* data;
	vkMapMemory(state->context->device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, skyboxVertices.data(), (size_t)bufferSize);
	vkUnmapMemory(state->context->device, stagingBufferMemory);

	// 3. Create device-local vertex buffer
	createBuffer(
		state,
		bufferSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		state->renderer->skyboxVbo,
		state->renderer->skyboxVboMemory
	);

	// 4. Copy staging → device-local
	copyBuffer(state, stagingBuffer, state->renderer->skyboxVbo, bufferSize);

	// 5. Cleanup staging
	vkDestroyBuffer(state->context->device, stagingBuffer, nullptr);
	vkFreeMemory(state->context->device, stagingBufferMemory, nullptr);
}
void destroySkyboxVbo(State* state)
{
	vkDestroyBuffer(state->context->device, state->renderer->skyboxVbo, nullptr);
	vkFreeMemory(state->context->device, state->renderer->skyboxVboMemory, nullptr);
}