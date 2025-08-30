#pragma once
#include "PipelineManager.h"
#include <array>
#include <stdexcept>
#include <iostream>
#include "SwapChain.h"
#include "ResourceManager.h"
#include "Device.h"

PipelineManager::PipelineManager(Device* device, ResourceManager* resourceManager, SwapChain* swapChain) : m_Device(device),
m_ResourceManager(resourceManager),
m_SwapChain(swapChain)
{
    LoadPipelineCache();

    CreateUniversalDescriptorSetLayout();
    CreateGBufferDescriptorSetLayout();
    CreateDepthPrepassDescriptorSetLayout();

    CreateDepthPrepassPipeline();
    CreateGBufferPipeline();
    CreateLightingDescriptorSetLayout();
    CreateLightingPipeline();
    CreateToneMappingDescriptorSetLayout();
    CreateToneMappingPipeline();

}

PipelineManager::~PipelineManager()
{
    vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_UniversalDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_GBufferDescriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_DepthPrepassDescriptorSetLayout, nullptr);

    SavePipelineCache();
	vkDestroyPipelineCache(m_Device->GetDevice(), m_PipelineCache, nullptr);

	// depth prepass pipeline cleanup
    vkDestroyPipeline(m_Device->GetDevice(), m_DepthPrepassPipeline, nullptr);
    vkDestroyPipelineLayout(m_Device->GetDevice(), m_DepthPrepassPipelineLayout, nullptr);
    // gBugger pipeline cleanup
    vkDestroyPipeline(m_Device->GetDevice(), m_GBufferPipeline, nullptr);
    vkDestroyPipelineLayout(m_Device->GetDevice(), m_GBufferPipelineLayout, nullptr);

    vkDestroyPipeline(m_Device->GetDevice(), m_LightingPipeline, nullptr);
    vkDestroyPipelineLayout(m_Device->GetDevice(), m_LightingPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_LightingDescriptorSetLayout, nullptr);

    vkDestroyPipeline(m_Device->GetDevice(), m_ToneMappingPipeline, nullptr);
    vkDestroyPipelineLayout(m_Device->GetDevice(), m_ToneMappingPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_Device->GetDevice(), m_ToneMappingDescriptorSetLayout, nullptr);

}

