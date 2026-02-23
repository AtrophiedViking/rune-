#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <vulkan/vulkan.h>
#include "core/math.h"


struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord0;
	glm::vec2 texCoord1;
	glm::vec3 normal;
	glm::vec4 tangent;
	uint32_t  materialIndex;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 6> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 6> attributeDescriptions{};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord0);

		attributeDescriptions[3].binding = 0;
		attributeDescriptions[3].location = 3;
		attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[3].offset = offsetof(Vertex, texCoord1);

		attributeDescriptions[4].binding = 0;
		attributeDescriptions[4].location = 4;
		attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[4].offset = offsetof(Vertex, normal);

		attributeDescriptions[5].binding = 0;
		attributeDescriptions[5].location = 5;
		attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attributeDescriptions[5].offset = offsetof(Vertex, tangent);

		return attributeDescriptions;

	}
	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && texCoord0 == other.texCoord0;
	}
};
namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord0) << 1);
		}
	};
}
struct Mesh {
	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;
	int                   materialIndex = -1;
	int					  gpuIndex = -1;

	//Move To GPU
	VkBuffer       vertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
	VkBuffer       indexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory indexMemory = VK_NULL_HANDLE;
}; 
