#pragma once
#include "core/math.h"
struct State;
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

void gatherDrawItems(
	const Node* node,
	const glm::vec3& camPos,
	const std::vector<Material*>& materials,
	Model* model,
	std::vector<DrawItem>& outItems);