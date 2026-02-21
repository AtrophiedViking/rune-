#define _CRT_SECURE_NO_WARNINGS
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <vulkan/vulkan.h>
#include <tiny_gltf.h>
#include "loader/gltf_loader.h"
#include "loader/gltf_textures.h"
#include "loader/gltf_nodes.h"
#include "loader/gltf_meshes.h"
#include "resources/images.h"
#include "resources/buffers.h"
#include "scene/texture.h"
#include "scene/materials.h"
#include "scene/mesh.h"
#include "scene/model.h"
#include "scene/scene.h"
#include "core/config.h"
#include "core/context.h"
#include "core/state.h"
#include <vector>

//Loading
void modelLoad(State* state, std::string modelPath)
{
	// Use tinygltf to load the model instead of tinyobjloader
	Model* model = new Model{};

	tinygltf::Model    gltfModel;
	tinygltf::TinyGLTF loader;
	std::string        err;
	std::string        warn;

	// Detect file extension to determine which loader to use
	bool ret = false;
	std::string extension = modelPath.substr(modelPath.find_last_of(".") + 1);
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

	if (extension == "glb") {
		ret = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, modelPath);
	}
	else if (extension == "gltf") {
		ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, modelPath);
	}
	else {
		err = "Unsupported file extension: " + extension + ". Expected .gltf or .glb";
	}

	if (!warn.empty())
	{
		std::cout << "glTF warning: " << warn << std::endl;
	}

	if (!err.empty())
	{
		std::cout << "glTF error: " << err << std::endl;
	}

	if (!ret)
	{
		throw std::runtime_error("Failed to load glTF model");
	}
	model->rootNode = new Node();
	model->rootNode->name = "Root";

	std::vector<int> textureToImage;
	textureToImage.reserve(gltfModel.textures.size());

	for (const auto& gltfTex : gltfModel.textures) {
		textureToImage.push_back(gltfTex.source);
	}

	std::string baseDir = "";
	size_t lastSlashPos = modelPath.find_last_of("/\\");
	if (lastSlashPos != std::string::npos) {
		baseDir = modelPath.substr(0, lastSlashPos + 1);
	};

	model->baseMaterialIndex = static_cast<uint32_t>(state->scene->materials.size());
	model->baseTextureIndex = static_cast<uint32_t>(state->scene->textures.size());
	state->scene->materials.reserve(gltfModel.materials.size());

	std::unordered_map<int, TextureRole> textureRoles;

	for (const auto& m : gltfModel.materials) {
		Material* mat = new Material{};
		// Base color factor
		if (m.pbrMetallicRoughness.baseColorFactor.size() == 4) {
			mat->baseColorFactor = glm::vec4(
				m.pbrMetallicRoughness.baseColorFactor[0],
				m.pbrMetallicRoughness.baseColorFactor[1],
				m.pbrMetallicRoughness.baseColorFactor[2],
				m.pbrMetallicRoughness.baseColorFactor[3]
			);
		}
		// Alpha mode
		if (m.alphaMode == "MASK")
			mat->alphaMode = "MASK";
		else if (m.alphaMode == "BLEND")
			mat->alphaMode = "BLEND";
		else
			mat->alphaMode = "OPAQUE";

		// Alpha cutoff
		if (m.alphaCutoff > 0.0f)
			mat->alphaCutoff = (float)m.alphaCutoff;

		// Double-sided
		mat->doubleSided = m.doubleSided;

		// Base color texture
		if (m.pbrMetallicRoughness.baseColorTexture.index >= 0) {
			mat->baseColorTextureIndex =
				model->baseTextureIndex + m.pbrMetallicRoughness.baseColorTexture.index;

			readTextureTransform(
				m.pbrMetallicRoughness.baseColorTexture,
				mat->baseColorTransform
			);

			textureRoles[mat->baseColorTextureIndex] = TextureRole::BaseColor;
		}

		// Metallic-roughness texture
		if (m.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
			mat->metallicRoughnessTextureIndex =
				model->baseTextureIndex + m.pbrMetallicRoughness.metallicRoughnessTexture.index;

			readTextureTransform(
				m.pbrMetallicRoughness.metallicRoughnessTexture,
				mat->metallicRoughnessTransform
			);

			textureRoles[mat->metallicRoughnessTextureIndex] = TextureRole::MetallicRoughness;
		}

		// Normal texture
		if (m.normalTexture.index >= 0) {
			mat->normalTextureIndex =
				model->baseTextureIndex + m.normalTexture.index;

			readTextureTransform(
				m.normalTexture,
				mat->normalTransform
			);

			textureRoles[mat->normalTextureIndex] = TextureRole::Normal;
		}

		// Occlusion texture
		if (m.occlusionTexture.index >= 0) {
			mat->occlusionTextureIndex =
				model->baseTextureIndex + m.occlusionTexture.index;

			readTextureTransform(
				m.occlusionTexture,
				mat->occlusionTransform
			);

			textureRoles[mat->occlusionTextureIndex] = TextureRole::Occlusion;
		}

		// Emissive texture
		if (m.emissiveTexture.index >= 0) {
			mat->emissiveTextureIndex =
				model->baseTextureIndex + m.emissiveTexture.index;

			readTextureTransform(
				m.emissiveTexture,
				mat->emissiveTransform
			);

			textureRoles[mat->emissiveTextureIndex] = TextureRole::Emissive;
		}



		// Scalars
		mat->metallicFactor = m.pbrMetallicRoughness.metallicFactor;
		mat->roughnessFactor = m.pbrMetallicRoughness.roughnessFactor;

		state->scene->materials.push_back(mat);
	}


	const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];
	for (int nodeIndex : scene.nodes) {
		// Process the node and its children recursively
		// (Implementation of node processing is omitted for brevity)
		tinygltf::Node node = gltfModel.nodes[nodeIndex];
		std::cout << "Loaded node: " << node.name << std::endl;
		processNode(gltfModel, node, model->rootNode, baseDir, model);
		gltfModel.nodes[nodeIndex] = node; // Update the node in the model with any changes made during processing
	}

	createMeshBuffers(state, model->rootNode);

	if (!gltfModel.images.empty()) {
		for (int i = 0; i < gltfModel.images.size(); i++) {
			const auto& image = gltfModel.images[i];

			Texture* tex = new Texture{};

			int globalIndex = model->baseTextureIndex + i;

			// Assign role BEFORE creating the texture
			if (textureRoles.count(globalIndex))
				tex->role = textureRoles[globalIndex];
			else
				tex->role = TextureRole::BaseColor; // safe default

			createTextureFromMemory(
				state,
				image.image.data(),
				image.image.size(),
				image.width,
				image.height,
				image.component,
				*tex
			);

			state->scene->textures.push_back(tex);
		}
	}
	else {
		// Fallback texture
		Texture* tex = new Texture{};
		textureImageCreate(state, state->config->KOBOLD_TEXTURE_PATH);
		textureImageViewCreate(state);
		textureSamplerCreate(state);

		tex->textureImageView = state->texture->textureImageView;
		tex->textureSampler = state->texture->textureSampler;

		state->scene->textures.push_back(tex);
	}

	state->scene->models.push_back(model);
}
void modelUnload(State* state)
{
	// 1. Destroy mesh buffers for every node in every model
	std::function<void(Node*)> cleanupNode = [&](Node* node)
		{
			for (Mesh* mesh : node->meshes)
			{
				if (mesh->vertexBuffer) {
					vkDestroyBuffer(state->context->device, mesh->vertexBuffer, nullptr);
					mesh->vertexBuffer = VK_NULL_HANDLE;
				}
				if (mesh->vertexMemory) {
					vkFreeMemory(state->context->device, mesh->vertexMemory, nullptr);
					mesh->vertexMemory = VK_NULL_HANDLE;
				}
				if (mesh->indexBuffer) {
					vkDestroyBuffer(state->context->device, mesh->indexBuffer, nullptr);
					mesh->indexBuffer = VK_NULL_HANDLE;
				}
				if (mesh->indexMemory) {
					vkFreeMemory(state->context->device, mesh->indexMemory, nullptr);
					mesh->indexMemory = VK_NULL_HANDLE;
				}
			}

			for (Node* child : node->children)
				if (child) cleanupNode(child);
		};

	// 2. Walk each model's node tree and clean GPU buffers ONLY
	for (Model* model : state->scene->models)
	{
		if (model->rootNode)
			cleanupNode(model->rootNode);
	}

	// 3. Clear models – this calls ~Model and deletes all nodes in linearNodes
	state->scene->models.clear();

	// 4. Destroy global material UBOs
	for (Material* mat : state->scene->materials)
	{
		if (mat->materialBuffer) {
			vkDestroyBuffer(state->context->device, mat->materialBuffer, nullptr);
			mat->materialBuffer = VK_NULL_HANDLE;
		}
		if (mat->materialMemory) {
			vkFreeMemory(state->context->device, mat->materialMemory, nullptr);
			mat->materialMemory = VK_NULL_HANDLE;
		}
	}
	state->scene->materials.clear();

	// 5. Destroy global textures
	for (Texture* tex : state->scene->textures)
	{
		if (tex->textureImageView)
			vkDestroyImageView(state->context->device, tex->textureImageView, nullptr);
		if (tex->textureSampler)
			vkDestroySampler(state->context->device, tex->textureSampler, nullptr);
		if (tex->textureImage)
			vkDestroyImage(state->context->device, tex->textureImage, nullptr);
		if (tex->textureImageMemory)
			vkFreeMemory(state->context->device, tex->textureImageMemory, nullptr);
	}
	state->scene->textures.clear();

	// 6. Fallback texture if you still use one
	textureImageDestroy(state);
}