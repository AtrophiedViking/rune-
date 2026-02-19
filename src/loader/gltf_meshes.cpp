#include "vulkan/vulkan.h"
#include "loader/gltf_meshes.h"
#include "resources/buffers.h"
#include "scene/node.h"
#include "core/state.h"
#include <iostream>

void createMeshBuffers(State* state, Node* node) {
	for (Mesh* mesh : node->meshes) {
		std::cout << "createMeshBuffers: node=" << node->name
			<< " verts=" << mesh->vertices.size()
			<< " idx=" << mesh->indices.size() << "\n";

		if (!mesh->vertices.empty()) {
			vertexBufferCreateForMesh(state, mesh->vertices, mesh->vertexBuffer, mesh->vertexMemory);
			std::cout << "  -> VBO created: " << (mesh->vertexBuffer != VK_NULL_HANDLE) << "\n";
		}
		if (!mesh->indices.empty()) {
			indexBufferCreateForMesh(state, mesh->indices, mesh->indexBuffer, mesh->indexMemory);
			std::cout << "  -> IBO created: " << (mesh->indexBuffer != VK_NULL_HANDLE) << "\n";
		}
	}

	for (Node* child : node->children) {
		createMeshBuffers(state, child);
	}
}