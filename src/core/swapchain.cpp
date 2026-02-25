#include "render/command_buffers.h"
#include "render/frame_buffers.h"
#include "render/descriptors.h"
#include "render/render_pass.h"
#include "resources/images.h"
#include "gui/gui.h"
#include "core/swapchain.h"
#include "core/context.h"
#include "core/config.h"
#include "core/state.h"

//utility
static uint32_t clamp(uint32_t value, uint32_t min, uint32_t max) {
	if (value < min) {
		return min;
	}
	else if (value > max) {
		return max;
	};
	return value;
};

//Swapchain
VkSurfaceCapabilitiesKHR surfaceCapabilitiesGet(State* state) {
	PANIC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state->context->physicalDevice, state->window.surface, &state->window.surfaceCapabilities), "Failed To Get Surface Cababilities");
	return state->window.surfaceCapabilities;
};
VkSurfaceFormatKHR surfaceFormatSelect(State* state) {
	uint32_t formatCount;
	PANIC(vkGetPhysicalDeviceSurfaceFormatsKHR(state->context->physicalDevice, state->window.surface, &formatCount, nullptr), "Failed To Get Format Count");
	VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
	PANIC(!formats, "Failed To Allocate Formats Memory");
	PANIC(vkGetPhysicalDeviceSurfaceFormatsKHR(state->context->physicalDevice, state->window.surface, &formatCount, formats), "Failed To Get Format Count");

	uint32_t formatIndex = 0;
	for (int i = 0; i < (int)formatCount; i++) {
		VkSurfaceFormatKHR format = formats[i];
		if (format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR && format.format == VK_FORMAT_B8G8R8A8_SRGB) {
			formatIndex = i;
			break;
		};
	};
	VkSurfaceFormatKHR format = formats[formatIndex];
	free(formats);
	return format;
};
VkPresentModeKHR presentModeSelect(State* state) {
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	uint32_t presentModeCount;
	PANIC(vkGetPhysicalDeviceSurfacePresentModesKHR(state->context->physicalDevice, state->window.surface, &presentModeCount, nullptr), "Failed To Get Present Mode Count");
	VkPresentModeKHR *presentModes = (VkPresentModeKHR*)malloc(presentModeCount * sizeof(VkPresentModeKHR));
	PANIC(!presentModes, "Failed To Allocate Present Mode Memory");
	PANIC(vkGetPhysicalDeviceSurfacePresentModesKHR(state->context->physicalDevice, state->window.surface, &presentModeCount, presentModes), "Failed To Get Surface Present Modes");

	for (uint32_t i = 0; i < presentModeCount; i++) {
		if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
	}

	free(presentModes);
	return presentMode;
};

void swapchainImageGet(State* state) {
	PANIC(vkGetSwapchainImagesKHR(state->context->device, state->window.swapchain.handle, &state->window.swapchain.imageCount, nullptr), "Failed to Get Swapchain Image Count");
	state->window.swapchain.images.resize(state->window.swapchain.imageCount);
	PANIC(!state->window.swapchain.images.data(), "Failed To Enumerate Swapchain Image Memory");
	PANIC(vkGetSwapchainImagesKHR(state->context->device, state->window.swapchain.handle, &state->window.swapchain.imageCount, state->window.swapchain.images.data()), "Failed To Get Swapchain Images");
};
void imageViewsCreate(State* state) {
	state->window.swapchain.imageViews = (VkImageView *)malloc(state->window.swapchain.imageCount * sizeof(VkImageView));
	PANIC(!state->window.swapchain.imageViews, "Failed To Count ImageViews");

	for (uint32_t i = 0; i < state->window.swapchain.imageCount; i++) {
		state->window.swapchain.imageViews[i] = imageViewCreate(state, state->window.swapchain.images[i], state->window.swapchain.format, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}
};
void imageViewsDestroy(State* state) {
	for (int i = 0; i < (int)state->window.swapchain.imageCount; i++) {
		vkDestroyImageView(state->context->device, state->window.swapchain.imageViews[i], nullptr);
	};
};

void swapchainCreate(State* state) {
	VkSurfaceCapabilitiesKHR capabilities = surfaceCapabilitiesGet(state);
	VkSurfaceFormatKHR surfaceFormat = surfaceFormatSelect(state);
	VkPresentModeKHR presentMode = presentModeSelect(state);

	state->window.swapchain.format = surfaceFormat.format;
	state->window.swapchain.colorSpace = surfaceFormat.colorSpace;
	state->window.swapchain.imageExtent = capabilities.currentExtent;

	VkSwapchainCreateInfoKHR swapchainInfo{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = state->window.surface,
		.minImageCount = clamp(state->config->swapchainBuffering, capabilities.minImageCount, capabilities.maxImageCount ? capabilities.maxImageCount : UINT32_MAX),
		.imageFormat = state->window.swapchain.format,
		.imageColorSpace = state->window.swapchain.colorSpace,
		.imageExtent = state->window.swapchain.imageExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.queueFamilyIndexCount = 1,
		.pQueueFamilyIndices = &state->context->queueFamilyIndex,
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = presentMode,
		.clipped = true,
	};

	PANIC(vkCreateSwapchainKHR(state->context->device, &swapchainInfo, nullptr, &state->window.swapchain.handle),"Failed To Create Swapchain");

};
void swapchainDestroy(State* state) {
	vkDestroySwapchainKHR(state->context->device, state->window.swapchain.handle, nullptr);
};

void swapchainCleanup(State* state) {
	sceneColorResourceDestroy(state);
	colorResourceDestroy(state);
	depthResourceDestroy(state);
	presentFramebuffersDestroy(state);
	transparentFrameBuffersDestroy(state);
	opaqueFrameBuffersDestroy(state);
	imageViewsDestroy(state);
	swapchainDestroy(state);
	guiFramebuffersDestroy(state);
};
void swapchainRecreate(State* state) {
	int width = 0, height = 0;
	glfwGetFramebufferSize(state->window.handle, &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(state->window.handle, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(state->context->device);
	swapchainCleanup(state);
	swapchainCreate(state);
	swapchainImageGet(state);
	
	transitionSwapchainImagesToPresent(state);

	imageViewsCreate(state);
	sceneColorResourceCreate(state);
	colorResourceCreate(state);
	depthResourceCreate(state);
	opaqueFrameBuffersCreate(state);
	transparentFrameBuffersCreate(state);
	presentFramebuffersCreate(state);
	presentDescriptorSetUpdate(state);
	guiFramebuffersCreate(state);
	ImGui_ImplVulkan_SetMinImageCount(state->window.swapchain.imageCount);
	commandBufferRecord(state);
};