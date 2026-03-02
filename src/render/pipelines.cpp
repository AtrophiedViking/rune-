#include "render/pipelines.h"
#include "render/renderer.h"
#include "resources/images.h"
#include "scene/mesh.h"
#include "scene/skybox.h"
#include "scene/texture.h"
#include "core/context.h"
#include "core/config.h"
#include "core/state.h"

//utility
static std::vector<char> shaderRead(const char* filePath) {
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);
	PANIC(!file.is_open(), "Failed To Open Shader: %s", filePath);
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();
	return buffer;
};
//Compute Pipelines
void iblPipelineCreate(State* state)
{
	// 1) Load SPIR-V
	auto computeShaderCode = shaderRead("./res/shaders/ibl_compute.spv");

	VkShaderModuleCreateInfo shaderModuleInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = computeShaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(computeShaderCode.data()),
	};

	VkShaderModule computeShaderModule;
	PANIC(
		vkCreateShaderModule(state->context->device, &shaderModuleInfo, nullptr, &computeShaderModule),
		"Failed to create IBL compute shader module"
	);

	// 2) Push constants
	VkPushConstantRange pushRange{
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.offset = 0,
		.size = sizeof(int) * 3 + sizeof(float) // face, mip, roughness, mipResolution
	};

	// 3) Pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &state->renderer->iblSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushRange
	};

	PANIC(
		vkCreatePipelineLayout(
			state->context->device,
			&pipelineLayoutInfo,
			nullptr,
			&state->renderer->iblPipelineLayout
		),
		"Failed to create IBL compute pipeline layout"
	);

	// 4) Compute pipeline
	VkComputePipelineCreateInfo pipelineInfo{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.module = computeShaderModule,
			.pName = "main"
		},
		.layout = state->renderer->iblPipelineLayout,
	};

	PANIC(
		vkCreateComputePipelines(
			state->context->device,
			VK_NULL_HANDLE,
			1,
			&pipelineInfo,
			nullptr,
			&state->renderer->iblPipeline
		),
		"Failed to create IBL compute pipeline"
	);

	vkDestroyShaderModule(state->context->device, computeShaderModule, nullptr);
}

//Graphics Pipelines
void skyboxPipelineCreate(State* state) {
	//ShaderModules
	auto vertShaderCode = shaderRead("./res/shaders/skybox_vert.spv");
	auto fragShaderCode = shaderRead("./res/shaders/skybox_frag.spv");
	VkShaderModuleCreateInfo vertShaderModuleInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = vertShaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data()),
	};
	PANIC(vkCreateShaderModule(state->context->device, &vertShaderModuleInfo, nullptr, &state->renderer->skyboxVertShaderModule), "Failed To Create Vertex Shader Module");
	VkShaderModuleCreateInfo fragShaderModuleInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = fragShaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data()),
	};
	PANIC(vkCreateShaderModule(state->context->device, &fragShaderModuleInfo, nullptr, &state->renderer->skyboxFragShaderModule), "Failed To Create Fragment Shader Module");

	//ShaderStages
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = state->renderer->skyboxVertShaderModule,
		.pName = "main"
	};
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = state->renderer->skyboxFragShaderModule,
		.pName = "main"
	};
	VkPipelineShaderStageCreateInfo shaderStages[]{ vertShaderStageInfo, fragShaderStageInfo };
	
	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamicStates,
	};
	//VertexInputs
	VkVertexInputBindingDescription skyboxBinding{ 
		skyboxBinding.binding = 0,
		skyboxBinding.stride = sizeof(SkyboxVertex),
		skyboxBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	VkVertexInputAttributeDescription skyboxAttr{
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(SkyboxVertex, pos),
	};
	VkPipelineVertexInputStateCreateInfo vertexInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &skyboxBinding,
		.vertexAttributeDescriptionCount = 1,
		.pVertexAttributeDescriptions = &skyboxAttr,
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	//ViewPort
	VkViewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)state->window.swapchain.imageExtent.width,
		.height = (float)state->window.swapchain.imageExtent.height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	VkRect2D scissor{
		.offset = { 0, 0 },
		.extent = state->window.swapchain.imageExtent,
	};
	VkPipelineViewportStateCreateInfo viewportState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};
	//depthStencil
	VkPipelineDepthStencilStateCreateInfo depthStencil{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_FALSE,
		.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
	};

	//Rasterizor
	VkPipelineRasterizationStateCreateInfo rasterizer{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_FRONT_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f,
	};
	//MultiSampling
	VkPipelineMultisampleStateCreateInfo multisampling{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = state->config->msaaSamples,
		.sampleShadingEnable = VK_TRUE,
		.minSampleShading = 0.1f, // Optional
		.pSampleMask = nullptr, // Optional
		.alphaToCoverageEnable = VK_FALSE, // Optional
		.alphaToOneEnable = VK_FALSE, // Optional
	};
	//ColorBlending
	VkPipelineColorBlendAttachmentState colorBlendAttachment{
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};
	VkPipelineColorBlendStateCreateInfo colorBlending{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
	};

	VkDescriptorSetLayout setLayouts[] = { state->renderer->globalSetLayout };
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = setLayouts,
	};
	PANIC(vkCreatePipelineLayout(state->context->device, &pipelineLayoutInfo, nullptr, &state->renderer->skyboxPipelineLayout), "Failed To Create Pipeline Layout");

	VkGraphicsPipelineCreateInfo pipelineInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shaderStages,
		.pVertexInputState = &vertexInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depthStencil,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicState,
		.layout = state->renderer->skyboxPipelineLayout,
		.renderPass = state->renderer->opaqueRenderPass,
		.subpass = 0,
	};
	PANIC(vkCreateGraphicsPipelines(state->context->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &state->renderer->skyboxPipeline), "Failed To Create Graphics Pipeline");
}
void skyboxPipelineDestroy(State* state) {
	vkDestroyPipeline(state->context->device, state->renderer->skyboxPipeline, nullptr);
	vkDestroyPipelineLayout(state->context->device, state->renderer->skyboxPipelineLayout, nullptr);
	vkDestroyShaderModule(state->context->device, state->renderer->skyboxVertShaderModule, nullptr);
	vkDestroyShaderModule(state->context->device, state->renderer->skyboxFragShaderModule, nullptr);
}

