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

// temp debug
void debugReadbackSpecularFace0(State* state, uint32_t texWidth)
{
    VkDevice device = state->context->device;
    VkPhysicalDevice phys = state->context->physicalDevice;
    VkCommandPool pool = state->renderer->commandPool;
    VkQueue queue = state->context->queue;

    VkImage srcImage = state->texture->computeImage;
    uint32_t size = 4 * sizeof(float); // assuming rgba16f or rgba32f? adjust if needed

    // create a small 2D image matching mip0 size
    uint32_t mip0Size = texWidth;
    VkDeviceSize byteSize = mip0Size * mip0Size * 4 * sizeof(float); // overkill but fine

    // create a linear-tiled staging image or buffer (buffer is simpler)
    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = byteSize;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buf;
    vkCreateBuffer(device, &bufInfo, nullptr, &buf);

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(device, buf, &memReq);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;
    alloc.memoryTypeIndex = findMemoryType(state, memReq, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory mem;
    vkAllocateMemory(device, &alloc, nullptr, &mem);
    vkBindBufferMemory(device, buf, mem, 0);

    // command buffer
    VkCommandBufferAllocateInfo cbAlloc{};
    cbAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAlloc.commandPool = pool;
    cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cbAlloc, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    // transition srcImage to TRANSFER_SRC_OPTIMAL (mip 0, layer 0)
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = srcImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    // copy image → buffer
    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = 0;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset = { 0, 0, 0 };
    copy.imageExtent = { mip0Size, mip0Size, 1 };

    vkCmdCopyImageToBuffer(
        cmd,
        srcImage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        buf,
        1,
        &copy
    );

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    // map and inspect first few pixels
    void* data;
    vkMapMemory(device, mem, 0, VK_WHOLE_SIZE, 0, &data);
    float* f = (float*)data;
    printf("specular face0 mip0 first pixel = %f %f %f %f\n", f[0], f[1], f[2], f[3]);
    vkUnmapMemory(device, mem);

    vkFreeCommandBuffers(device, pool, 1, &cmd);
    vkDestroyBuffer(device, buf, nullptr);
    vkFreeMemory(device, mem, nullptr);
}


// Forward declarations
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
    // After you know texWidth/texHeight and envMipLevels
    state->texture->specularFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    state->texture->specularMipLevels = state->texture->envMipLevels;
    specularCubeImageCreate(state, texWidth);
	specularCubeViewCreate(state);
	specularSamplerCreate(state);

    printf("computeImage      = %p\n", (void*)state->texture->computeImage);
    printf("computeImageView  = %p\n", (void*)state->texture->computeImageView);
    printf("computeSampler    = %p\n", (void*)state->texture->computeSampler);

    // 4) Per-mip compute views (must exist before iblSetCreate)
    computeMipViewsCreate(state);


    // 5) BRDF LUT (runtime generated)
    brdfLutImageCreate(state);
    brdfLutSetLayoutCreate(state);
    brdfLutDescriptorCreate(state);
    brdfLutPipelineCreate(state);
    brdfLutGenerate(state);

    // 6) IBL descriptor pool + sets + pipeline + dispatch
    iblDescriptorPoolCreate(state);
    iblSetCreate(state);
    iblPipelineCreate(state);
    iblPrefilterDispatch(state, texWidth);

    vkQueueWaitIdle(state->context->queue);
    //debugReadbackSpecularFace0(state, texWidth);

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

    //debugReadbackSpecularFace0(state, texWidth);
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