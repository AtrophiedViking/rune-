#pragma once
#include "core/math.h"
struct Node;
struct Mesh;
struct Material;
struct Model;

struct DrawItem {
	const Node* node;
	const Mesh* mesh;
	const Model* model;
	float distanceToCamera;
	bool transparent;
};

void gatherDrawItems(const Node* root,
	const glm::vec3& camPos,
	const std::vector<Material*>& materials,
	Model* model,
	std::vector<DrawItem>& out);