void opaquePipelineCreate(State* state) {
	//ShaderModules
	auto vertShaderCode = shaderRead("./res/shaders/vert.spv");
	auto fragShaderCode = shaderRead("./res/shaders/opaque_frag.spv");
	VkShaderModuleCreateInfo vertShaderModuleInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = vertShaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data()),
	};
	PANIC(vkCreateShaderModule(state->context->device, &vertShaderModuleInfo, nullptr, &state->renderer->vertShaderModule), "Failed To Create Vertex Shader Module");
	VkShaderModuleCreateInfo fragShaderModuleInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = fragShaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data()),
	};
	PANIC(vkCreateShaderModule(state->context->device, &fragShaderModuleInfo, nullptr, &state->renderer->opaqueFragShaderModule), "Failed To Create Fragment Shader Module");
	//ShaderStages
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = state->renderer->vertShaderModule,
		.pName = "main"
	};
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = state->renderer->opaqueFragShaderModule,
		.pName = "main"
	};
	VkPipelineShaderStageCreateInfo shaderStages[]{ vertShaderStageInfo, fragShaderStageInfo };

	//DynamicStates
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = (uint32_t)dynamicStates.size(),
		.pDynamicStates = dynamicStates.data(),
	};
	//VertexInputs
	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = (uint32_t)attributeDescriptions.size(),
		.pVertexAttributeDescriptions = attributeDescriptions.data(),
	};

	//InputAssembly
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	//ViewPort
	VkViewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)state->window.swapchain.imageExtent.width,
		.height = (float)state->window.swapchain.imageExtent.height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	VkRect2D scissor{
		.offset = { 0, 0 },
		.extent = state->window.swapchain.imageExtent,
	};
	VkPipelineViewportStateCreateInfo viewportState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};
	//Rasterizor
	VkPipelineRasterizationStateCreateInfo rasterizer{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_TRUE,
		.lineWidth = 1.0f,
	};
	//MultiSampling
	VkPipelineMultisampleStateCreateInfo multisampling{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = state->config->msaaSamples,
		.sampleShadingEnable = VK_TRUE,
		.minSampleShading = 0.1f, // Optional
		.pSampleMask = nullptr, // Optional
		.alphaToCoverageEnable = VK_FALSE, // Optional
		.alphaToOneEnable = VK_FALSE, // Optional
	};
	//ColorBlending
	VkPipelineColorBlendAttachmentState colorBlendAttachment{
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	};
	VkPipelineColorBlendStateCreateInfo colorBlending{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
	};

	VkPushConstantRange pushConstantRange{
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(PushConstantBlock),
	};

	VkDescriptorSetLayout setLayouts[] = {
	state->renderer->globalSetLayout,   // set = 0
	state->renderer->materialSetLayout   // set = 1
	};
	//PipelineLayout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 2,
		.pSetLayouts = setLayouts,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange
	};
	PANIC(vkCreatePipelineLayout(state->context->device, &pipelineLayoutInfo, nullptr, &state->renderer->opaquePipelineLayout), "Failed To Create Pipeline Layout");

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;

	//GraphicsPipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shaderStages,
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depthStencil,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicState,
		.layout = state->renderer->opaquePipelineLayout,
		.renderPass = state->renderer->opaqueRenderPass,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE, // Optional
	};
	PANIC(vkCreateGraphicsPipelines(state->context->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &state->renderer->opaquePipeline), "Failed To Create GraphicsPipeline");
};
void opaquePipelineDestroy(State* state) {
	vkDestroyPipelineLayout(state->context->device, state->renderer->opaquePipelineLayout, nullptr);
	vkDestroyPipeline(state->context->device, state->renderer->opaquePipeline, nullptr);
	vkDestroyShaderModule(state->context->device, state->renderer->opaqueFragShaderModule, nullptr);
};

