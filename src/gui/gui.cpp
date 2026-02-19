#include "gui/gui.h"
#include "core/context.h"
#include "render/renderer.h"
#include "core/state.h"

void guiDescriptorPoolCreate(State* state) {
    VkDescriptorPoolSize pool_sizes[] =
    {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    vkCreateDescriptorPool(state->context->device, &pool_info, nullptr, &state->gui->descriptorPool);
};
void guiRenderPassCreate(State* state) {
    VkAttachmentDescription attachment{};
    attachment.format = state->window.swapchain.format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Correct layouts for a swapchain-backed GUI pass
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &attachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;

    vkCreateRenderPass(state->context->device, &rpInfo, nullptr, &state->gui->renderPass);
}
void guiRenderPassDestroy(State* state) {
    vkDestroyRenderPass(state->context->device, state->gui->renderPass, nullptr);
}

void guiFramebuffersCreate(State* state) {
    state->gui->framebuffers.resize(state->window.swapchain.imageCount);
    for (size_t i = 0; i < state->window.swapchain.imageCount; i++) {
        VkImageView attachments[] = { state->window.swapchain.imageViews[i] };
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = state->gui->renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = state->window.swapchain.imageExtent.width;
        fbInfo.height = state->window.swapchain.imageExtent.height;
        fbInfo.layers = 1;
        vkCreateFramebuffer(state->context->device, &fbInfo, nullptr, &state->gui->framebuffers[i]);
    }
}
void guiFramebuffersDestroy(State* state) {
    for (auto fb : state->gui->framebuffers) {
        vkDestroyFramebuffer(state->context->device, fb, nullptr);
    }
    state->gui->framebuffers.clear();
}

void guiInit(State* state) {
    ImGui::CreateContext();
    state->gui->io = ImGui::GetIO();

    state->gui->io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    state->gui->io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    state->gui->io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	state->gui->io.IniFilename = nullptr; // disable saving .ini file
    ImGui::LoadIniSettingsFromMemory("");
    ImGui_ImplGlfw_InitForVulkan(state->window.handle, true);

    // 1. Load default font at custom size
    float fontSize = 34;
	state->gui->io.Fonts->AddFontFromFileTTF("src/gui/fonts/digital.ttf", fontSize);

    // 2. Init Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance       = state->context->instance;
    initInfo.PhysicalDevice = state->context->physicalDevice;
    initInfo.Device         = state->context->device;
    initInfo.QueueFamily    = state->context->queueFamilyIndex;
    initInfo.Queue          = state->context->queue;
    initInfo.DescriptorPool = state->gui->descriptorPool;
    initInfo.MinImageCount  = state->window.swapchain.imageCount;
    initInfo.ImageCount     = state->window.swapchain.imageCount;
    initInfo.UseDynamicRendering = false;
    initInfo.PipelineInfoMain.RenderPass        = state->gui->renderPass;
    initInfo.PipelineInfoForViewports.RenderPass = state->gui->renderPass;

    ImGui_ImplVulkan_Init(&initInfo);

    float scale = fontSize / 16.0f;

    // 3. Scale the REAL style (not a copy)
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(fontSize / 16.0f);

    // Docking node padding
    style.WindowPadding = ImVec2(8 * scale, 8 * scale);
    style.FramePadding = ImVec2(6 * scale, 4 * scale);
    style.ItemSpacing = ImVec2(8 * scale, 6 * scale);
    style.TabRounding = 4 * scale;
    style.ScrollbarSize = 14 * scale;
    style.WindowRounding = 4 * scale;
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.15, 0.075, 0.025, 0.85);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0, 0.0, 0.0, 0.85);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1, 0.1, 0.1, 0.75); // transparent background)
    style.Colors[ImGuiCol_Border] = ImVec4(0.15, 0.075, 0.025, 1.0);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.20f, 0.20f, 0.0f, 0.85f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.15f, 0.075f, 0.025f, 0.90f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.15f, 0.075f, 0.025f, 1.0f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.15f, 0.075f, 0.025f, 0.0f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.20f, 0.20f, 0.0f, 0.85f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.15f, 0.075f, 0.025f, 0.90f);
	style.Colors[ImGuiCol_Text] = ImVec4(0.2f, 0.5f, 0.0f, 0.90f);
	state->gui->style = style;
}

void guiDraw(State* state, VkCommandBuffer cmd) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Build UI
    ImGui::Begin("FPS");
    ImGui::Text("  %.1f   ", state->gui->io.Framerate);
    ImGui::End();

    ImGui::Render();

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = state->gui->renderPass;
    rpInfo.framebuffer = state->gui->framebuffers[state->renderer->imageAquiredIndex];
    rpInfo.renderArea.offset = { 0, 0 };
    rpInfo.renderArea.extent = state->window.swapchain.imageExtent;
    rpInfo.clearValueCount = 0;
    rpInfo.pClearValues = nullptr;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd, VK_NULL_HANDLE);
    vkCmdEndRenderPass(cmd);
}

void guiClean(State* state) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(state->context->device, state->gui->descriptorPool, nullptr);
    guiRenderPassDestroy(state);
}