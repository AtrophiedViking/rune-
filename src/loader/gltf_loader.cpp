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
#include "loader/gltf_materials.h"
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
//Utility
std::string extractBaseDir(const std::string& path){
	size_t pos = path.find_last_of("/\\");
	if (pos == std::string::npos)
		return "";
	return path.substr(0, pos + 1);
}
//Loading
void loadModel(State* state, std::string modelPath)
{
	Model* model = new Model{};
	model->rootNode = new Node();
	model->rootNode->name = "Root";
	tinygltf::Model gltf = loadGltf(modelPath);
	std::unordered_map<int, TextureRole> textureRoles;
	parseMaterials(state, model, gltf, textureRoles);
	std::string baseDir = extractBaseDir(modelPath);
	parseSceneNodes(gltf, model, baseDir);
	createMeshBuffers(state, model->rootNode);
	createModelTextures(state, model, gltf, textureRoles);
	state->scene->models.push_back(model);
};
tinygltf::Model loadGltf(std::string modelPath) {
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string        err;
	std::string        warn;

	// Detect file extension to determine which loader to use
	bool ret = false;
	std::string extension = modelPath.substr(modelPath.find_last_of(".") + 1);
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

	if (extension == "glb") {
		ret = loader.LoadBinaryFromFile(&model, &err, &warn, modelPath);
	}
	else if (extension == "gltf") {
		ret = loader.LoadASCIIFromFile(&model, &err, &warn, modelPath);
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
	return model;
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