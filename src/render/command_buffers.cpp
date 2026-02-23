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

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    vkBeginCommandBuffer(cmd, &beginInfo);

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { state->config->backgroundColor.color };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = state->renderer->renderPass,
        .framebuffer = state->buffers->framebuffers[state->renderer->imageAquiredIndex],
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data()
    };
    renderPassInfo.renderArea = {
            .offset = { 0, 0 },
            .extent = state->window.swapchain.imageExtent
    };

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)state->window.swapchain.imageExtent.width,
        .height = (float)state->window.swapchain.imageExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{
        .offset = { 0, 0 },
        .extent = state->window.swapchain.imageExtent,
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Bind global UBO (set = 0)
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        state->renderer->pipelineLayout,
        0,
        1,
        &state->renderer->descriptorSets[state->renderer->frameIndex],
        0,
        nullptr
    );

    // ─────────────────────────────────────────────
    // GLOBAL DRAW ITEM COLLECTION
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

    // ─────────────────────────────────────────────
    // SPLIT OPAQUE / TRANSPARENT
    // ─────────────────────────────────────────────
    std::vector<DrawItem> opaqueItems;
    std::vector<DrawItem> transparentItems;

    for (const DrawItem& item : allItems)
    {
        if (item.transparent)
            transparentItems.push_back(item);
        else
            opaqueItems.push_back(item);
    }

    // ─────────────────────────────────────────────
    // DRAW OPAQUE FIRST
    // ─────────────────────────────────────────────
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->graphicsPipeline);

    for (const DrawItem& item : opaqueItems)
    {
        drawMesh(
            state,
            cmd,
            item.mesh,
            item.node->getGlobalMatrix(),
            item.model->transform
        );
    }

    // ─────────────────────────────────────────────
    // SORT TRANSPARENT BACK-TO-FRONT
    // ─────────────────────────────────────────────
    std::sort(
        transparentItems.begin(),
        transparentItems.end(),
        [](const DrawItem& a, const DrawItem& b)
        {
            return a.distanceToCamera > b.distanceToCamera;
        }
    );

    // ─────────────────────────────────────────────
    // DRAW TRANSPARENT LAST
    // ─────────────────────────────────────────────
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->transparencyPipeline);

    for (const DrawItem& item : transparentItems)
    {
        drawMesh(
            state,
            cmd,
            item.mesh,
            item.node->getGlobalMatrix(),
            item.model->transform
        );
    }

    vkCmdEndRenderPass(cmd);

    guiDraw(state, cmd);

    PANIC(vkEndCommandBuffer(cmd), "Failed To Record Command Buffer");
}


