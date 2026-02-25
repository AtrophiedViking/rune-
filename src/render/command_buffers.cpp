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
        .width = (float)state->window.swapchain.imageExtent.width,
        .height = (float)state->window.swapchain.imageExtent.height,
        .minDepth = 0.f, .maxDepth = 1.0f
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
    clearValues[0].color = { state->config->backgroundColor.color };
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
            // This should NEVER happen
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
    // DEPTH BLIT: MSAA → SINGLE SAMPLE
    // ─────────────────────────────────────────────

    // MSAA depth → TRANSFER_SRC
    VkImageMemoryBarrier msaaToSrc{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .image = state->texture->msaaDepthImage,
        .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1}
    };

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr,
        1, &msaaToSrc
    );

    // single depth → TRANSFER_DST
    VkImageMemoryBarrier singleToDst{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = state->texture->singleDepthImage,
        .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1}
    };

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr,
        1, &singleToDst
    );

    // Blit depth
    VkImageBlit depthBlit{};
    depthBlit.srcSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1 };
    depthBlit.dstSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1 };
    depthBlit.srcOffsets[1] = {
        (int32_t)state->window.swapchain.imageExtent.width,
        (int32_t)state->window.swapchain.imageExtent.height,
        1
    };
    depthBlit.dstOffsets[1] = depthBlit.srcOffsets[1];

    vkCmdBlitImage(
        cmd,
        state->texture->msaaDepthImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        state->texture->singleDepthImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &depthBlit,
        VK_FILTER_NEAREST
    );

    // single depth → ATTACHMENT
    VkImageMemoryBarrier depthToAttachment{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .image = state->texture->singleDepthImage,
        .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1}
    };

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        0, 0, nullptr, 0, nullptr,
        1, &depthToAttachment
    );

    // ─────────────────────────────────────────────
    // PASS 2: TRANSPARENT (LOAD COLOR + LOAD DEPTH)
    // ─────────────────────────────────────────────

    VkRenderPassBeginInfo transInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = state->renderer->transparencyRenderPass,
        .framebuffer = state->buffers->transparencyFramebuffers[state->renderer->imageAquiredIndex],
        .renderArea = {{0,0}, state->window.swapchain.imageExtent},
        .clearValueCount = 0,
        .pClearValues = nullptr,
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

    // Sort back-to-front
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
    std::array<VkClearValue, 2> presentClearValues{};
    presentClearValues[0].color = { state->config->backgroundColor.color };
    presentClearValues[1].depthStencil = { 1.0f, 0 };

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
