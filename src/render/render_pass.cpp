#include "render/render_pass.h"
#include "core/config.h"
#include "core/context.h"
#include "render/renderer.h"
#include "resources/buffers.h"
#include "scene/texture.h"
#include "resources/images.h"
#include "core/state.h"

void renderPassCreate(State* state) {
    bool msaa = (state->config->msaaSamples != VK_SAMPLE_COUNT_1_BIT);

    // ─────────────────────────────────────────────
    // Attachment 0: Color (MSAA or single-sample)
    // ─────────────────────────────────────────────
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = state->window.swapchain.format;
    colorAttachment.samples = state->config->msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout =
        msaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    // ─────────────────────────────────────────────
    // Attachment 1: Depth
    // ─────────────────────────────────────────────
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat(state);
    depthAttachment.samples = state->config->msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    // ─────────────────────────────────────────────
    // Subpass
    // ─────────────────────────────────────────────
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // ─────────────────────────────────────────────
    // MSAA resolve attachment (only if MSAA enabled)
    // ─────────────────────────────────────────────
    VkAttachmentDescription resolveAttachment{};
    VkAttachmentReference resolveAttachmentRef{};

    std::array<VkAttachmentDescription, 3> attachments3{};
    std::array<VkAttachmentDescription, 2> attachments2{};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    if (msaa) {
        // Resolve attachment
        resolveAttachment.format = state->window.swapchain.format;
        resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        resolveAttachmentRef.attachment = 2;
        resolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        subpass.pResolveAttachments = &resolveAttachmentRef;

        attachments3 = { colorAttachment, depthAttachment, resolveAttachment };
        renderPassInfo.attachmentCount = 3;
        renderPassInfo.pAttachments = attachments3.data();
    }
    else {
        // No resolve attachment
        subpass.pResolveAttachments = nullptr;

        attachments2 = { colorAttachment, depthAttachment };
        renderPassInfo.attachmentCount = 2;
        renderPassInfo.pAttachments = attachments2.data();
    }

    // ─────────────────────────────────────────────
    // Dependency
    // ─────────────────────────────────────────────
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    PANIC(
        vkCreateRenderPass(state->context->device, &renderPassInfo, nullptr,
            &state->renderer->renderPass),
        "Failed To Create Render Pass"
    );
}

void renderPassDestroy(State* state) {
	vkDestroyRenderPass(state->context->device, state->renderer->renderPass, nullptr);
};

VkSampleCountFlagBits getMaxUsableSampleCount(State* state) {
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(state->context->physicalDevice, &props);

	VkSampleCountFlags counts =
		props.limits.framebufferColorSampleCounts &
		props.limits.framebufferDepthSampleCounts;

	if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
	if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
	if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
	if (counts & VK_SAMPLE_COUNT_8_BIT)  return VK_SAMPLE_COUNT_8_BIT;
	if (counts & VK_SAMPLE_COUNT_4_BIT)  return VK_SAMPLE_COUNT_4_BIT;
	if (counts & VK_SAMPLE_COUNT_2_BIT)  return VK_SAMPLE_COUNT_2_BIT;

	return VK_SAMPLE_COUNT_1_BIT;
}

void presentRenderPassCreate(State* state) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = state->window.swapchain.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &colorAttachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;

    PANIC(vkCreateRenderPass(state->context->device, &info, nullptr,
        &state->renderer->presentRenderPass),
        "Failed to create present render pass");
}
void presentRenderPassDestroy(State* state) {
    vkDestroyRenderPass(state->context->device, state->renderer->presentRenderPass, nullptr);
};


void colorResourceCreate(State* state) {
	VkFormat colorFormat = state->window.swapchain.format;
	printf("colorResourceCreate: swapchain format=%d extent=%ux%u\n",
       state->window.swapchain.format,
       state->window.swapchain.imageExtent.width,
       state->window.swapchain.imageExtent.height);

	imageCreate(state, state->window.swapchain.imageExtent.width, state->window.swapchain.imageExtent.height, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->texture->colorImage, state->texture->colorImageMemory, 1, state->config->msaaSamples);
	state->texture->colorImageView = imageViewCreate(state, state->texture->colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}
void colorResourceDestroy(State* state) {
	vkDestroyImageView(state->context->device, state->texture->colorImageView, nullptr);
	vkDestroyImage(state->context->device, state->texture->colorImage, nullptr);
	vkFreeMemory(state->context->device, state->texture->colorImageMemory, nullptr);
};

void sceneColorResourceCreate(State* state) {
	VkFormat colorFormat = state->window.swapchain.format;

	imageCreate(
		state,
		state->window.swapchain.imageExtent.width,
		state->window.swapchain.imageExtent.height,
		colorFormat,
		VK_IMAGE_TILING_OPTIMAL,
		// NOTE: no TRANSIENT, and we add SAMPLED
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		state->texture->sceneColorImage,
		state->texture->sceneColorImageMemory,
		1, // mipLevels
		VK_SAMPLE_COUNT_1_BIT // single-sample
	);

	state->texture->sceneColorImageView =
		imageViewCreate(
			state,
			state->texture->sceneColorImage,
			colorFormat,
			VK_IMAGE_ASPECT_COLOR_BIT,
			1
		);
}
void sceneColorResourceDestroy(State* state) {
	vkDestroyImageView(state->context->device, state->texture->sceneColorImageView, nullptr);
	vkDestroyImage(state->context->device, state->texture->sceneColorImage, nullptr);
	vkFreeMemory(state->context->device, state->texture->sceneColorImageMemory, nullptr);
};

void depthResourceCreate(State* state) {
	VkFormat depthFormat = findDepthFormat(state);
	imageCreate(state, state->window.swapchain.imageExtent.width, state->window.swapchain.imageExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->texture->depthImage, state->texture->depthImageMemory, 1, state->config->msaaSamples);
	state->texture->depthImageView = imageViewCreate(state, state->texture->depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
	transitionImageLayout(state, state->texture->depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
};
void depthBufferDestroy(State* state) {
	vkDestroyImageView(state->context->device, state->texture->depthImageView, nullptr);
	vkDestroyImage(state->context->device, state->texture->depthImage, nullptr);
	vkFreeMemory(state->context->device, state->texture->depthImageMemory, nullptr);
};
