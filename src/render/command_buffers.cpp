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
    // 1. FRAME + IMAGE INDICES
    uint32_t frameIndex = state->renderer->frameIndex;
    uint32_t imageIndex = state->renderer->imageAquiredIndex;

    VkCommandBuffer cmd = state->buffers->commandBuffer[frameIndex];

    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkViewport viewport{
        .x = 0.f, .y = 0.f,
        .width = (float)state->window.swapchain.imageExtent.width,
        .height = (float)state->window.swapchain.imageExtent.height,
        .minDepth = 0.0f, .maxDepth = 1.0f
    };

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = state->window.swapchain.imageExtent
    };

    // 2. BUILD DRAW LISTS
    std::vector<DrawItem> allItems;
    for (Model* model : state->scene->models) {
        gatherDrawItems(model->rootNode, state->scene->camera->getPosition(), state->scene->materials, model, allItems);
    }

    std::vector<DrawItem> opaqueItems;
    std::vector<DrawItem> transparentItems;
    for (auto& item : allItems) {
        if (item.transparent) transparentItems.push_back(item);
        else opaqueItems.push_back(item);
    }

    // 3. PASS 1: OPAQUE
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { state->config->backgroundColor.color };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo opaqueInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = state->renderer->opaqueRenderPass,
        .framebuffer = state->buffers->opaqueFramebuffers[imageIndex],
        .renderArea = {{0,0}, state->window.swapchain.imageExtent},
        .clearValueCount = 2,
        .pClearValues = clearValues.data(),
    };

    vkCmdBeginRenderPass(cmd, &opaqueInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // Skybox
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->skyboxPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->skyboxPipelineLayout, 0, 1, &state->renderer->globalSets[frameIndex], 0, nullptr);
    VkDeviceSize skyboxOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &state->renderer->skyboxVbo, &skyboxOffset);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    // Opaque PBR
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->opaquePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->opaquePipelineLayout, 0, 1, &state->renderer->globalSets[frameIndex], 0, nullptr);

    for (auto& item : opaqueItems) {
        drawMesh(state, cmd, item.mesh, item.node->getGlobalMatrix(), item.model->transform, state->renderer->opaquePipelineLayout);
    }
    vkCmdEndRenderPass(cmd);

    // 4. TRANSITION sceneColor FOR SAMPLING
    {
        VkImageMemoryBarrier barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.image = state->texture->sceneColorImage;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // 5. DEPTH RESOLVE/COPY
    {
        // ============================================================
        // PART A: Prepare for Copy (Transfer)
        // ============================================================
        VkImageMemoryBarrier srcBarrierPrep{};
        srcBarrierPrep.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        srcBarrierPrep.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        srcBarrierPrep.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        srcBarrierPrep.image = state->texture->msaaDepthImage;
        srcBarrierPrep.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
        srcBarrierPrep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcBarrierPrep.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        VkImageMemoryBarrier dstBarrierPrep{};
        dstBarrierPrep.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        dstBarrierPrep.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Discard old frame's data
        dstBarrierPrep.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        dstBarrierPrep.image = state->texture->sceneDepthImage;
        dstBarrierPrep.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
        dstBarrierPrep.srcAccessMask = 0;
        dstBarrierPrep.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        // Fresh array for the PREP barriers
        std::array<VkImageMemoryBarrier, 2> prepBarriers = { srcBarrierPrep, dstBarrierPrep };

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, // Wait for opaque pass to finish writing depth
            VK_PIPELINE_STAGE_TRANSFER_BIT,            // Before transfer starts
            0, 0, nullptr, 0, nullptr,
            static_cast<uint32_t>(prepBarriers.size()), prepBarriers.data()
        );

        // ============================================================
        // PART B: Blit/Copy Depth
        // ============================================================
        VkImageBlit depthBlit{};
        depthBlit.srcSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1 };
        depthBlit.srcOffsets[1] = { (int32_t)state->window.swapchain.imageExtent.width, (int32_t)state->window.swapchain.imageExtent.height, 1 };
        depthBlit.dstSubresource = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1 };
        depthBlit.dstOffsets[1] = depthBlit.srcOffsets[1];

        vkCmdBlitImage(
            cmd,
            state->texture->msaaDepthImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            state->texture->sceneDepthImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &depthBlit, VK_FILTER_NEAREST
        );

        // ============================================================
        // PART C: Restore and Prepare for Shader Read
        // ============================================================
        VkImageMemoryBarrier srcBarrierRestore{};
        srcBarrierRestore.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        srcBarrierRestore.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        srcBarrierRestore.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // Restore to attachment layout
        srcBarrierRestore.image = state->texture->msaaDepthImage;
        srcBarrierRestore.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
        srcBarrierRestore.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcBarrierRestore.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkImageMemoryBarrier dstBarrierRead{};
        dstBarrierRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        dstBarrierRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        dstBarrierRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Prepare for transparency sampling
        dstBarrierRead.image = state->texture->sceneDepthImage;
        dstBarrierRead.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
        dstBarrierRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstBarrierRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        // Fresh array for the POST barriers
        std::array<VkImageMemoryBarrier, 2> postBarriers = { srcBarrierRestore, dstBarrierRead };

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, // Wait for the blit/transfer to finish
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // Before transparent pass reads/writes
            0, 0, nullptr, 0, nullptr,
            static_cast<uint32_t>(postBarriers.size()), postBarriers.data()
        );
    }

    // 6. PASS 2: TRANSPARENT
    std::array<VkClearValue, 3> transClears{};
    transClears[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
    transClears[1].color = { { 1.0f, 0.0f, 0.0f, 0.0f } };
    transClears[2].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo transInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = state->renderer->transparencyRenderPass,
        .framebuffer = state->buffers->transparencyFramebuffers[imageIndex],
        .renderArea = {{0,0}, state->window.swapchain.imageExtent},
        .clearValueCount = 3,
        .pClearValues = transClears.data(),
    };

    vkCmdBeginRenderPass(cmd, &transInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->transparencyPipelineLayout, 0, 1, &state->renderer->globalSets[frameIndex], 0, nullptr);

    std::sort(transparentItems.begin(), transparentItems.end(), [](auto& a, auto& b) { return a.distanceToCamera > b.distanceToCamera; });

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->transparencyPipeline);
    for (auto& item : transparentItems) {
        drawMesh(state, cmd, item.mesh, item.node->getGlobalMatrix(), item.model->transform, state->renderer->transparencyPipelineLayout);
    }
    vkCmdEndRenderPass(cmd);

    // 7. PASS 3: PRESENT (Tonemapping/UI)
    std::array<VkClearValue, 1> presentClearValues{};
    presentClearValues[0].color = { state->config->backgroundColor.color };

    VkRenderPassBeginInfo presentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = state->renderer->presentRenderPass,
        .framebuffer = state->buffers->presentFramebuffers[imageIndex],
        .renderArea = {{0,0}, state->window.swapchain.imageExtent},
        .clearValueCount = 1,
        .pClearValues = presentClearValues.data(),
    };

    vkCmdBeginRenderPass(cmd, &presentInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->presentPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->presentPipelineLayout, 0, 1, &state->renderer->presentSet, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);

    // 8. GUI
    guiDraw(state, cmd);

    vkEndCommandBuffer(cmd);
}