void transparencyPipelineCreate(State* state) {
	// Fragment shader (you probably meant frag here, name is fine though)
	auto fragShaderCode = shaderRead("./res/shaders/transparent_frag.spv");
	VkShaderModuleCreateInfo fragShaderModuleInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = fragShaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data()),
	};
	PANIC(
		vkCreateShaderModule(state->context->device, &fragShaderModuleInfo, nullptr,
			&state->renderer->transparentFragShaderModule),
		"Failed To Create Fragment Shader Module"
	);

	// Shader stages
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = state->renderer->vertShaderModule,
		.pName = "main"
	};
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = state->renderer->transparentFragShaderModule,
		.pName = "main"
	};
	VkPipelineShaderStageCreateInfo shaderStages[]{ vertShaderStageInfo, fragShaderStageInfo };

	// Dynamic state
	std::vector<VkDynamicState> dynamicStates{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
		.pDynamicStates = dynamicStates.data(),
	};

	// Vertex input
	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &bindingDescription,
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
		.pVertexAttributeDescriptions = attributeDescriptions.data(),
	};

	// Input assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	// Viewport/scissor (will be overridden by dynamic state, but fine)
	VkViewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(state->window.swapchain.imageExtent.width),
		.height = static_cast<float>(state->window.swapchain.imageExtent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	VkRect2D scissor{
		.offset = { 0, 0 },
		.extent = state->window.swapchain.imageExtent,
	};
	VkPipelineViewportStateCreateInfo viewportState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor
	};

	// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizer{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f,
	};

	// Multisampling: MUST be 1x to match transparent render pass
	VkPipelineMultisampleStateCreateInfo multisampling{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
	};

	// attachment 0: accumulation
	VkPipelineColorBlendAttachmentState accumBlend{};
	accumBlend.blendEnable = VK_TRUE;
	accumBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	// C_accum += vec4(color.rgb * alpha, alpha)
	accumBlend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	accumBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	accumBlend.colorBlendOp = VK_BLEND_OP_ADD;
	accumBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	accumBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	accumBlend.alphaBlendOp = VK_BLEND_OP_ADD;

	// attachment 1: revealage
	VkPipelineColorBlendAttachmentState revealBlend{};
	revealBlend.blendEnable = VK_TRUE;
	revealBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
	// R_reveal *= (1 - alpha)
	revealBlend.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	revealBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	revealBlend.colorBlendOp = VK_BLEND_OP_ADD;
	revealBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	revealBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	revealBlend.alphaBlendOp = VK_BLEND_OP_ADD;

	std::array<VkPipelineColorBlendAttachmentState, 2> blendAttachments{
		accumBlend, revealBlend
	};

	VkPipelineColorBlendStateCreateInfo colorBlending{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.attachmentCount = static_cast<uint32_t>(blendAttachments.size()),
		.pAttachments = blendAttachments.data(),
	};

	// Depth: test on, write off
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;


	std::array<VkDescriptorSetLayout, 2> setLayouts = {
		state->renderer->globalSetLayout,     // set = 0 (UBO)
		state->renderer->materialSetLayout    // set = 1 (textures)
	};

	VkPushConstantRange pushRange{};
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof(PushConstantBlock);

	VkPipelineLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	info.pSetLayouts = setLayouts.data();
	info.pushConstantRangeCount = 1;
	info.pPushConstantRanges = &pushRange;

	PANIC(
		vkCreatePipelineLayout(
			state->context->device,
			&info,
			nullptr,
			&state->renderer->transparencyPipelineLayout
		),
		"Failed to create transparency pipeline layout"
	);

	// Graphics pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shaderStages,
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depthStencil,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicState,
		// IMPORTANT: use a VALID layout. Reuse opaque layout for now.
		.layout = state->renderer->transparencyPipelineLayout,
		// IMPORTANT: use the correct render pass handle
		.renderPass = state->renderer->transparencyRenderPass,
		.subpass = 0,
	};

	PANIC(
		vkCreateGraphicsPipelines(state->context->device, VK_NULL_HANDLE, 1,
			&pipelineInfo, nullptr,
			&state->renderer->transparencyPipeline),
		"Failed To Create Transparency Graphics Pipeline"
	);
}
void transparencyPipelineDestroy(State* state) {
	vkDestroyPipelineLayout(state->context->device, state->renderer->transparencyPipelineLayout, nullptr);
	vkDestroyPipeline(state->context->device, state->renderer->transparencyPipeline, nullptr);
	vkDestroyShaderModule(state->context->device, state->renderer->transparentFragShaderModule, nullptr);
};

