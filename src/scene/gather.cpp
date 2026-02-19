#include "scene/gather.h"
#include "scene/node.h"
#include "scene/materials.h"
#include "scene/mesh.h"

#include "core/state.h"
void gatherDrawItems(const Node* root, const glm::vec3& camPos, const std::vector<Material*>& materials, std::vector<DrawItem>& out) {
	std::function<void(const Node*)> recurse = [&](const Node* node) {

		// FIX: extract translation from global matrix
		glm::mat4 M = node->getGlobalMatrix();
		glm::vec3 worldPos = glm::vec3(M[3]);
		float dist = glm::length(worldPos - camPos);

		for (const Mesh* mesh : node->meshes) {

			bool isTransparent = false;

			if (mesh->materialIndex >= 0 && mesh->materialIndex < (int)materials.size()) {
				const auto& mat = materials[mesh->materialIndex];

				if (mat->alphaMode == "BLEND")
					isTransparent = true;
				else if (mat->alphaMode == "MASK")
					isTransparent = false; // still opaque, but alpha-tested
				else if (mat->baseColorFactor.a < 1.0f)
					isTransparent = true;
			};

			out.push_back({
				.node = node,
				.mesh = mesh,
				.distanceToCamera = dist,
				.transparent = isTransparent
				});

		}

		for (const Node* child : node->children) {
			recurse(child);
		}
		};
	recurse(root);
}