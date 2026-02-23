#pragma once
#include "scene/texture.h"
struct Material {
    // glTF indices into scene.textures
    int baseColorTextureIndex = -1;
    int metallicRoughnessTextureIndex = -1;
    int normalTextureIndex = -1;
    int occlusionTextureIndex = -1;
    int emissiveTextureIndex = -1;
    int transmissionTextureIndex = -1;
    int transmissionTexCoordIndex = 0;
    int thicknessTextureIndex = -1;
    int thicknessTexCoordIndex = 0;

    // NEW: per-texture texCoord indices (glTF textureInfo.texCoord)
    int baseColorTexCoordIndex = 0;
    int metallicRoughnessTexCoordIndex = 0;
    int normalTexCoordIndex = 0;
    int occlusionTexCoordIndex = 0;
    int emissiveTexCoordIndex = 0;

    // Per-texture transforms + texCoord selection
    TextureTransform baseColorTransform;
    TextureTransform metallicRoughnessTransform;
    TextureTransform normalTransform;
    TextureTransform occlusionTransform;
    TextureTransform emissiveTransform;
    TextureTransform transmissionTransform;

    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    float     metallicFactor = 1.0f;
    float     roughnessFactor = 1.0f;
    glm::vec3 emissiveFactor = glm::vec3(0.0f);
    float     transmissionFactor = 0.0f;
    float     thicknessFactor = 0.0f;

    std::string alphaMode = "OPAQUE";
    float alphaCutoff = 0.1f;
    bool  doubleSided = false;

    glm::vec4 attenuationColor = glm::vec4(1.0f);
    float      attenuationDistance = 1e6f;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkBuffer        materialBuffer = VK_NULL_HANDLE;
    VkDeviceMemory  materialMemory = VK_NULL_HANDLE;
};