void presentPipelineCreate(State* state) {
	// Shader modules
	auto vertShaderCode = shaderRead("./res/shaders/present_vert.spv");
	auto fragShaderCode = shaderRead("./res/shaders/present_frag.spv");

	VkShaderModuleCreateInfo vertShaderModuleInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = vertShaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data()),
	};
	PANIC(vkCreateShaderModule(state->context->device,
		&vertShaderModuleInfo,
		nullptr,
		&state->renderer->presentVertShaderModule),
		"Failed To Create Present Vertex Shader Module");

	VkShaderModuleCreateInfo fragShaderModuleInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = fragShaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data()),
	};
	PANIC(vkCreateShaderModule(state->context->device,
		&fragShaderModuleInfo,
		nullptr,
		&state->renderer->presentFragShaderModule),
		"Failed To Create Present Fragment Shader Module");

	// Shader stages
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = state->renderer->presentVertShaderModule,
		.pName = "main",
	};
	VkPipelineShaderStageCreateInfo fragShaderStageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = state->renderer->presentFragShaderModule,
		.pName = "main",
	};
	VkPipelineShaderStageCreateInfo shaderStages[]{ vertShaderStageInfo, fragShaderStageInfo };

	// No vertex input (fullscreen triangle via gl_VertexIndex)
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 0,
		.pVertexBindingDescriptions = nullptr,
		.vertexAttributeDescriptionCount = 0,
		.pVertexAttributeDescriptions = nullptr,
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		.primitiveRestartEnable = VK_FALSE,
	};

	VkViewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)state->window.swapchain.imageExtent.width,
		.height = (float)state->window.swapchain.imageExtent.height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	VkRect2D scissor{
		.offset = { 0, 0 },
		.extent = state->window.swapchain.imageExtent,
	};
	VkPipelineViewportStateCreateInfo viewportState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor,
	};

	VkPipelineRasterizationStateCreateInfo rasterizer{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f,
	};

	VkPipelineMultisampleStateCreateInfo multisampling{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
	};

	VkPipelineColorBlendAttachmentState colorBlendAttachment{
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT,
	};
	VkPipelineColorBlendStateCreateInfo colorBlending{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
	};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
	.setLayoutCount = 1,
	.pSetLayouts = &state->renderer->presentSetLayout,
	.pushConstantRangeCount = 0,
	.pPushConstantRanges = nullptr,
	};
	PANIC(vkCreatePipelineLayout(state->context->device,
		&pipelineLayoutInfo,
		nullptr,
		&state->renderer->presentPipelineLayout),
		"Failed To Create Present Pipeline Layout");

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamicState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamicStates,
	};

	VkGraphicsPipelineCreateInfo pipelineInfo{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shaderStages,
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = nullptr,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicState,
		.layout = state->renderer->presentPipelineLayout,
		.renderPass = state->renderer->presentRenderPass,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
	};

	PANIC(vkCreateGraphicsPipelines(state->context->device,
		VK_NULL_HANDLE,
		1,
		&pipelineInfo,
		nullptr,
		&state->renderer->presentPipeline),
		"Failed To Create Present Graphics Pipeline");
}
void presentPipelineDestroy(State* state) {
	if (state->renderer->presentPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(state->context->device, state->renderer->presentPipeline, nullptr);
		state->renderer->presentPipeline = VK_NULL_HANDLE;
	}

	if (state->renderer->presentPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(state->context->device, state->renderer->presentPipelineLayout, nullptr);
		state->renderer->presentPipelineLayout = VK_NULL_HANDLE;
	}

	if (state->renderer->presentSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(state->context->device, state->renderer->presentSetLayout, nullptr);
		state->renderer->presentSetLayout = VK_NULL_HANDLE;
	}

	if (state->renderer->presentVertShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(state->context->device, state->renderer->presentVertShaderModule, nullptr);
		state->renderer->presentVertShaderModule = VK_NULL_HANDLE;
	}

	if (state->renderer->presentFragShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(state->context->device, state->renderer->presentFragShaderModule, nullptr);
		state->renderer->presentFragShaderModule = VK_NULL_HANDLE;
	}
}

