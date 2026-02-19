#include "render/frame_buffers.h"
#include "render/renderer.h"
#include "render/render_pass.h"
#include "scene/texture.h"
#include "core/config.h"
#include "core/context.h"
#include "resources/buffers.h"
#include "core/state.h"
void frameBuffersCreate(State* state) {
    uint32_t frameBufferCount = state->window.swapchain.imageCount;
    state->window.framebufferResized = false;

    state->buffers->framebuffers =
        (VkFramebuffer*)malloc(frameBufferCount * sizeof(VkFramebuffer));
    PANIC(!state->buffers->framebuffers,
        "Failed To Allocate Framebuffer Array Memory");

    for (uint32_t i = 0; i < frameBufferCount; i++) {
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = state->renderer->renderPass;
        framebufferInfo.width = state->window.swapchain.imageExtent.width;
        framebufferInfo.height = state->window.swapchain.imageExtent.height;
        framebufferInfo.layers = 1;

        if (state->config->msaaSamples == VK_SAMPLE_COUNT_1_BIT) {
            // No MSAA: color = swapchain, depth = depth
            std::array<VkImageView, 2> attachments = {
                state->window.swapchain.imageViews[i], // color
                state->texture->depthImageView         // depth
            };

            framebufferInfo.attachmentCount =
                static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();

            PANIC(vkCreateFramebuffer(state->context->device,
                &framebufferInfo,
                nullptr,
                &state->buffers->framebuffers[i]),
                "Failed To Create Framebuffer");
        }
        else {
            // MSAA: color = MSAA image, depth = depth, resolve = swapchain
            std::array<VkImageView, 3> attachments = {
                state->texture->colorImageView,        // MSAA color
                state->texture->depthImageView,        // depth
                state->window.swapchain.imageViews[i]  // resolve / present
            };

            framebufferInfo.attachmentCount =
                static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();

            PANIC(vkCreateFramebuffer(state->context->device,
                &framebufferInfo,
                nullptr,
                &state->buffers->framebuffers[i]),
                "Failed To Create Framebuffer");
        }
    }
}

void frameBuffersDestroy(State* state) {
	uint32_t frameBufferCount = state->window.swapchain.imageCount;
	for (int i = 0; i < (int)frameBufferCount; i++) {
		vkDestroyFramebuffer(state->context->device, state->buffers->framebuffers[i], nullptr);

	};
};