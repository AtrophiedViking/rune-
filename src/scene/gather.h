#pragma once
#include "core/math.h"
struct Node;
struct Mesh;
struct Material;

struct DrawItem {
	const Node* node;
	const Mesh* mesh;
	float distanceToCamera;
	bool transparent;
};

void gatherDrawItems(const Node* root, const glm::vec3& camPos, const std::vector<Material*>& materials, std::vector<DrawItem>& out);
