#include "render/command_buffers.h"
#include "resources/buffers.h"
#include "render/renderer.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/model.h"
#include "scene/gather.h"
#include "gui/gui.h"
#include "core/context.h"
#include "core/config.h"
#include "core/state.h"

void commandPoolCreate(State* state) {
	VkCommandPoolCreateInfo poolInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = state->context->queueFamilyIndex,
	};
	PANIC(vkCreateCommandPool(state->context->device, &poolInfo, nullptr, &state->renderer->commandPool), "Failed To Create Command Pool");
};

void commandPoolDestroy(State* state) {
	vkDestroyCommandPool(state->context->device, state->renderer->commandPool, nullptr);
};

VkCommandBuffer beginSingleTimeCommands(State* state, VkCommandPool commandPool) {
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool,
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(state->context->device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}
void endSingleTimeCommands(State* state, VkCommandBuffer commandBuffer) {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(state->context->queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(state->context->queue);

	vkFreeCommandBuffers(state->context->device, state->renderer->commandPool, 1, &commandBuffer);
}

void commandBufferGet(State* state) {
	VkCommandBufferAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = state->renderer->commandPool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = state->config->swapchainBuffering,
	};
	state->buffers->commandBuffer = (VkCommandBuffer*)malloc(state->config->swapchainBuffering * sizeof(VkCommandBuffer));
	PANIC(vkAllocateCommandBuffers(state->context->device, &allocInfo, state->buffers->commandBuffer), "Failed To Create Command Buffer");
};

void commandBufferRecord(State* state)
{
    VkCommandBuffer cmd = state->buffers->commandBuffer[state->renderer->frameIndex];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(cmd, &beginInfo);

    VkViewport viewport{
        .x = 0.f, .y = 0.f,
        .width  = (float)state->window.swapchain.imageExtent.width,
        .height = (float)state->window.swapchain.imageExtent.height,
        .minDepth = state->scene->camera->nearClip, .maxDepth = state->scene->camera->viewDistance
    };

    VkRect2D scissor{
        .offset = {0,0},
        .extent = state->window.swapchain.imageExtent
    };

    // ─────────────────────────────────────────────
    // BUILD DRAW LISTS
    // ─────────────────────────────────────────────
    std::vector<DrawItem> allItems;
    for (Model* model : state->scene->models)
    {
        gatherDrawItems(
            model->rootNode,
            state->scene->camera->getPosition(),
            state->scene->materials,
            model,
            allItems
        );
    }

    std::vector<DrawItem> opaqueItems;
    std::vector<DrawItem> transparentItems;

    for (auto& item : allItems)
    {
        if (item.transparent)
            transparentItems.push_back(item);
        else
            opaqueItems.push_back(item);
    }

    // ─────────────────────────────────────────────
    // PASS 1: OPAQUE (MSAA COLOR + MSAA DEPTH)
    // ─────────────────────────────────────────────
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = { state->config->backgroundColor.color };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo opaqueInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = state->renderer->opaqueRenderPass,
        .framebuffer = state->buffers->opaqueFramebuffers[state->renderer->imageAquiredIndex],
        .renderArea = {{0,0}, state->window.swapchain.imageExtent},
        .clearValueCount = 2,
        .pClearValues = clearValues.data(),
    };

    vkCmdBeginRenderPass(cmd, &opaqueInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Global UBO
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        state->renderer->opaquePipelineLayout,
        0, 1,
        &state->renderer->descriptorSets[state->renderer->frameIndex],
        0, nullptr
    );

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->opaquePipeline);

    for (auto& item : opaqueItems)
    {
        if (item.transparent) {
            printf("ERROR: transparent item in opaque pass (mesh %p)\n", (void*)item.mesh);
        }

        drawMesh(
            state, cmd,
            item.mesh,
            item.node->getGlobalMatrix(),
            item.model->transform,
            state->renderer->opaquePipelineLayout
        );
    }

    vkCmdEndRenderPass(cmd);

   

    // ─────────────────────────────────────────────
    // PASS 2: TRANSPARENT (CLEAR ACCUM/REVEAL + LOAD DEPTH)
    // ─────────────────────────────────────────────

    std::array<VkClearValue, 3> transClears{};
    transClears[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } }; // accum = 0
    transClears[1].color = { { 1.0f, 0.0f, 0.0f, 0.0f } }; // reveal = 1
    transClears[2].depthStencil = { 1.0f, 0 };             // depth (LOAD, but clear value unused)

    VkRenderPassBeginInfo transInfo{
     .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
     .renderPass = state->renderer->transparencyRenderPass,
     .framebuffer = state->buffers->transparencyFramebuffers[state->renderer->imageAquiredIndex],
     .renderArea = {{0,0}, state->window.swapchain.imageExtent},
     .clearValueCount = 3,              // only color attachments are cleared
     .pClearValues = transClears.data(),
    };

    vkCmdBeginRenderPass(cmd, &transInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        state->renderer->transparencyPipelineLayout,
        0, 1,
        &state->renderer->descriptorSets[state->renderer->frameIndex],
        0, nullptr
    );

    std::sort(
        transparentItems.begin(),
        transparentItems.end(),
        [](auto& a, auto& b) { return a.distanceToCamera > b.distanceToCamera; }
    );

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->transparencyPipeline);

    for (auto& item : transparentItems)
    {
        drawMesh(
            state, cmd,
            item.mesh,
            item.node->getGlobalMatrix(),
            item.model->transform,
            state->renderer->transparencyPipelineLayout
        );
    }

    vkCmdEndRenderPass(cmd);

    // ─────────────────────────────────────────────
    // PASS 3: PRESENT
    // ─────────────────────────────────────────────
    std::array<VkClearValue, 1> presentClearValues{};
    presentClearValues[0].color = { state->config->backgroundColor.color };

    VkRenderPassBeginInfo presentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = state->renderer->presentRenderPass,
        .framebuffer = state->buffers->presentFramebuffers[state->renderer->imageAquiredIndex],
        .renderArea = {{0,0}, state->window.swapchain.imageExtent},
        .clearValueCount = 1,
        .pClearValues = presentClearValues.data(),
    };

    vkCmdBeginRenderPass(cmd, &presentInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->presentPipeline);

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        state->renderer->presentPipelineLayout,
        0, 1,
        &state->renderer->presentSet,
        0, nullptr
    );

    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);

    guiDraw(state, cmd);
    vkEndCommandBuffer(cmd);
}
