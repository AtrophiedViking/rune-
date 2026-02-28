#include "resources/buffers.h"
#include "render/descriptors.h"
#include "render/gpu_material.h"
#include "render/renderer.h"
#include "scene/scene.h"
#include "scene/materials.h"
#include "scene/texture.h"
#include "scene/model.h"
#include "scene/camera.h"
#include "core/context.h"
#include "core/config.h"
#include "core/state.h"

VkResult allocateDescriptorSetsWithResize(State* state,	const VkDescriptorSetAllocateInfo* allocInfo, VkDescriptorSet* sets)
{
	VkResult result = vkAllocateDescriptorSets(state->context->device, allocInfo, sets);

	if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
		// Resize pool
		globalDescriptorPoolDestroy(state);
		globalDescriptorPoolCreate(state); // Recreate with larger size

		// Retry allocation
		result = vkAllocateDescriptorSets(state->context->device, allocInfo, sets);
	}

	return result;
}

// set 0: global UBO
void globalSetLayoutCreate(State* state) {

	VkDescriptorSetLayoutBinding uboBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr
	};
	VkDescriptorSetLayoutBinding cubemapBinding{
	.binding = 1,
	.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	.descriptorCount = 1,
	.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	.pImmutableSamplers = nullptr
	};
	VkDescriptorSetLayoutBinding colorBinding{
	.binding = 2,
	.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	.descriptorCount = 1,
	.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	.pImmutableSamplers = nullptr
	};
	VkDescriptorSetLayoutBinding depthBinding{
	.binding = 3,
	.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	.descriptorCount = 1,
	.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	.pImmutableSamplers = nullptr
	};

	std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
		uboBinding,
		cubemapBinding,
		colorBinding,
		depthBinding
	};

	VkDescriptorSetLayoutCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};

	PANIC(
		vkCreateDescriptorSetLayout(state->context->device, &info, nullptr, &state->renderer->globalSetLayout),
		"Failed to create global UBO set layout"
	);
}
void globalSetLayoutDestroy(State* state) {
	vkDestroyDescriptorSetLayout(state->context->device, state->renderer->globalSetLayout, nullptr);
};
void globalDescriptorPoolCreate(State* state)
{
	uint32_t frames = state->config->swapchainBuffering;

	// envMap + sceneColor + sceneDepth
	uint32_t globalImageDescriptors =
		frames * 3 * state->renderer->descriptorPoolMultiplier;

	// present: scene + accum + reveal
	uint32_t presentImageDescriptors = 3;

	uint32_t imageDescriptorCount =
		globalImageDescriptors + presentImageDescriptors;

	uint32_t uboDescriptorCount =
		frames * state->renderer->descriptorPoolMultiplier;

	std::array<VkDescriptorPoolSize, 2> poolSizes{
		VkDescriptorPoolSize{
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			uboDescriptorCount
		},
		VkDescriptorPoolSize{
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			imageDescriptorCount
		}
	};

	uint32_t totalSets =
		(frames + 1) * state->renderer->descriptorPoolMultiplier; // frames global + 1 present

	VkDescriptorPoolCreateInfo poolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = 0,
		.maxSets = totalSets,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data(),
	};

	PANIC(
		vkCreateDescriptorPool(
			state->context->device,
			&poolInfo,
			nullptr,
			&state->renderer->globalDescriptorPool
		),
		"Failed to create global descriptor pool!"
	);

}
void globalDescriptorPoolDestroy(State* state)
{
	// 1. Destroy per-material UBOs
	for (Material* mat : state->scene->materials)
	{
		if (mat->materialBuffer != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(state->context->device, mat->materialBuffer, nullptr);
			mat->materialBuffer = VK_NULL_HANDLE;
		}

		if (mat->materialMemory != VK_NULL_HANDLE)
		{
			vkFreeMemory(state->context->device, mat->materialMemory, nullptr);
			mat->materialMemory = VK_NULL_HANDLE;
		}
	}

	// 2. Destroy descriptor pool
	if (state->renderer->globalDescriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(state->context->device,
			state->renderer->globalDescriptorPool,
			nullptr);
		state->renderer->globalDescriptorPool = VK_NULL_HANDLE;
	}
}
void globalSetsCreate(State* state)
{
	uint32_t frames = state->config->swapchainBuffering;

	state->renderer->globalSets.resize(frames);

	std::vector<VkDescriptorSetLayout> layouts(
		frames,
		state->renderer->globalSetLayout
	);

	VkDescriptorSetAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = state->renderer->globalDescriptorPool,
		.descriptorSetCount = frames,
		.pSetLayouts = layouts.data()
	};

	PANIC(
		vkAllocateDescriptorSets(
			state->context->device,
			&allocInfo,
			state->renderer->globalSets.data()
		),
		"Failed to allocate global UBO descriptor sets!"
	);

	for (uint32_t i = 0; i < frames; i++)
	{
		// ─────────────────────────────────────────────
		// Binding 0: UBO
		// ─────────────────────────────────────────────
		VkDescriptorBufferInfo bufferInfo{
			.buffer = state->renderer->uniformBuffers[i],
			.offset = 0,
			.range = sizeof(UniformBufferObject)
		};

		VkWriteDescriptorSet writeUBO{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = state->renderer->globalSets[i],
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &bufferInfo
		};

		// ─────────────────────────────────────────────
		// Binding 1: Cube map sampler (envMap)
		// ─────────────────────────────────────────────
		VkDescriptorImageInfo cubeInfo{
			.sampler = state->texture->cubeSampler,
			.imageView = state->texture->cubeImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		VkWriteDescriptorSet writeCube{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = state->renderer->globalSets[i],
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &cubeInfo
		};

		// ─────────────────────────────────────────────
		// Binding 2: sceneColor sampler (opaque pass)
		// ─────────────────────────────────────────────
		VkDescriptorImageInfo sceneInfo{
			.sampler = state->texture->sceneColorSampler,
			.imageView = state->texture->sceneColorImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		VkWriteDescriptorSet writeScene{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = state->renderer->globalSets[i],
			.dstBinding = 2,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &sceneInfo
		};

		// ─────────────────────────────────────────────
		// Binding 3: sceneDepth sampler (opaque depth)
		// ─────────────────────────────────────────────
		VkDescriptorImageInfo depthInfo{
			.sampler = state->texture->sceneDepthSampler,      // your resolved depth sampler
			.imageView = state->texture->sceneDepthImageView,  // your resolved depth view
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		VkWriteDescriptorSet writeDepth{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = state->renderer->globalSets[i],
			.dstBinding = 3,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &depthInfo
		};

		// ─────────────────────────────────────────────
		// Submit all writes
		// ─────────────────────────────────────────────
		std::array<VkWriteDescriptorSet, 4> writes = {
			writeUBO,
			writeCube,
			writeScene,
			writeDepth
		};

		vkUpdateDescriptorSets(
			state->context->device,
			static_cast<uint32_t>(writes.size()),
			writes.data(),
			0,
			nullptr
		);
	}
}

// set 1: texture (for now, just baseColor at binding 0)
void materialSetLayoutCreate(State* state) {
	std::array<VkDescriptorSetLayoutBinding, 8> bindings{};

	
	// 0 — material UBO
	bindings[0] = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};

	// 1 — baseColor
	bindings[1] = {
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};

	// 1 — metallicRoughness
	bindings[2] = {
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};

	// 2 — occlusion
	bindings[3] = {
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};

	// 3 — emissive
	bindings[4] = {
		.binding = 4,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};

	// 4 — normal
	bindings[5] = {
		.binding = 5,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};


	// 6 — transmission texture
	bindings[6] = {
		.binding = 6,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};

	// 7 — volume texture
	bindings[7] = {
		.binding = 7,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};

	PANIC(
		vkCreateDescriptorSetLayout(
			state->context->device,
			&layoutInfo,
			nullptr,
			&state->renderer->materialSetLayout),
		"Failed to create texture set layout"
	);
}
void materialSetLayoutDestroy(State* state) {
	vkDestroyDescriptorSetLayout(state->context->device, state->renderer->materialSetLayout, nullptr);
}
void materialDescriptorPoolCreate(State* state)
{
	uint32_t materialCount = state->scene->materials.size();
	if (materialCount == 0) return;

	uint32_t materialImageDescriptors =
		materialCount * 7 * state->renderer->descriptorPoolMultiplier;  // 7 textures

	uint32_t materialUboDescriptors =
		materialCount * state->renderer->descriptorPoolMultiplier;      // 1 UBO per material

	std::array<VkDescriptorPoolSize, 2> poolSizes{
		VkDescriptorPoolSize{
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			materialUboDescriptors
		},
		VkDescriptorPoolSize{
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			materialImageDescriptors
		}
	};

	uint32_t totalSets =
		materialCount * state->renderer->descriptorPoolMultiplier;

	VkDescriptorPoolCreateInfo poolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.flags = 0,
		.maxSets = totalSets,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data(),
	};

	PANIC(
		vkCreateDescriptorPool(
			state->context->device,
			&poolInfo,
			nullptr,
			&state->renderer->materialDescriptorPool
		),
		"Failed to create material descriptor pool!"
	);
}
void materialDescriptorPoolDestroy(State* state)
{
	if (state->renderer->materialDescriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(state->context->device,
			state->renderer->materialDescriptorPool,
			nullptr);
		state->renderer->materialDescriptorPool = VK_NULL_HANDLE;
	}
}
void materialSetsCreate(State* state)
{
	uint32_t materialCount = state->scene->materials.size();
	if (materialCount == 0) return;

	for (Material* mat : state->scene->materials)
	{
		VkDescriptorSetAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = state->renderer->materialDescriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &state->renderer->materialSetLayout
		};

		VkResult result = allocateDescriptorSetsWithResize(state, &allocInfo, &mat->descriptorSet);
		PANIC(result, "Failed to allocate material descriptor set");

		auto resolveTex = [&](int index) -> const Texture& {
			if (index >= 0 && index < (int)state->scene->textures.size()) {
				return *state->scene->textures[index];
			}
				return *state->scene->textures[state->scene->defaultTextureIndex];
			};

		const Texture& baseTex = resolveTex(mat->baseColorTextureIndex);
		const Texture& mrTex = resolveTex(mat->metallicRoughnessTextureIndex);
		const Texture& occTex = resolveTex(mat->occlusionTextureIndex);
		const Texture& emisTex = resolveTex(mat->emissiveTextureIndex);
		const Texture& normTex = resolveTex(mat->normalTextureIndex);
		const Texture& transTex = resolveTex(mat->transmissionTextureIndex);
		const Texture& thickTex = resolveTex(mat->thicknessTextureIndex);

		
		// MaterialGPU UBO
		MaterialGPU gpu{};
		gpu.baseColorTT = toGPU(mat->baseColorTransform);
		gpu.mrTT = toGPU(mat->metallicRoughnessTransform);
		gpu.normalTT = toGPU(mat->normalTransform);
		gpu.occlusionTT = toGPU(mat->occlusionTransform);
		gpu.emissiveTT = toGPU(mat->emissiveTransform);

		// UV set indices (0 or 1)
		gpu.baseColorTT.rot_center_tex.w = static_cast<float>(mat->baseColorTexCoordIndex);
		gpu.mrTT.rot_center_tex.w = static_cast<float>(mat->metallicRoughnessTexCoordIndex);
		gpu.normalTT.rot_center_tex.w = static_cast<float>(mat->normalTexCoordIndex);
		gpu.occlusionTT.rot_center_tex.w = static_cast<float>(mat->occlusionTexCoordIndex);
		gpu.emissiveTT.rot_center_tex.w = static_cast<float>(mat->emissiveTexCoordIndex);

		gpu.thicknessFactor = mat->thicknessFactor;
		gpu.attenuationDistance = mat->attenuationDistance;
		gpu.attenuationColor = mat->attenuationColor;
		gpu.ior = mat->ior;

		createBufferForMaterial(
			state,
			sizeof(MaterialGPU),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			&mat->materialBuffer,
			&mat->materialMemory
		);

		void* data = nullptr;
		vkMapMemory(state->context->device, mat->materialMemory, 0, sizeof(MaterialGPU), 0, &data);
		memcpy(data, &gpu, sizeof(MaterialGPU));
		vkUnmapMemory(state->context->device, mat->materialMemory);

		VkDescriptorBufferInfo materialBufInfo{
			.buffer = mat->materialBuffer,
			.offset = 0,
			.range = sizeof(MaterialGPU)
		};

		// 6 textures (0–4 existing, 6 = transmission)
		std::array<VkDescriptorImageInfo, 7> infos{
			VkDescriptorImageInfo{ baseTex.textureSampler,  baseTex.textureImageView,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, // 0
			VkDescriptorImageInfo{ mrTex.textureSampler,    mrTex.textureImageView,    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, // 1
			VkDescriptorImageInfo{ occTex.textureSampler,   occTex.textureImageView,   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, // 2
			VkDescriptorImageInfo{ emisTex.textureSampler,  emisTex.textureImageView,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, // 3
			VkDescriptorImageInfo{ normTex.textureSampler,  normTex.textureImageView,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, // 4
			VkDescriptorImageInfo{ transTex.textureSampler, transTex.textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, // 5
			VkDescriptorImageInfo{ thickTex.textureSampler, thickTex.textureImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, // 6

		};

		// 7 writes: 0–4 textures, 5 UBO, 6 transmission
		std::array<VkWriteDescriptorSet, 8> writes{};

		// binding 5: material UBO
		writes[0] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = mat->descriptorSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &materialBufInfo
		};

		// bindings 0–4: existing textures
		for (uint32_t i = 1; i <= 7; ++i) {
			writes[i] = {
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = mat->descriptorSet,
				.dstBinding = i,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &infos[i-1]
			};
		}

		vkUpdateDescriptorSets(
			state->context->device,
			static_cast<uint32_t>(writes.size()),
			writes.data(),
			0, nullptr
		);
	}
}

// set 2: present (sceneColor, transAccum, transReveal)
void presentSetLayoutCreate(State* state) {
	VkDescriptorSetLayoutBinding sceneBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	};

	VkDescriptorSetLayoutBinding accumBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	};

	VkDescriptorSetLayoutBinding revealBinding{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	};

	std::array<VkDescriptorSetLayoutBinding, 3> bindings{
		sceneBinding, accumBinding, revealBinding
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data(),
	};

	PANIC(
		vkCreateDescriptorSetLayout(
			state->context->device,
			&layoutInfo,
			nullptr,
			&state->renderer->presentSetLayout),
		"Failed To Create Present Descriptor Set Layout"
	);
}
void presentDescriptorSetAllocate(State* state) {
	VkDescriptorSetLayout layout = state->renderer->presentSetLayout;

	VkDescriptorSetAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = state->renderer->globalDescriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	PANIC(
		vkAllocateDescriptorSets(
			state->context->device,
			&allocInfo,
			&state->renderer->presentSet
		),
		"Failed To Allocate Present Descriptor Set"
	);
}
void presentDescriptorSetUpdate(State* state) {
	VkDescriptorImageInfo sceneInfo{
		.sampler = state->texture->sceneColorSampler,
		.imageView = state->texture->sceneColorImageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkDescriptorImageInfo accumInfo{
		.sampler = state->texture->sceneColorSampler, // reuse sampler
		.imageView = state->texture->transAccumImageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	VkDescriptorImageInfo revealInfo{
		.sampler = state->texture->sceneColorSampler, // reuse sampler
		.imageView = state->texture->transRevealImageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	std::array<VkWriteDescriptorSet, 3> writes{};

	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = state->renderer->presentSet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &sceneInfo;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = state->renderer->presentSet;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = &accumInfo;

	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = state->renderer->presentSet;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[2].pImageInfo = &revealInfo;

	vkUpdateDescriptorSets(
		state->context->device,
		static_cast<uint32_t>(writes.size()),
		writes.data(),
		0,
		nullptr
	);
}
void presentSamplerCreate(State* state) {
	VkSamplerCreateInfo samplerInfo{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 1.0f,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
	};

	PANIC(
		vkCreateSampler(
			state->context->device,
			&samplerInfo,
			nullptr,
			&state->texture->sceneColorSampler
		),
		"Failed To Create Present Sampler"
	);
}
void destroySceneColorSampler(State* state) {
	if (state->texture->sceneColorSampler != VK_NULL_HANDLE) {
		vkDestroySampler(
			state->context->device,
			state->texture->sceneColorSampler,
			nullptr
		);
		state->texture->sceneColorSampler = VK_NULL_HANDLE;
	}
}

// For sampling the opaque depth buffer in the transparent pass. We use a simple linear sampler since we'll be doing manual PCF in the shader.
void sceneDepthSamplerCreate(State* state)
{
	VkSamplerCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.mipLodBias = 0.0f,
		.anisotropyEnable = VK_FALSE,
		.maxAnisotropy = 1.0f,
		.compareEnable = VK_FALSE,     // sampling depth as float, not shadow compare
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
		.unnormalizedCoordinates = VK_FALSE
	};

	PANIC(
		vkCreateSampler(
			state->context->device,
			&info,
			nullptr,
			&state->texture->sceneDepthSampler
		),
		"Failed to create scene depth sampler"
	);
}

// Uniform buffers (global UBO at set 0 binding 0)
void uniformBuffersCreate(State* state) {
	// One global UBO per frame in flight
	state->renderer->uniformBuffers.resize(state->config->swapchainBuffering);
	state->renderer->uniformBuffersMemory.resize(state->config->swapchainBuffering);
	state->renderer->uniformBuffersMapped.resize(state->config->swapchainBuffering);

	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	for (size_t i = 0; i < state->config->swapchainBuffering; i++) {
		VkBuffer buffer{};
		VkDeviceMemory bufferMem{};

		createBuffer(
			state,
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			buffer,
			bufferMem
		);

		state->renderer->uniformBuffers[i] = buffer;
		state->renderer->uniformBuffersMemory[i] = bufferMem;

		vkMapMemory(
			state->context->device,
			state->renderer->uniformBuffersMemory[i],
			0,
			bufferSize,
			0,
			&state->renderer->uniformBuffersMapped[i]
		);
	}
}
void uniformBuffersUpdate(State* state) {
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float>(currentTime - startTime).count();

	// Camera matrices
	glm::mat4 view = state->scene->camera->getViewMatrix();

	float aspect = static_cast<float>(state->window.swapchain.imageExtent.width) /
		static_cast<float>(state->window.swapchain.imageExtent.height);

	glm::mat4 proj = state->scene->camera->getProjectionMatrix(aspect, 0.1f, 2000.0f);

	// Global UBO (model is identity; node transforms come from push constants)
	UniformBufferObject ubo{};
	ubo.model = glm::mat4(1.0f);
	ubo.view = view;
	ubo.proj = proj;

	// Lights
	ubo.lightPositions[0] = glm::vec4(5.0f, 5.0f, 0.0f, 0.0f);
	ubo.lightColors[0] = glm::vec4(300.0f, 300.0f, 300.0f, 0.0f);

	ubo.lightPositions[1] = glm::vec4(0.0f, 5.0f, 5.0f, 0.0f);
	ubo.lightColors[1] = glm::vec4(0.0f, 0.0f, 300.0f, 0.0f);

	ubo.lightPositions[2] = glm::vec4(5.0f, 0.0f, 5.0f, 0.0f);
	ubo.lightColors[2] = glm::vec4(300.0f, 0.0f, 0.0f, 0.0f);

	ubo.lightPositions[3] = glm::vec4(0.0f, 5.0f, 0.0f, 0.0f);
	ubo.lightColors[3] = glm::vec4(0.0f, 300.0f, 0.0f, 0.0f);

	// Camera position
	ubo.camPos = glm::vec4(state->scene->camera->getPosition(), 1.0f);

	// PBR params
	ubo.exposure = 1.0f;
	ubo.gamma = 1.0f;
	ubo.prefilteredCubeMipLevels = 1.0f;
	ubo.scaleIBLAmbient = 1.0f;

	// Write into the mapped global UBO buffer for this frame
	void* data = state->renderer->uniformBuffersMapped[state->renderer->frameIndex];
	memcpy(data, &ubo, sizeof(ubo));
}
void uniformBuffersDestroy(State* state) {
	for (size_t i = 0; i < state->config->swapchainBuffering; i++) {
		vkDestroyBuffer(state->context->device, state->renderer->uniformBuffers[i], nullptr);
		vkFreeMemory(state->context->device, state->renderer->uniformBuffersMemory[i], nullptr);
		state->renderer->uniformBuffersMapped[i] = nullptr;
	};
};