#include "app/application.h"
#include "loader/gltf_textures.h"
#include "loader/gltf_meshes.h"
#include "loader/gltf_loader.h"
#include "scene/model.h"
#include "scene/scene.h"
#include "scene/camera.h"
#include "resources/buffers.h"
#include "render/command_buffers.h"
#include "render/frame_buffers.h"
#include "render/descriptors.h"
#include "render/pipelines.h"
#include "render/sync_objects.h"
#include "render/renderer.h"
#include "render/render_pass.h"
#include "core/swapchain.h"
#include "core/context.h"
#include "core/window.h"
#include "core/config.h"
#include "core/input.h"
#include "gui/gui.h"
#include "core/state.h"	
void init(State *state) {
	state->texture = new Texture{};
	state->context = new Context{};
	state->renderer = new Renderer{};
	state->buffers = new Buffers{};
	state->gui = new Gui{};
	state->scene = new Scene{};
	state->scene->camera = new Camera{};
	state->scene->camera->updateCameraVectors();
	windowCreate(state);
	deviceCreate(state);

	swapchainCreate(state);
	swapchainImageGet(state);
	imageViewsCreate(state);
	renderPassCreate(state);

	guiRenderPassCreate(state);
	guiFramebuffersCreate(state);
	guiDescriptorPoolCreate(state);   // <-- MUST be here

	guiInit(state);
	// MUST come BEFORE pipeline creation
	createGlobalSetLayout(state);
	createTextureSetLayout(state);

	graphicsPipelineCreate(state);
	tranparencyPipelineCreate(state);
	commandPoolCreate(state);

	printf("init: msaaSamples = %d\n", state->config->msaaSamples);

	colorResourceCreate(state);
	depthResourceCreate(state);
	frameBuffersCreate(state);

	callbackSetup(state);

	// Load model + textures BEFORE descriptor sets


	loadModel(state, state->config->KOBOLD_MODEL_PATH);
	state->scene->models[0]->setTransform(
		{ 0.0f, 0.0f, 0.0f },   // position
		{ 0.0f, 0.0f, 0.0f },    // rotation
		{ 1.0f, 1.0f, 1.0f }     // scale
	);
	loadModel(state, state->config->MODEL_PATH);
	state->scene->models[1]->setTransform(
		{ 0.5f, 0.5f, 0.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 5.0f, 5.0f, 5.0f }
	);

	loadModel(state, state->config->HOVER_BIKE_MODEL_PATH);
	state->scene->models[2]->setTransform(
		{ 0.0f, -1.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 1.0f, 1.0f, 1.0f }
	);


	uniformBuffersCreate(state);

	descriptorPoolCreate(state);

	descriptorSetsCreate(state);        // global UBO set (set = 0)
	createMaterialDescriptorSets(state); // texture sets (set = 1)

	commandBufferGet(state);
	commandBufferRecord(state);

	syncObjectsCreate(state);
};

void mainloop(State *state) {
	while (!glfwWindowShouldClose(state->window.handle)) {
		glfwPollEvents();
		updateFPS(state);
		processInput(state);

		uniformBuffersUpdate(state);

		frameDraw(state);

	};
		vkDeviceWaitIdle(state->context->device);
};

void cleanup(State *state) {
	vkDestroyShaderModule(state->context->device, state->renderer->fragShaderModule, nullptr);
	vkDestroyShaderModule(state->context->device, state->renderer->vertShaderModule, nullptr);
	swapchainCleanup(state);

	guiClean(state);
	modelUnload(state);
	destroyTextures(state);

	uniformBuffersDestroy(state);
	descriptorPoolDestroy(state);
	descriptorSetLayoutDestroy(state);
	indexBufferDestroy(state);
	vertexBufferDestroy(state);
	syncObjectsDestroy(state);
	commandPoolDestroy(state);
	tranparencyPipelineDestroy(state);
	graphicsPipelineDestroy(state);
	renderPassDestroy(state);
	deviceDestroy(state);
	windowDestroy(state);
};