//IBL Helper
void createComputeMipViews(State* state)
{
	VkImage image = state->texture->computeImage;
	VkFormat format = state->texture->specularFormat;
	uint32_t mipLevels = state->texture->specularMipLevels;

	// sanity
	assert(mipLevels > 0);

	state->renderer->computeMipViews.resize(mipLevels);

	for (uint32_t mip = 0; mip < mipLevels; ++mip)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewInfo.format = format;

		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = mip;   // per‑mip view
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 6;

		PANIC(
			vkCreateImageView(
				state->context->device,
				&viewInfo,
				nullptr,
				&state->renderer->computeMipViews[mip]),
			"Failed to create compute mip view");
	}
}

void iblPrefilterDispatch(State* state, uint32_t baseSize)
{
	VkDevice device = state->context->device;
	VkCommandPool pool = state->renderer->commandPool;
	VkImage image = state->texture->computeImage;
	VkFormat format = state->texture->specularFormat;
	uint32_t mipLevels = state->texture->specularMipLevels;

	assert(mipLevels > 0);

	// Allocate one-time command buffer
	VkCommandBufferAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = pool,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	VkCommandBuffer cmd;
	vkAllocateCommandBuffers(device, &allocInfo, &cmd);

	VkCommandBufferBeginInfo beginInfo{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(cmd, &beginInfo);

	// computeImage: UNDEFINED → GENERAL (for STORAGE_IMAGE writes)
	transitionImageLayout(
		state,
		image,
		format,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		mipLevels,
		6
	);

	// Bind compute pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, state->renderer->iblPipeline);

	for (uint32_t mip = 0; mip < mipLevels; ++mip)
	{
		uint32_t mipSize = baseSize >> mip;
		float roughness = (mipLevels > 1) ? float(mip) / float(mipLevels - 1) : 0.0f;

		// Descriptor for this mip's storage image
		VkDescriptorImageInfo specInfo{};
		specInfo.sampler = VK_NULL_HANDLE;
		specInfo.imageView = state->renderer->computeMipViews[mip];
		specInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // ← stays GENERAL

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = state->renderer->iblSets[mip];   // per-mip set
		write.dstBinding = 1;                           // storage image
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		write.pImageInfo = &specInfo;

		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

		// Bind this mip's set
		vkCmdBindDescriptorSets(
			cmd,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			state->renderer->iblPipelineLayout,
			0,
			1,
			&state->renderer->iblSets[mip],
			0,
			nullptr
		);

		for (uint32_t face = 0; face < 6; ++face)
		{
			struct Push {
				int face;
				int mip;
				float roughness;
				int mipResolution;
			} pc;

			pc.face = int(face);
			pc.mip = int(mip);
			pc.roughness = roughness;
			pc.mipResolution = int(mipSize);

			vkCmdPushConstants(
				cmd,
				state->renderer->iblPipelineLayout,
				VK_SHADER_STAGE_COMPUTE_BIT,
				0,
				sizeof(Push),
				&pc
			);

			uint32_t groups = (mipSize + 7) / 8;
			vkCmdDispatch(cmd, groups, groups, 1);
		}
	}

	// IMPORTANT: leave image in GENERAL
	// No GENERAL → SHADER_READ_ONLY_OPTIMAL transition here

	vkEndCommandBuffer(cmd);

	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &cmd
	};

	vkQueueSubmit(state->context->queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(state->context->queue);

	vkFreeCommandBuffers(device, pool, 1, &cmd);
}
