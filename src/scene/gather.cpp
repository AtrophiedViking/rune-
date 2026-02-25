#include "scene/gather.h"
#include "scene/node.h"
#include "scene/model.h"
#include "scene/materials.h"
#include "scene/mesh.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "core/state.h"
void gatherDrawItems(
    const Node* node,
    const glm::vec3& camPos,
    const std::vector<Material*>& materials,
    Model* model,
    std::vector<DrawItem>& outItems)
{
    // World matrix for this node
    glm::mat4 nodeWorld = model->transform * node->getGlobalMatrix();

    // For each mesh on this node
    for (const Mesh* mesh : node->meshes) {
        const Material* mat = materials[mesh->materialIndex];

        // World-space center of the mesh
        glm::vec3 centerWorld =
            glm::vec3(nodeWorld * glm::vec4(mesh->center, 1.0f));

        // World-space distance to camera
        float dist = glm::length(centerWorld - camPos);

        // Transparent if glTF alphaMode == "BLEND" or has transmission
        bool isTransparent =
            (mat->alphaMode == "BLEND") ||
            (mat->transmissionFactor > 0.0f) ||
            (mat->transmissionTextureIndex >= 0);

        DrawItem item{};
        item.model = model;
        item.node = node;
        item.mesh = mesh;
        item.distanceToCamera = dist;
        item.transparent = isTransparent;

        outItems.push_back(item);
    }

    // Recurse children
    for (const Node* child : node->children) {
        gatherDrawItems(child, camPos, materials, model, outItems);
    }
}
