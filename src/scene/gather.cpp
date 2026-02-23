#include "scene/gather.h"
#include "scene/node.h"
#include "scene/model.h"
#include "scene/materials.h"
#include "scene/mesh.h"

#include "core/state.h"
void gatherDrawItems(const Node* root,
    const glm::vec3& camPos,
    const std::vector<Material*>& materials,
    Model* model,
    std::vector<DrawItem>& out)
{
    std::function<void(const Node*)> recurse = [&](const Node* node)
        {
            glm::mat4 M = node->getGlobalMatrix();

            for (const Mesh* mesh : node->meshes)
            {
                // ✔ FIX: use mesh center instead of node origin
                glm::vec3 meshCenterWorld =
                    glm::vec3(M * glm::vec4(mesh->center, 1.0f));

                float dist = glm::length(meshCenterWorld - camPos);

                bool isTransparent = false;

                if (mesh->materialIndex >= 0 &&
                    mesh->materialIndex < (int)materials.size())
                {
                    const Material* mat = materials[mesh->materialIndex];

                    // Alpha-based transparency
                    if (mat->alphaMode == "BLEND")
                        isTransparent = true;
                    else if (mat->alphaMode == "MASK")
                        isTransparent = false;
                    else if (mat->baseColorFactor.a < 1.0f)
                        isTransparent = true;

                    // Transmission-based transparency
                    bool isTransmissive =
                        (mat->transmissionFactor > 0.0f) ||
                        (mat->transmissionTextureIndex >= 0);

                    if (isTransmissive)
                        isTransparent = true;
                }

                out.push_back({
                    .node = node,
                    .mesh = mesh,
                    .model = model,
                    .distanceToCamera = dist,
                    .transparent = isTransparent
                    });
            }

            for (const Node* child : node->children)
                recurse(child);
        };

    recurse(root);
}