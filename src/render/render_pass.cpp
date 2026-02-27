#include "render/render_pass.h"
#include "core/config.h"
#include "core/context.h"
#include "render/renderer.h"
#include "resources/buffers.h"
#include "scene/texture.h"
#include "resources/images.h"
#include "core/state.h"
//Utility
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

//RenderPasses
void opaqueRenderPassCreate(State* state) {
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
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


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
        resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
            &state->renderer->opaqueRenderPass),
        "Failed To Create Render Pass"
    );
}
void opaqueRenderPassDestroy(State* state) {
	vkDestroyRenderPass(state->context->device, state->renderer->opaqueRenderPass, nullptr);
};

void transparentRenderPassCreate(State* state)
{
    // 0: accumulation
    VkAttachmentDescription accum{};
    accum.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    accum.samples = VK_SAMPLE_COUNT_1_BIT;
    accum.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    accum.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    accum.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    accum.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    accum.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    accum.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 1: revealage
    VkAttachmentDescription reveal{};
    reveal.format = VK_FORMAT_R8_UNORM;
    reveal.samples = VK_SAMPLE_COUNT_1_BIT;
    reveal.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    reveal.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    reveal.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    reveal.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    reveal.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    reveal.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // 2: depth (same as before)
    VkAttachmentDescription depth{};
    depth.format = findDepthFormat(state);
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;   // keep opaque depth
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRefs[2];
    colorRefs[0].attachment = 0;
    colorRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorRefs[1].attachment = 1;
    colorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 2;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 2;
    subpass.pColorAttachments = colorRefs;
    subpass.pDepthStencilAttachment = &depthRef;

    std::array<VkAttachmentDescription, 3> attachments{ accum, reveal, depth };

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = static_cast<uint32_t>(attachments.size());
    info.pAttachments = attachments.data();
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dep;

    PANIC(vkCreateRenderPass(state->context->device, &info, nullptr,
        &state->renderer->transparencyRenderPass),
        "Failed to create transparent render pass");
}
void transparentRenderPassDestroy(State* state) {
    vkDestroyRenderPass(state->context->device, state->renderer->transparencyRenderPass, nullptr);
};

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


//Resources
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

void sceneColorResourceCreate(State* state)
{
    VkExtent2D extent = state->window.swapchain.imageExtent;

    // Opaque scene color uses the swapchain format
    VkFormat sceneColorFormat = state->window.swapchain.format;

    // Transparency targets: MUST match transparentRenderPass
    VkFormat transAccumFormat = VK_FORMAT_R16G16B16A16_SFLOAT; // attachment 0
    VkFormat transRevealFormat = VK_FORMAT_R8_UNORM;            // attachment 1

    // Scene color (opaque pass target)
    imageCreate(
        state,
        extent.width,
        extent.height,
        sceneColorFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        state->texture->sceneColorImage,
        state->texture->sceneColorImageMemory,
        1,
        VK_SAMPLE_COUNT_1_BIT
    );

    state->texture->sceneColorImageView =
        imageViewCreate(
            state,
            state->texture->sceneColorImage,
            sceneColorFormat,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1
        );

    // Transparency accum (matches transparentRenderPass attachment 0)
    imageCreate(
        state,
        extent.width,
        extent.height,
        transAccumFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        state->texture->transAccumImage,
        state->texture->transAccumImageMemory,
        1,
        VK_SAMPLE_COUNT_1_BIT
    );

    state->texture->transAccumImageView =
        imageViewCreate(
            state,
            state->texture->transAccumImage,
            transAccumFormat,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1
        );

    // Transparency reveal (matches transparentRenderPass attachment 1)
    imageCreate(
        state,
        extent.width,
        extent.height,
        transRevealFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        state->texture->transRevealImage,
        state->texture->transRevealImageMemory,
        1,
        VK_SAMPLE_COUNT_1_BIT
    );

    state->texture->transRevealImageView =
        imageViewCreate(
            state,
            state->texture->transRevealImage,
            transRevealFormat,
            VK_IMAGE_ASPECT_COLOR_BIT,
            1
        );
}


void sceneColorResourceDestroy(State* state) {
	vkDestroyImageView(state->context->device, state->texture->sceneColorImageView, nullptr);
	vkDestroyImage(state->context->device, state->texture->sceneColorImage, nullptr);
	vkFreeMemory(state->context->device, state->texture->sceneColorImageMemory, nullptr);

	vkDestroyImageView(state->context->device, state->texture->transAccumImageView, nullptr);
	vkDestroyImage(state->context->device, state->texture->transAccumImage, nullptr);
	vkFreeMemory(state->context->device, state->texture->transAccumImageMemory, nullptr);
	
    vkDestroyImageView(state->context->device, state->texture->transRevealImageView, nullptr);
	vkDestroyImage(state->context->device, state->texture->transRevealImage, nullptr);
	vkFreeMemory(state->context->device, state->texture->transRevealImageMemory, nullptr);
};

void depthResourceCreate(State* state) {
    VkFormat depthFormat = findDepthFormat(state);
    imageCreate(state, state->window.swapchain.imageExtent.width, state->window.swapchain.imageExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->texture->msaaDepthImage, state->texture->msaaDepthImageMemory, 1, state->config->msaaSamples);
    state->texture->msaaDepthImageView = imageViewCreate(state, state->texture->msaaDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
    transitionImageLayout(state, state->texture->msaaDepthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 1);

    imageCreate(state, state->window.swapchain.imageExtent.width, state->window.swapchain.imageExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->texture->singleDepthImage, state->texture->singleDepthImageMemory, 1, VK_SAMPLE_COUNT_1_BIT);
    state->texture->singleDepthImageView = imageViewCreate(state, state->texture->singleDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
    transitionImageLayout(state, state->texture->singleDepthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1, 1);

    imageCreate(state, state->window.swapchain.imageExtent.width, state->window.swapchain.imageExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->texture->sceneDepthImage, state->texture->sceneDepthImageMemory, 1, VK_SAMPLE_COUNT_1_BIT);
    state->texture->sceneDepthImageView = imageViewCreate(state, state->texture->sceneDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
    transitionImageLayout(state, state->texture->sceneDepthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 1);
}
void depthResourceDestroy(State* state) {
	vkDestroyImageView(state->context->device, state->texture->msaaDepthImageView, nullptr);
	vkDestroyImage(state->context->device, state->texture->msaaDepthImage, nullptr);
	vkFreeMemory(state->context->device, state->texture->msaaDepthImageMemory, nullptr);
	
    vkDestroyImageView(state->context->device, state->texture->singleDepthImageView, nullptr);
	vkDestroyImage(state->context->device, state->texture->singleDepthImage, nullptr);
	vkFreeMemory(state->context->device, state->texture->singleDepthImageMemory, nullptr);

    vkDestroyImageView(state->context->device, state->texture->sceneDepthImageView, nullptr);
    vkDestroyImage(state->context->device, state->texture->sceneDepthImage, nullptr);
    vkFreeMemory(state->context->device, state->texture->sceneDepthImageMemory, nullptr);
};