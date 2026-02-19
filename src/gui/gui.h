#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

#include "gui/imgui/imgui.h"
#include "gui/imgui/imgui_impl_glfw.h"
#include "gui/imgui/imgui_impl_vulkan.h"


struct State;

struct Gui{
    VkDescriptorPool descriptorPool;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    ImGuiIO io;
    ImGuiStyle style;
    std::vector<VkFramebuffer> framebuffers;
};

void guiDescriptorPoolCreate(State* state);
void guiRenderPassCreate(State* state);
void guiRenderPassDestroy(State* state);
void guiFramebuffersCreate(State* state);
void guiFramebuffersDestroy(State* state);

void guiInit(State* state);
void guiDraw(State* state, VkCommandBuffer cmd);
void guiClean(State* state);