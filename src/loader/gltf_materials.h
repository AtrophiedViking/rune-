#pragma once
#include "core/math.h"
namespace tinygltf{
	class Model;
	class Material;
};
enum TextureRole;
struct Material;
struct Model;
struct State;

void fillMaterialFromGltf(const tinygltf::Material& m, Material& mat, uint32_t baseTextureIndex, std::unordered_map<int, TextureRole>& roles);

void parseMaterials(State* state, Model* model, const tinygltf::Model& gltf, std::unordered_map<int, TextureRole>& textureRoles);