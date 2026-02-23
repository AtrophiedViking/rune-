#pragma once
#include "vulkan/vulkan.h"
#include "core/math.h"

namespace tinygltf {
    class Material;
};

enum TextureRole;
struct Material;

void parseTransmissionExtension(const tinygltf::Material& m, Material& mat, uint32_t baseTextureIndex, std::unordered_map<int, TextureRole>& roles);
void parseVolumeExtension(const tinygltf::Material& m, Material& mat, uint32_t baseTextureIndex, std::unordered_map<int, TextureRole>& roles);
