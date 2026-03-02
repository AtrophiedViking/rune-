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
    // FRAME + IMAGE INDICES
    uint32_t frameIndex = state->renderer->frameIndex;
    uint32_t imageIndex = state->renderer->imageAquiredIndex;

    VkCommandBuffer cmd = state->buffers->commandBuffer[frameIndex];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkViewport viewport{
        .x = 0.f, .y = 0.f,
        .width = (float)state->window.swapchain.imageExtent.width,
        .height = (float)state->window.swapchain.imageExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = state->window.swapchain.imageExtent
    };

    // BUILD DRAW LISTS
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

    // PASS 1: OPAQUE
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

    // 🔹 SKYBOX FIRST
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->skyboxPipeline);

    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        state->renderer->skyboxPipelineLayout,
        0, 1,
        &state->renderer->globalSets[frameIndex],
        0, nullptr
    );

    VkDeviceSize skyboxOffset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &state->renderer->skyboxVbo, &skyboxOffset);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    // 🔹 NOW OPAQUE PBR
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, state->renderer->opaquePipeline);

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        state->renderer->opaquePipelineLayout,
        0, 1,
        &state->renderer->globalSets[frameIndex],
        0, nullptr
    );

    for (auto& item : opaqueItems)
    {
        drawMesh(
            state, cmd,
            item.mesh,
            item.node->getGlobalMatrix(),
            item.model->transform,
            state->renderer->opaquePipelineLayout
        );
    }

    vkCmdEndRenderPass(cmd);

    // TRANSITION sceneColor FOR SAMPLING
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = state->texture->sceneColorImage;

        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }

    // DEPTH "RESOLVE" VIA BLIT (MSAA → single-sample)
    {
        // 1) msaaDepth: DEPTH_STENCIL_ATTACHMENT_OPTIMAL → TRANSFER_SRC_OPTIMAL
        VkImageMemoryBarrier depthSrcBarrier{};
        depthSrcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depthSrcBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthSrcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        depthSrcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthSrcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthSrcBarrier.image = state->texture->msaaDepthImage;
        depthSrcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthSrcBarrier.subresourceRange.baseMipLevel = 0;
        depthSrcBarrier.subresourceRange.levelCount = 1;
        depthSrcBarrier.subresourceRange.baseArrayLayer = 0;
        depthSrcBarrier.subresourceRange.layerCount = 1;
        depthSrcBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthSrcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &depthSrcBarrier
        );

        // 2) sceneDepth is already in TRANSFER_DST_OPTIMAL from creation

        // 3) Blit depth
        VkImageBlit depthBlit{};
        depthBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthBlit.srcSubresource.mipLevel = 0;
        depthBlit.srcSubresource.baseArrayLayer = 0;
        depthBlit.srcSubresource.layerCount = 1;
        depthBlit.srcOffsets[0] = { 0, 0, 0 };
        depthBlit.srcOffsets[1] = {
            (int32_t)state->window.swapchain.imageExtent.width,
            (int32_t)state->window.swapchain.imageExtent.height,
            1
        };

        depthBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthBlit.dstSubresource.mipLevel = 0;
        depthBlit.dstSubresource.baseArrayLayer = 0;
        depthBlit.dstSubresource.layerCount = 1;
        depthBlit.dstOffsets[0] = { 0, 0, 0 };
        depthBlit.dstOffsets[1] = {
            (int32_t)state->window.swapchain.imageExtent.width,
            (int32_t)state->window.swapchain.imageExtent.height,
            1
        };

        vkCmdBlitImage(
            cmd,
            state->texture->msaaDepthImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            state->texture->sceneDepthImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &depthBlit,
            VK_FILTER_NEAREST
        );

        // 4) sceneDepth: TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL
        VkImageMemoryBarrier depthDstBarrier{};
        depthDstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depthDstBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        depthDstBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        depthDstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthDstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthDstBarrier.image = state->texture->sceneDepthImage;
        depthDstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthDstBarrier.subresourceRange.baseMipLevel = 0;
        depthDstBarrier.subresourceRange.levelCount = 1;
        depthDstBarrier.subresourceRange.baseArrayLayer = 0;
        depthDstBarrier.subresourceRange.layerCount = 1;
        depthDstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        depthDstBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &depthDstBarrier
        );

        // 5) (optional but clean) msaaDepth: TRANSFER_SRC_OPTIMAL → DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        VkImageMemoryBarrier depthSrcBack{};
        depthSrcBack.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depthSrcBack.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        depthSrcBack.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthSrcBack.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthSrcBack.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthSrcBack.image = state->texture->msaaDepthImage;
        depthSrcBack.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthSrcBack.subresourceRange.baseMipLevel = 0;
        depthSrcBack.subresourceRange.levelCount = 1;
        depthSrcBack.subresourceRange.baseArrayLayer = 0;
        depthSrcBack.subresourceRange.layerCount = 1;
        depthSrcBack.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        depthSrcBack.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &depthSrcBack
        );
    }

    // PASS 2: TRANSPARENT
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

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        state->renderer->transparencyPipelineLayout,
        0, 1,
        &state->renderer->globalSets[frameIndex],
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

    // PASS 3: PRESENT
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

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        state->renderer->presentPipelineLayout,
        0, 1,
        &state->renderer->presentSet,
        0, nullptr
    );

    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);

    // GUI
    guiDraw(state, cmd);

    vkEndCommandBuffer(cmd);
}