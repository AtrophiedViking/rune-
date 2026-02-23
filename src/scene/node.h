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


	bool hasMatrix() const
	{
		const float eps = 1e-6f;
		glm::mat4 I(1.0f);

		for (int r = 0; r < 4; r++)
		{
			for (int c = 0; c < 4; c++)
			{
				if (!glm::epsilonEqual(matrix[r][c], I[r][c], eps))
					return true; // matrix differs from identity
			}
		}
		return false; // matrix is identity → no baked matrix
	}

	glm::mat4 getLocalMatrix() const
	{
		if (hasMatrix())
			return matrix;   // glTF rule: matrix overrides TRS

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