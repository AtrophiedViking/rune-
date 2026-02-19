#include "render/sync_objects.h"
#include "render/renderer.h"
#include "core/context.h"
#include "core/config.h"
#include "core/state.h"


void syncObjectsCreate(State* state) {
	VkSemaphoreCreateInfo semaphoreInfo{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};
	VkFenceCreateInfo fenceInfo{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};
	state->renderer->imageAvailableSemaphore = (VkSemaphore*)malloc(state->config->swapchainBuffering * sizeof(VkSemaphore));
	state->renderer->renderFinishedSemaphore = (VkSemaphore*)malloc(state->config->swapchainBuffering * sizeof(VkSemaphore));
	state->renderer->inFlightFence = (VkFence*)malloc(state->config->swapchainBuffering * sizeof(VkFence));
	PANIC(!state->renderer->imageAvailableSemaphore || !state->renderer->renderFinishedSemaphore || !state->renderer->inFlightFence, "Failed To Allocate Semaphore Memory");
	for (int i = 0; i < (int)state->config->swapchainBuffering; i++) {
		PANIC((vkCreateSemaphore(state->context->device, &semaphoreInfo, nullptr, &state->renderer->imageAvailableSemaphore[i])
			|| vkCreateSemaphore(state->context->device, &semaphoreInfo, nullptr, &state->renderer->renderFinishedSemaphore[i]))
			|| vkCreateFence(state->context->device, &fenceInfo, nullptr, &state->renderer->inFlightFence[i]),
			"Failed To Create Image Aquired Semaphore");
	};
};
void syncObjectsDestroy(State* state) {
	for (int i = 0; i < (int)state->window.swapchain.imageCount; i++) {
		vkDestroySemaphore(state->context->device, state->renderer->imageAvailableSemaphore[i], nullptr);
		vkDestroySemaphore(state->context->device, state->renderer->renderFinishedSemaphore[i], nullptr);
		vkDestroyFence(state->context->device, state->renderer->inFlightFence[i], nullptr);
	};
};

