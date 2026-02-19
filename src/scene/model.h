#pragma once

#include "scene/node.h"
#include "scene/materials.h"
#include "scene/animation.h"
#include "core/math.h"


struct Model {
	std::string name;
	std::vector<Node*> nodes;
	Node* rootNode = nullptr;
	std::vector<Node*> linearNodes;
	std::vector<Animation> animations;
	glm::mat4 transform = glm::mat4(1.0f);

	uint32_t baseMaterialIndex = 0;
	uint32_t baseTextureIndex = 0;

	~Model() {
		for (auto node : linearNodes) {
			delete node;
		}
	}

	void translate(const glm::vec3& delta) {
		transform = glm::translate(transform, delta);
	}

	void rotateEuler(const glm::vec3& eulerDegrees) {
		glm::vec3 r = glm::radians(eulerDegrees);
		glm::quat q = glm::quat(r);
		transform = transform * glm::mat4_cast(q);
	}

	void scaleBy(const glm::vec3& s) {
		transform = transform * glm::scale(glm::mat4(1.0f), s);
	}

	void setPosition(const glm::vec3& pos) {
		transform = glm::translate(glm::mat4(1.0f), pos);
	}

	void setScale(const glm::vec3& s) {
		transform = glm::scale(glm::mat4(1.0f), s);
	}

	void setUniformScale(float s) {
		transform = glm::scale(glm::mat4(1.0f), glm::vec3(s));
	}

	void setRotationEuler(const glm::vec3& eulerDegrees) {
		glm::vec3 r = glm::radians(eulerDegrees);
		glm::quat q = glm::quat(r);
		transform = glm::mat4_cast(q);
	}

	void setTransform(const glm::vec3& pos,
		const glm::vec3& eulerDegrees,
		const glm::vec3& scale)
	{
		glm::mat4 T = glm::translate(glm::mat4(1.0f), pos);
		glm::mat4 R = glm::mat4_cast(glm::quat(glm::radians(eulerDegrees)));
		glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

		transform = T * R * S;
	}

	Node* findNode(const std::string& name) {
		auto nodeIt = std::ranges::find_if(linearNodes, [&name](auto const& node) {
			return node->name == name;
			});
		return (nodeIt != linearNodes.end()) ? *nodeIt : nullptr;
	}

	void updateAnimation(uint32_t index, float deltaTime) {
		assert(!animations.empty() && index < animations.size());



		Animation& animation = animations[index];
		animation.currentTime += deltaTime;
		if (animation.currentTime > animation.end) {
			animation.currentTime = animation.start;
		}

		for (auto& channel : animation.channels) {
			AnimationSampler& sampler = animation.samplers[channel.samplerIndex];

			// Find the current key frame using binary search
			auto keyFrameIt = std::ranges::lower_bound(sampler.inputs, animation.currentTime);
			if (keyFrameIt != sampler.inputs.end() && keyFrameIt != sampler.inputs.begin()) {
				size_t i = std::distance(sampler.inputs.begin(), keyFrameIt) - 1;
				float t = (animation.currentTime - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);

				switch (channel.path) {
				case AnimationChannel::TRANSLATION: {
					glm::vec3 start = sampler.outputsVec3[i];
					glm::vec3 end = sampler.outputsVec3[i + 1];
					channel.node->translation = glm::mix(start, end, t);
					break;
				}
				case AnimationChannel::ROTATION: {
					glm::quat start = glm::quat(sampler.outputsVec4[i].w, sampler.outputsVec4[i].x, sampler.outputsVec4[i].y, sampler.outputsVec4[i].z);
					glm::quat end = glm::quat(sampler.outputsVec4[i + 1].w, sampler.outputsVec4[i + 1].x, sampler.outputsVec4[i + 1].y, sampler.outputsVec4[i + 1].z);
					channel.node->rotation = glm::slerp(start, end, t);
					break;
				}
				case AnimationChannel::SCALE: {
					glm::vec3 start = sampler.outputsVec3[i];
					glm::vec3 end = sampler.outputsVec3[i + 1];
					channel.node->scale = glm::mix(start, end, t);
					break;
				}
				}
				break;
			}
		}
	}
};