void PipelineManager::CreateDepthPrepassPipeline()
{
	auto vertShaderCode = readFile("CustomShaders/depthPrepass.vert.spv");

	VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	auto fragShaderCode = readFile("CustomShaders/depthPrepass.frag.spv");
	VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo  colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 0; // No color attachments in depth prepass
	colorBlending.pAttachments = nullptr;

	VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(PushConstantData);

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

    std::array<VkDescriptorSetLayout, 2> depthSetLayouts = {
    m_UniversalDescriptorSetLayout,
    m_DepthPrepassDescriptorSetLayout
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(depthSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = depthSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(m_Device->GetDevice(), &pipelineLayoutInfo, nullptr, &m_DepthPrepassPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

    VkFormat depthFormat = m_ResourceManager->FindDepthFormat();

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	pipelineRenderingCreateInfo.colorAttachmentCount = 0; // no colors
	pipelineRenderingCreateInfo.pColorAttachmentFormats = nullptr;
	pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &pipelineRenderingCreateInfo;
	pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = m_DepthPrepassPipelineLayout;
    pipelineInfo.renderPass = nullptr;
    pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(m_Device->GetDevice(), m_PipelineCache, 1, &pipelineInfo, nullptr, &m_DepthPrepassPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline");
	}

	vkDestroyShaderModule(m_Device->GetDevice(), fragShaderModule, nullptr);
	vkDestroyShaderModule(m_Device->GetDevice(), vertShaderModule, nullptr);
}

void PipelineManager::CreateGBufferPipeline()
{
    auto vertShaderCode = readFile("CustomShaders/gbuffer.vert.spv");
    auto fragShaderCode = readFile("CustomShaders/gbuffer.frag.spv");

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStage[] = { vertShaderStageInfo,fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE; //dont write depth (depth prepass did this)
    depthStencil.depthCompareOp = VK_COMPARE_OP_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    std::array<VkPipelineColorBlendAttachmentState, 3> colorBlendAttachments{};

    // Albedo attachment
    colorBlendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachments[0].blendEnable = VK_FALSE;

    // Normal attachment
    colorBlendAttachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    colorBlendAttachments[1].blendEnable = VK_FALSE;

    // pbr attachment
    colorBlendAttachments[2].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    colorBlendAttachments[2].blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
    colorBlending.pAttachments = colorBlendAttachments.data();

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstantData);

    std::array<VkDescriptorSetLayout, 2> gbufferSetLayouts = {
    m_UniversalDescriptorSetLayout,
    m_GBufferDescriptorSetLayout
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(gbufferSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = gbufferSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_Device->GetDevice(), &pipelineLayoutInfo, nullptr, &m_GBufferPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create G-Buffer pipeline layout!");
    }

    VkFormat colorFormats[] = {
        VK_FORMAT_R8G8B8A8_SRGB,     // Albedo
        VK_FORMAT_R8G8B8A8_UNORM,    // Normals
        VK_FORMAT_R8G8B8A8_SRGB      // pbr
    };

    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCreateInfo.colorAttachmentCount = 3;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = colorFormats;
    pipelineRenderingCreateInfo.depthAttachmentFormat = m_ResourceManager->FindDepthFormat();
    pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStage;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_GBufferPipelineLayout;
    pipelineInfo.renderPass = nullptr;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_Device->GetDevice(), m_PipelineCache, 1, &pipelineInfo, nullptr, &m_GBufferPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create G-Buffer pipeline!");
    }

    vkDestroyShaderModule(m_Device->GetDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(m_Device->GetDevice(), vertShaderModule, nullptr);
}

void PipelineManager::CreateLightingPipeline()
{
    auto vertShaderCode = readFile("CustomShaders/lighting.vert.spv");
    auto fragShaderCode = readFile("CustomShaders/lighting.frag.spv");

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    fragShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo shaderStage[] = { vertShaderStageInfo,fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE; //dont write depth (depth prepass did this)
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};

    // Albedo attachment
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    // Normal attachment

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_LightingDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(m_Device->GetDevice(), &pipelineLayoutInfo, nullptr, &m_LightingPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create Lighting pipeline layout!");
    }

    VkFormat swapChainFormat = VK_FORMAT_R32G32B32A32_SFLOAT;



    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChainFormat;
    pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStage;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_LightingPipelineLayout;
    pipelineInfo.renderPass = nullptr;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_Device->GetDevice(), m_PipelineCache, 1, &pipelineInfo, nullptr, &m_LightingPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create Lighting pipeline!");
    }

    vkDestroyShaderModule(m_Device->GetDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(m_Device->GetDevice(), vertShaderModule, nullptr);
}

void PipelineManager::CreateToneMappingPipeline()
{
    auto vertShaderCode = readFile("CustomShaders/tonemapping.vert.spv");
    auto fragShaderCode = readFile("CustomShaders/tonemapping.frag.spv");

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    fragShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo shaderStage[] = { vertShaderStageInfo,fragShaderStageInfo };


    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE; //dont write depth (depth prepass did this)
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};

    // hdr attachment
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(TonemappingPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_ToneMappingDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_Device->GetDevice(), &pipelineLayoutInfo, nullptr, &m_ToneMappingPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create Tonemapping pipeline layout!");
    }

    VkFormat swapChainFormat = m_SwapChain->GetSwapChainImageFormat();

    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChainFormat;
    pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &pipelineRenderingCreateInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStage;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_ToneMappingPipelineLayout;
    pipelineInfo.renderPass = nullptr;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(m_Device->GetDevice(), m_PipelineCache, 1, &pipelineInfo, nullptr, &m_ToneMappingPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create ToneMapping pipeline!");
    }

    vkDestroyShaderModule(m_Device->GetDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(m_Device->GetDevice(), vertShaderModule, nullptr);
}

void PipelineManager::CreateUniversalDescriptorSetLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> universalBindings;
    std::vector<VkDescriptorBindingFlags> universalBindingFlags;

    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    universalBindings.push_back(uboLayoutBinding);
    universalBindingFlags.push_back(0);

    VkDescriptorSetLayoutBinding vertexBufferLayoutBinding{};
    vertexBufferLayoutBinding.binding = 1;
    vertexBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexBufferLayoutBinding.descriptorCount = 1;
    vertexBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    vertexBufferLayoutBinding.pImmutableSamplers = nullptr;
    universalBindings.push_back(vertexBufferLayoutBinding);
    universalBindingFlags.push_back(0);

    VkDescriptorSetLayoutBinding materialBufferLayoutBinding{};
    materialBufferLayoutBinding.binding = 2;
    materialBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialBufferLayoutBinding.descriptorCount = 1;
    materialBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    materialBufferLayoutBinding.pImmutableSamplers = nullptr;
    universalBindings.push_back(materialBufferLayoutBinding);
    universalBindingFlags.push_back(0);

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = static_cast<uint32_t>(universalBindings.size());
    bindingFlagsInfo.pBindingFlags = universalBindingFlags.data();

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = &bindingFlagsInfo;
    layoutInfo.bindingCount = static_cast<uint32_t>(universalBindings.size());
    layoutInfo.pBindings = universalBindings.data();

    if (vkCreateDescriptorSetLayout(m_Device->GetDevice(), &layoutInfo, nullptr, &m_UniversalDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create universal descriptor set layout!");
    }
}
void PipelineManager::CreateGBufferDescriptorSetLayout()
{
    uint32_t textureAmount = 5000;
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = textureAmount;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorBindingFlags bindlessFlags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = 1;
    bindingFlagsInfo.pBindingFlags = &bindlessFlags;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = &bindingFlagsInfo;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(m_Device->GetDevice(), &layoutInfo, nullptr, &m_GBufferDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create GBuffer descriptor set layout!");
    }
}

void PipelineManager::CreateDepthPrepassDescriptorSetLayout()
{
    uint32_t textureAmount = 5000;
    VkDescriptorSetLayoutBinding alphaSamplerLayoutBinding{};
    alphaSamplerLayoutBinding.binding = 0;
    alphaSamplerLayoutBinding.descriptorCount = textureAmount;
    alphaSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    alphaSamplerLayoutBinding.pImmutableSamplers = nullptr;
    alphaSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorBindingFlags bindlessFlags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = 1;
    bindingFlagsInfo.pBindingFlags = &bindlessFlags;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = &bindingFlagsInfo;
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &alphaSamplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(m_Device->GetDevice(), &layoutInfo, nullptr, &m_DepthPrepassDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth prepass descriptor set layout!");
    }
}

void PipelineManager::CreateToneMappingDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 1> bindings = {};

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_Device->GetDevice(), &layoutInfo, nullptr, &m_ToneMappingDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create tonemapping descriptor set layout!");
    }
}

void PipelineManager::CreateLightingDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 6> bindings = {};

    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].pImmutableSamplers = nullptr;

    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[4].pImmutableSamplers = nullptr;

    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[5].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(m_Device->GetDevice(), &layoutInfo, nullptr, &m_LightingDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create lighting descriptor set layout!");
    }
}

