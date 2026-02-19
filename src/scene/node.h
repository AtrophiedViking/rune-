#pragma once
#include "scene/mesh.h"
#include "core/math.h"
#include <string>
struct Node {
	std::string name;
	Node* parent = nullptr;
	std::vector<Node*> children;
	std::vector<Mesh*>meshes;
	glm::mat4 matrix = glm::mat4(1.0f);

	// For animation
	glm::vec3 translation = glm::vec3(0.0f);
	glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 scale = glm::vec3(1.0f);


	bool hasBakedMatrix() const {
		const float eps = 1e-6f;
		glm::mat4 identity(1.0f);

		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 4; c++) {
				if (fabs(matrix[r][c] - identity[r][c]) > eps)
					return true; // matrix differs from identity
			}
		}
		return false; // matrix is effectively identity
	}

	glm::mat4 getLocalMatrix() const {
		// If glTF provided a meaningful baked matrix, use it
		if (hasBakedMatrix()) {
			return matrix;
		}

		glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
		glm::mat4 R = glm::mat4_cast(rotation);
		glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
		return T * R * S;
	}

	glm::mat4 getGlobalMatrix() const {
		if (parent)
			return parent->getGlobalMatrix() * getLocalMatrix();
		else
			return getLocalMatrix();
	}

};