#include "loader/gltf_materials.h"
#include "loader/gltf_textures.h"
#include "loader/gltf_extensions.h"
#include "tiny_gltf.h"
#include "scene/texture.h"
#include "scene/materials.h"
#include "scene/model.h"
#include "scene/scene.h"
#include "core/state.h"

void fillMaterialFromGltf(const tinygltf::Material& m, Material& mat,
    uint32_t baseTextureIndex,
    std::unordered_map<int, TextureRole>& roles)
{
    // Base color factor
    if (m.pbrMetallicRoughness.baseColorFactor.size() == 4)
    {
        mat.baseColorFactor = glm::vec4(
            m.pbrMetallicRoughness.baseColorFactor[0],
            m.pbrMetallicRoughness.baseColorFactor[1],
            m.pbrMetallicRoughness.baseColorFactor[2],
            m.pbrMetallicRoughness.baseColorFactor[3]
        );
    }

    // Alpha mode
    mat.alphaMode =
        (m.alphaMode == "MASK") ? "MASK" :
        (m.alphaMode == "BLEND") ? "BLEND" :
        "OPAQUE";

    mat.alphaCutoff = (float)m.alphaCutoff;
    mat.doubleSided = m.doubleSided;

    // ─────────────────────────────────────────────
    // BaseColor
    // ─────────────────────────────────────────────
    if (m.pbrMetallicRoughness.baseColorTexture.index >= 0)
    {
        int idx = m.pbrMetallicRoughness.baseColorTexture.index;
        mat.baseColorTextureIndex = baseTextureIndex + idx;
        mat.baseColorTexCoordIndex = m.pbrMetallicRoughness.baseColorTexture.texCoord;

        readTextureTransform(m.pbrMetallicRoughness.baseColorTexture,
            mat.baseColorTransform);

        roles[mat.baseColorTextureIndex] = TextureRole::BaseColor;
    }
    else
    {
        mat.baseColorTextureIndex = -1;
        mat.baseColorTexCoordIndex = -1;   // <── IMPORTANT
    }

    // ─────────────────────────────────────────────
    // Metallic-Roughness
    // ─────────────────────────────────────────────
    if (m.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
    {
        int idx = m.pbrMetallicRoughness.metallicRoughnessTexture.index;
        mat.metallicRoughnessTextureIndex = baseTextureIndex + idx;
        mat.metallicRoughnessTexCoordIndex =
            m.pbrMetallicRoughness.metallicRoughnessTexture.texCoord;

        readTextureTransform(m.pbrMetallicRoughness.metallicRoughnessTexture,
            mat.metallicRoughnessTransform);

        roles[mat.metallicRoughnessTextureIndex] = TextureRole::MetallicRoughness;
    }
    else
    {
        mat.metallicRoughnessTextureIndex = -1;
        mat.metallicRoughnessTexCoordIndex = -1;
    }

    // ─────────────────────────────────────────────
    // Normal
    // ─────────────────────────────────────────────
    if (m.normalTexture.index >= 0)
    {
        int idx = m.normalTexture.index;
        mat.normalTextureIndex = baseTextureIndex + idx;
        mat.normalTexCoordIndex = m.normalTexture.texCoord;

        readTextureTransform(m.normalTexture, mat.normalTransform);

        roles[mat.normalTextureIndex] = TextureRole::Normal;
    }
    else
    {
        mat.normalTextureIndex = -1;
        mat.normalTexCoordIndex = -1;
    }

    // ─────────────────────────────────────────────
    // Occlusion
    // ─────────────────────────────────────────────
    if (m.occlusionTexture.index >= 0)
    {
        int idx = m.occlusionTexture.index;
        mat.occlusionTextureIndex = baseTextureIndex + idx;
        mat.occlusionTexCoordIndex = m.occlusionTexture.texCoord;

        readTextureTransform(m.occlusionTexture, mat.occlusionTransform);

        roles[mat.occlusionTextureIndex] = TextureRole::Occlusion;
    }
    else
    {
        mat.occlusionTextureIndex = -1;
        mat.occlusionTexCoordIndex = -1;
    }

    // ─────────────────────────────────────────────
    // Emissive
    // ─────────────────────────────────────────────
    if (m.emissiveTexture.index >= 0)
    {
        int idx = m.emissiveTexture.index;
        mat.emissiveTextureIndex = baseTextureIndex + idx;
        mat.emissiveTexCoordIndex = m.emissiveTexture.texCoord;

        readTextureTransform(m.emissiveTexture, mat.emissiveTransform);

        roles[mat.emissiveTextureIndex] = TextureRole::Emissive;
    }
    else
    {
        mat.emissiveTextureIndex = -1;
        mat.emissiveTexCoordIndex = -1;
    }

    // Scalars
    mat.metallicFactor = m.pbrMetallicRoughness.metallicFactor;
    mat.roughnessFactor = m.pbrMetallicRoughness.roughnessFactor;

    // Extensions
    parseTransmissionExtension(m, mat, baseTextureIndex, roles);
    parseVolumeExtension(m, mat, baseTextureIndex, roles);
}



void parseMaterials(State *state, Model* model, const tinygltf::Model& gltf, std::unordered_map<int, TextureRole>& textureRoles) {
    model->baseMaterialIndex = static_cast<uint32_t>(state->scene->materials.size());
    model->baseTextureIndex = static_cast<uint32_t>(state->scene->textures.size());

    state->scene->materials.reserve(
        state->scene->materials.size() + gltf.materials.size()
    );

    for (size_t i = 0; i < gltf.materials.size(); i++)
    {
        const tinygltf::Material& gm = gltf.materials[i];

        Material* mat = new Material{};
        fillMaterialFromGltf(gm, *mat, model->baseTextureIndex, textureRoles);

        state->scene->materials.push_back(mat);
    }

};
