#include "core/window.h"
#include "core/context.h"
#include "core/swapchain.h"
#include "loader/gltf_loader.h"
#include "render/renderer.h"
#include "render/render_pass.h"
#include "render/command_buffers.h"
#include "resources/buffers.h"
#include "core/config.h"
#include "render/pipelines.h"
#include "render/descriptors.h"
#include "render/sync_objects.h"
#include "core/input.h"
#include "gui/gui.h"
#include "core/state.h"

//Error Handlin
void logPrint() {
	uint32_t instanceApiVersion;
	PANIC(vkEnumerateInstanceVersion(&instanceApiVersion), "Failed To Enumerate Instance Version");
	uint32_t apiVersionVarient = VK_API_VERSION_VARIANT(instanceApiVersion);
	uint32_t apiVersionMajor = VK_API_VERSION_MAJOR(instanceApiVersion);
	uint32_t apiVersionMinor = VK_API_VERSION_MINOR(instanceApiVersion);
	uint32_t apiVersionPatch = VK_API_VERSION_PATCH(instanceApiVersion);
	printf("Vulkan API %i.%i.%i.%i\n", apiVersionVarient, apiVersionMajor, apiVersionMinor, apiVersionPatch);
	printf("GLFW %s\n\n", glfwGetVersionString());
};
void exitCallback() {
	glfwTerminate();
};
void glfwErrorCallback(int errorCode, const char* description) {
	PANIC(errorCode, "GLFW %s", description);
};
void errorHandlingSetup() {
	glfwSetErrorCallback(glfwErrorCallback);
	atexit(exitCallback);
};

//utility
static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
	auto app = (State*)glfwGetWindowUserPointer(window);
	app->window.framebufferResized = true;
};
void callbackSetup(State* state) {
	// Store ImGui callbacks so we can chain them
	auto imguiMouseCallback = ImGui_ImplGlfw_MouseButtonCallback;
	auto imguiCursorCallback = ImGui_ImplGlfw_CursorPosCallback;
	auto imguiScrollCallback = ImGui_ImplGlfw_ScrollCallback;
	auto imguiKeyCallback = ImGui_ImplGlfw_KeyCallback;
	auto imguiCharCallback = ImGui_ImplGlfw_CharCallback;

	glfwSetWindowUserPointer(state->window.handle, state);
	glfwSetCursorPosCallback(state->window.handle, mouseCallback);
	glfwSetMouseButtonCallback(state->window.handle, mouseButtonCallback);
	glfwSetCharCallback(state->window.handle, charCallback);
	glfwSetKeyCallback(state->window.handle, keyCallback);

};

//Window
void initGLFW(bool windowResizable) {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, windowResizable);
};

void surfaceCreate(State* state) {
	PANIC(glfwCreateWindowSurface(state->context->instance, state->window.handle, nullptr, &state->window.surface), "Failed To Create Surface");
};
void surfaceDestroy(State* state) {
	vkDestroySurfaceKHR(state->context->instance, state->window.surface, nullptr);
};

void windowCreate(State* state) {
	initGLFW(state->config->windowResizable);
	errorHandlingSetup();
	logPrint();
	state->window.handle = glfwCreateWindow(state->config->windowWidth, state->config->windowHeight, state->config->windowTitle.c_str(), nullptr, nullptr);

	glfwSetFramebufferSizeCallback(state->window.handle, framebufferResizeCallback);
	
	instanceCreate(state);
	surfaceCreate(state);
}

void windowDestroy(State* state) {
	surfaceDestroy(state);
	instanceDestroy(state);
	glfwDestroyWindow(state->window.handle);
	glfwTerminate();
};

//Draw
void frameDraw(State* state) {
	vkWaitForFences(state->context->device, 1, &state->renderer->inFlightFence[state->renderer->frameIndex], VK_TRUE, UINT64_MAX);
	VkResult result = vkAcquireNextImageKHR(state->context->device, state->window.swapchain.handle, UINT64_MAX, state->renderer->imageAvailableSemaphore[state->renderer->frameIndex], VK_NULL_HANDLE, &state->renderer->imageAquiredIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		swapchainRecreate(state);
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to present swap chain image!");
	}
	vkResetFences(state->context->device, 1, &state->renderer->inFlightFence[state->renderer->frameIndex]);
	vkResetCommandBuffer(state->buffers->commandBuffer[state->renderer->frameIndex],/*VkCommandBufferResetFlagBits*/0);
	commandBufferRecord(state);


	VkSemaphore waitSemaphores[] = { state->renderer->imageAvailableSemaphore[state->renderer->frameIndex] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore signalSemaphores[] = { state->renderer->renderFinishedSemaphore[state->renderer->frameIndex] };
	VkSwapchainKHR swapChains[] = { state->window.swapchain.handle };

	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = waitSemaphores,
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &state->buffers->commandBuffer[state->renderer->frameIndex],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = signalSemaphores,
	};

	PANIC(vkQueueSubmit(state->context->queue, 1, &submitInfo, state->renderer->inFlightFence[state->renderer->frameIndex]), "Failed To Submit Queue");
	VkPresentInfoKHR presentInfo{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,

		.waitSemaphoreCount = 1,
		.pWaitSemaphores = signalSemaphores,
		.swapchainCount = 1,
		.pSwapchains = swapChains,
		.pImageIndices = &state->renderer->imageAquiredIndex,
		.pResults = nullptr, // Optional
	};
	result = vkQueuePresentKHR(state->context->presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || state->window.framebufferResized) {
		state->window.framebufferResized = false;
		swapchainRecreate(state);
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}
	state->renderer->frameIndex = (state->renderer->frameIndex + 1) % state->config->swapchainBuffering;
};

void updateFPS(State* state) {
	static int frameCount = 0;
	static double lastTime = glfwGetTime();

	double currentTime = glfwGetTime();
	frameCount++;

	if (currentTime - lastTime >= 1.0) {
		state->gui->io.Framerate = frameCount;
		frameCount = 0;
		lastTime = currentTime;
	}
}