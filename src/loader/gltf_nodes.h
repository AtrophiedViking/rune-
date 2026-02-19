#pragma once
#include <string>

namespace tinygltf{
	class Model;
	class Node;
}

struct Node;
struct Model;


void processNode(tinygltf::Model& gltfModel, tinygltf::Node& node, Node* parent, const std::string& baseDir, Model* model);