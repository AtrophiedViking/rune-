#pragma once
#include <string>
namespace tinygltf {
	class Model;
};
struct State;
//Utility
std::string extractBaseDir(const std::string& path);
//Loading
void loadModel(State* state, std::string modelPath);
tinygltf::Model loadGltf(std::string modelPath);
void modelUnload(State* state);