void PipelineManager::LoadPipelineCache()
{
    std::vector<uint8_t> initialCacheData;

    VkPipelineCacheCreateInfo CacheCreateInfo{};
    CacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

    std::string readFileName = "pipeline_cache_data.bin";
    FILE* pReadFile = fopen(readFileName.c_str(), "rb");

    if (pReadFile) {
        fseek(pReadFile, 0, SEEK_END);
        long fileSize = ftell(pReadFile);
        rewind(pReadFile);

        initialCacheData.resize(fileSize);
        if (fread(initialCacheData.data(), 1, fileSize, pReadFile) != fileSize) {
            std::cerr << "Failed to read pipeline cache file" << std::endl;
            initialCacheData.clear();
        }

        fclose(pReadFile);
        printf(" Pipeline cache HIT!\n");
        printf(" cacheData loaded from %s\n", readFileName.c_str());
    }
    else
    {
        printf(" Pipeline cache miss!\n");
    }

    if (!initialCacheData.empty()) {
        VkPipelineCacheHeaderVersionOne* header =
            reinterpret_cast<VkPipelineCacheHeaderVersionOne*>(initialCacheData.data());

        bool isValideCache =
            header->headerSize == sizeof(VkPipelineCacheHeaderVersionOne) &&
            header->headerVersion == VK_PIPELINE_CACHE_HEADER_VERSION_ONE;

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(m_Device->GetPhysicalDevice(), &deviceProperties);

        isValideCache &= (header->vendorID == deviceProperties.vendorID);
        isValideCache &= (header->deviceID == deviceProperties.deviceID);

        if (!isValideCache) {
            std::cout << "existing pipeline cache is invalid. Creating new cache" << std::endl;
            initialCacheData.clear();
        }
    } if (vkCreatePipelineCache(m_Device->GetDevice(), &CacheCreateInfo, nullptr, &m_PipelineCache) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline cache!");
    }

    CacheCreateInfo.initialDataSize = initialCacheData.size();
    CacheCreateInfo.pInitialData = initialCacheData.empty() ? nullptr : initialCacheData.data();

    if (vkCreatePipelineCache(m_Device->GetDevice(), &CacheCreateInfo, nullptr, &m_PipelineCache) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline cache!");
    }
}

void PipelineManager::SavePipelineCache()
{
    size_t cacheSize;
    VkResult result = vkGetPipelineCacheData(m_Device->GetDevice(), m_PipelineCache, &cacheSize, nullptr);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to query pipeline cache size: " << result << std::endl;
        return;
    }

    std::vector<uint8_t> cacheData(cacheSize);
    result = vkGetPipelineCacheData(m_Device->GetDevice(), m_PipelineCache, &cacheSize, cacheData.data());

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to retrieve pipeline cache data: " << result << std::endl;
        return;
    }
    const std::string cacheFileName = "pipeline_cache_data.bin";
    FILE* outputFile = fopen(cacheFileName.c_str(), "wb");
    if (!outputFile) {
        std::cerr << "Failed to open pipeline cache file for writing: " << cacheFileName << std::endl;
        return;
    }
    size_t written = fwrite(cacheData.data(), 1, cacheSize, outputFile);

    fclose(outputFile);

    if (written != cacheSize) {
        std::cerr << "Failed to write complete pipeline cache. " << "Wrote " << written << " of " << cacheSize << " bytes." << std::endl;
    }
    else {
        std::cout << "Successfully saved pipeline cache to " << cacheFileName << " (" << cacheSize << " bytes)" << std::endl;
    }
}

VkShaderModule PipelineManager::CreateShaderModule(const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);  // Convert to bytes
    createInfo.pCode = code.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_Device->GetDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}