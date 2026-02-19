#pragma once
#include <vector>
struct Model;
struct Texture;
struct Material;
struct Camera;

struct Scene {
	int defaultTextureIndex = 0;

	std::vector<Model*> models;
	std::vector<Texture*> textures;
	std::vector<Material*> materials;
	Camera *camera;
};