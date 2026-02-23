#pragma once
#include "vulkan/vulkan.h"
#include <string>

namespace tinygltf{
	class Model;
	class Node;
}

struct Node;
struct Model;


void processNode(const tinygltf::Model& gltf, const tinygltf::Node& node, Node* parent, const std::string& baseDir, Model* model);
void parseSceneNodes(const tinygltf::Model& gltf, Model* model, const std::string& baseDir);