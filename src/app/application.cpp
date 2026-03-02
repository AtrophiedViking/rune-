#include "app/application.h"
#include "loader/gltf_textures.h"
#include "loader/gltf_meshes.h"
#include "loader/gltf_loader.h"
#include "loader/ktx_cubemap.h"
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
void init(State* state) {
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
    commandPoolCreate(state);

    swapchainCreate(state);
    swapchainImageGet(state);
    imageViewsCreate(state);
    opaqueRenderPassCreate(state);
    transparentRenderPassCreate(state);
    presentRenderPassCreate(state);

    guiRenderPassCreate(state);
    guiFramebuffersCreate(state);
    guiDescriptorPoolCreate(state);
    guiInit(state);

    globalSetLayoutCreate(state);
    materialSetLayoutCreate(state);
    presentSetLayoutCreate(state);

    createSkyboxVbo(state);

    skyboxPipelineCreate(state);
    opaquePipelineCreate(state);
    transparencyPipelineCreate(state);
    presentPipelineCreate(state);

    printf("init: msaaSamples = %d\n", state->config->msaaSamples);

    colorResourceCreate(state);
    depthResourceCreate(state);
    sceneDepthSamplerCreate(state);
    sceneColorResourceCreate(state);
    presentSamplerCreate(state);

    iblSetLayoutCreate(state);

    uint32_t texWidth, texHeight;

    // 1) Env cubemap
    textureCubeImageCreate(
        state,
        state->config->DEFAULT_CUBEMAP,
        state->texture->cubeImage,
        state->texture->cubeImageMemory,
        state->texture->envFormat,
        state->texture->envMipLevels,
        &texWidth,
        &texHeight
    );

    createCubeImageView(
        state,
        state->texture->cubeImage,
        state->texture->envFormat,
        state->texture->envMipLevels,
        state->texture->cubeImageView
    );

    createCubeSampler(
        state,
        state->texture->cubeSampler,
        state->texture->envMipLevels
    );

    // 2) Irradiance
    textureCubeImageCreate(
        state,
        state->config->DEFAULT_IRRADIANCE,
        state->texture->irradianceImage,
        state->texture->irradianceImageMemory,
        state->texture->irradianceFormat,
        state->texture->irradianceMipLevels
    );

    createCubeImageView(
        state,
        state->texture->irradianceImage,
        state->texture->irradianceFormat,
        state->texture->irradianceMipLevels,
        state->texture->irradianceImageView
    );

    createCubeSampler(
        state,
        state->texture->irradianceSampler,
        state->texture->irradianceMipLevels
    );

    // 3) Specular target (compute)
    computeCubeImageCreate(
        state,
        texWidth,
        texHeight,
        state->texture->envFormat,
        state->texture->envMipLevels,
        state->texture->computeImage,
        state->texture->computeImageMemory
    );

    state->texture->specularFormat = state->texture->envFormat;
    state->texture->specularMipLevels = state->texture->envMipLevels;

    createCubeImageView(
        state,
        state->texture->computeImage,
        state->texture->specularFormat,
        state->texture->specularMipLevels,
        state->texture->computeImageView
    );

    createCubeSampler(
        state,
        state->texture->computeSampler,
        state->texture->specularMipLevels
    );

    // 4) BRDF LUT
    textureImageCreate(
        state,
        state->config->DEFAULT_BRDF_LUT,
        state->texture->lutImage,
        state->texture->lutImageMemory,
        state->texture->lutFormat,
        state->texture->lutMipLevels
    );

    textureImageViewCreate(
        state,
        state->texture->lutImage,
        state->texture->lutFormat,
        VK_IMAGE_ASPECT_COLOR_BIT,
        state->texture->lutMipLevels,
        state->texture->lutImageView
    );

    textureSamplerCreate(
        state,
        state->texture->lutSampler,
        state->texture->lutMipLevels
    );

    // 5) Per-mip compute views (must exist before iblSetCreate)
    createComputeMipViews(state);

    // 6) IBL descriptor pool + sets + pipeline + dispatch
    iblDescriptorPoolCreate(state);
    iblSetCreate(state);
    iblPipelineCreate(state);
    iblPrefilterDispatch(state, texWidth);

    printf("Opaque depth image:      %p\n", (void*)state->texture->msaaDepthImage);
    printf("Opaque depth image view: %p\n", (void*)state->texture->msaaDepthImageView);
    opaqueFrameBuffersCreate(state);
    printf("Transparent depth image:      %p\n", (void*)state->texture->singleDepthImage);
    printf("Transparent depth image view: %p\n", (void*)state->texture->singleDepthImageView);
    transparentFrameBuffersCreate(state);
    presentFramebuffersCreate(state);
    callbackSetup(state);

    // Load model + textures BEFORE descriptor sets
    loadModel(state, state->config->MODEL_PATH);
    state->scene->models[0]->setTransform(
        { 1.0f, -1.0f, 0.0f },
        { 0.0f, -90.0f, 0.0f },
        { 1.0f, 1.0f, 1.0f }
    );

    uniformBuffersCreate(state);

    globalDescriptorPoolCreate(state);
    materialDescriptorPoolCreate(state);

    globalSetsCreate(state);
    materialSetsCreate(state);

    presentDescriptorSetAllocate(state);
    presentDescriptorSetUpdate(state);

    commandBufferGet(state);
    commandBufferRecord(state);

    syncObjectsCreate(state);
}

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
	vkDestroyShaderModule(state->context->device, state->renderer->vertShaderModule, nullptr);

	swapchainCleanup(state);

	guiClean(state);
	modelUnload(state);
	destroyTextures(state);

	uniformBuffersDestroy(state);
	globalDescriptorPoolDestroy(state);
	globalSetLayoutDestroy(state);
	indexBufferDestroy(state);
	vertexBufferDestroy(state);
	syncObjectsDestroy(state);
	commandPoolDestroy(state);
	presentPipelineDestroy(state);
	destroySceneColorSampler(state);
	transparencyPipelineDestroy(state);
	opaquePipelineDestroy(state);
	presentRenderPassDestroy(state);
	transparentRenderPassDestroy(state);
	opaqueRenderPassDestroy(state);
	deviceDestroy(state);
	windowDestroy(state);
};