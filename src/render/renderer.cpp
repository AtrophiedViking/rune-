#include "render/renderer.h"
#include "resources/buffers.h"
#include "scene/node.h"
#include "scene/mesh.h"
#include "scene/texture.h"
#include "scene/materials.h"
#include "scene/scene.h"
#include "scene/gather.h"
#include "core/state.h"

void drawMesh(State* state, VkCommandBuffer cmd, const Mesh* mesh, const glm::mat4& nodeMatrix, const glm::mat4& modelTransform)
{
	const Material* mat = state->scene->materials[mesh->materialIndex];

	// Bind descriptor set for this material (set = 1)
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		state->renderer->pipelineLayout,
		1, // set = 1
		1,
		&mat->descriptorSet,
		0,
		nullptr
	);

	// Push constants
	PushConstantBlock pcb{};
	pcb.nodeMatrix = modelTransform * nodeMatrix;
	pcb.baseColorFactor = mat->baseColorFactor;
	pcb.metallicFactor = mat->metallicFactor;
	pcb.roughnessFactor = mat->roughnessFactor;

	pcb.baseColorTextureSet = 0;
	pcb.physicalDescriptorTextureSet = 1;
	pcb.normalTextureSet = 2;
	pcb.occlusionTextureSet = 3;
	pcb.emissiveTextureSet = 4;
	pcb.alphaMask = (mat->alphaMode == "MASK") ? 1.0f : 0.0f;
	pcb.alphaMaskCutoff = mat->alphaCutoff;
	pcb.transmissionFactor = mat->transmissionFactor;
	pcb.transmissionTextureIndex = mat->transmissionTextureIndex;
	pcb.transmissionTexCoordIndex = mat->transmissionTexCoordIndex;
	// Volume
	pcb.thicknessFactor = mat->thicknessFactor;
	pcb.thicknessTextureIndex = mat->thicknessTextureIndex;
	pcb.thicknessTexCoordIndex = mat->thicknessTexCoordIndex;
	pcb.attenuation = mat->attenuationColor;
	pcb.attenuation.w = mat->attenuationDistance;



	vkCmdPushConstants(
		cmd,
		state->renderer->pipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(PushConstantBlock),
		&pcb
	);

	// Bind vertex + index buffers
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertexBuffer, offsets);
	vkCmdBindIndexBuffer(cmd, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdDrawIndexed(cmd, mesh->indices.size(), 1, 0, 0, 0);
}