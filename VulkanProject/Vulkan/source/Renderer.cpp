#include "Renderer.h"
#include "Device.h"
#include "PipelineManager.h"
#include "SwapChain.h"
#include "CommandManager.h"
#include "ResourceManager.h"
#include "Instance.h"

#include <array>
#include <chrono>
#include <GLFW/glfw3.h>
#include <algorithm>
#include "../../Window/InputManager.h"
#include "../../Window/CameraManager.h"


Renderer::Renderer(Device* device, PipelineManager* pipelineManager,
                    SwapChain* swapChain, CommandManager* commandManager,
                    ResourceManager* resourceManager,Instance* instance):
	m_Device(device),
	m_PipelineManager(pipelineManager),
	m_SwapChain(swapChain),
	m_CommandManager(commandManager),
	m_ResourceManager(resourceManager),
	m_Instance(instance)
{
    glfwSetFramebufferSizeCallback(m_Instance->GetWindow(), framebufferResizeCallback);
}

Renderer::~Renderer()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(m_Device->GetDevice(), m_ImageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(m_Device->GetDevice(), m_RenderFinishedSemaphores[i], nullptr);
        vkDestroyFence(m_Device->GetDevice(), m_InFlightFences[i], nullptr);
    }

}

void Renderer::UpdatePushConstants(VkCommandBuffer commandBuffer)
{
}

float Renderer::UpdateUniformBuffer(uint32_t currentImage)
{
    // Calculate delta time
    float currentTime = glfwGetTime();
    float deltaTime = currentTime - m_LastFrameTime;
    m_LastFrameTime = currentTime;

    // Update input
    InputManager::Instance().Update(deltaTime);

    UniformBufferObject ubo{};

    if (m_Camera) {
        // Use camera for view matrix and position
        ubo.view = m_Camera->GetViewMatrix();
        ubo.CameraManagerPosition = m_Camera->GetPosition();
    }
    else {
        // Fallback to your original hardcoded camera
        glm::vec3 cameraPosition = glm::vec3(-4.f, 1.5f, -0.3f);
        glm::vec3 target = cameraPosition + glm::vec3(1.f, 0.f, -0.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        ubo.view = glm::lookAt(cameraPosition, target, up);
        ubo.CameraManagerPosition = cameraPosition;
    }

    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        m_SwapChain->GetSwapChainExtent().width / (float)m_SwapChain->GetSwapChainExtent().height,
        0.1f,
        50.0f
    );

    ubo.resolution = glm::ivec2(m_SwapChain->GetSwapChainExtent().width, m_SwapChain->GetSwapChainExtent().height);
    ubo.proj[1][1] *= -1; // Flip Y-axis for Vulkan coordinate system

    memcpy(m_ResourceManager->GetUniformBuffersMapped()[currentImage], &ubo, sizeof(ubo));

    return deltaTime;
}
void Renderer::DrawFrame()
{

    vkWaitForFences(m_Device->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_Device->GetDevice(), m_SwapChain->GetSwapChain(), UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(m_Device->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame]);
    vkResetCommandBuffer(m_CommandManager->GetCommandBuffers()[m_CurrentFrame], 0);

    float deltaTime = UpdateUniformBuffer(m_CurrentFrame);

    RecordDeferredCommandBuffer(m_CommandManager->GetCommandBuffers()[m_CurrentFrame], imageIndex, deltaTime);

	VkCommandBufferSubmitInfo cmdBufferSubmitInfo{};
	cmdBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdBufferSubmitInfo.commandBuffer = m_CommandManager->GetCommandBuffers()[m_CurrentFrame];
	cmdBufferSubmitInfo.deviceMask = 0;

	VkSemaphoreSubmitInfo waitSemaphoreSubmitInfo{};
	waitSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphoreSubmitInfo.semaphore = m_ImageAvailableSemaphores[m_CurrentFrame];
	waitSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	waitSemaphoreSubmitInfo.deviceIndex = 0;
	waitSemaphoreSubmitInfo.value = 0;

	VkSemaphoreSubmitInfo signalSemaphoreSubmitInfo{};
	signalSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreSubmitInfo.semaphore = m_RenderFinishedSemaphores[m_CurrentFrame];
	signalSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	signalSemaphoreSubmitInfo.deviceIndex = 0;
	signalSemaphoreSubmitInfo.value = 0;

	VkSubmitInfo2 submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.flags = 0;
	submitInfo.waitSemaphoreInfoCount = 1;
	submitInfo.pWaitSemaphoreInfos = &waitSemaphoreSubmitInfo;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &cmdBufferSubmitInfo;
	submitInfo.signalSemaphoreInfoCount = 1;
	submitInfo.pSignalSemaphoreInfos = &signalSemaphoreSubmitInfo;

    if (vkQueueSubmit2(m_Device->GetGraphicsQueue(), 1, &submitInfo,
        m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    
    
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_RenderFinishedSemaphores[m_CurrentFrame];
    VkSwapchainKHR swapChains[] = { m_SwapChain->GetSwapChain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(m_Device->GetPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized) {
        m_FramebufferResized = false;
        RecreateSwapChain();
    }
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::CreateSyncObjects()
{
    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

        if (vkCreateSemaphore(m_Device->GetDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_Device->GetDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_Device->GetDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create semaphores");
        }
    }

}
void Renderer::RecreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_Instance->GetWindow(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_Instance->GetWindow(), &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(m_Device->GetDevice());

    m_SwapChain->CleanupSwapchain();


    m_SwapChain->CreateSwapChain();
    m_SwapChain->CreateImageViews();
    m_ResourceManager->RecreateResources(m_SwapChain,m_PipelineManager);

}

void Renderer::RenderDepthPrepass(VkCommandBuffer commandBuffer)
{

    // Depth-only attachment
    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = m_ResourceManager->GetDepthImageView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.clearValue.depthStencil = { 1.0f, 0 };
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = { {0, 0}, m_SwapChain->GetSwapChainExtent() };
    renderInfo.layerCount = 1;
    renderInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(commandBuffer, &renderInfo);

    // Set viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_SwapChain->GetSwapChainExtent().width);
    viewport.height = static_cast<float>(m_SwapChain->GetSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_SwapChain->GetSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Bind depth prepass pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_PipelineManager->GetPipeline("DepthPrepass").pipeline);

    // Draw all objects
    vkCmdBindIndexBuffer(commandBuffer, m_ResourceManager->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    const auto& meshes = m_ResourceManager->GetMeshes();
    const auto& pushConstants = m_ResourceManager->GetPushConstants();

    for (size_t i = 0; i < meshes.size(); ++i) {
        const auto& mesh = meshes[i];

        if (i < pushConstants.size()) {
            vkCmdPushConstants(
                commandBuffer,
                m_PipelineManager->GetDepthPrepassPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(PushConstantData),
                &pushConstants[i]
            );
        }

        auto universalDescriptors = m_ResourceManager->GetUniversalDescriptorSet(m_CurrentFrame);
        auto depthDescriptors = m_ResourceManager->GetDepthPrepassDescriptorSet(m_CurrentFrame);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_PipelineManager->GetDepthPrepassPipelineLayout(), 0, 1,
            &universalDescriptors, 0, nullptr);

        if (m_ResourceManager->HasAlphaTextures()) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_PipelineManager->GetDepthPrepassPipelineLayout(), 1, 1,
                &depthDescriptors, 0, nullptr);
        }

        vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
    }

    vkCmdEndRendering(commandBuffer);
}

void Renderer::RenderGBufferPass(VkCommandBuffer commandBuffer)
{

    std::array<VkRenderingAttachmentInfo, 3> colorAttachments = {};

    colorAttachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachments[0].imageView = m_ResourceManager->GetGBuffer().albedoImageView;
    colorAttachments[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachments[0].clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    colorAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachments[1] = colorAttachments[0];
    colorAttachments[1].imageView = m_ResourceManager->GetGBuffer().normalImageView;

    colorAttachments[2] = colorAttachments[0];
    colorAttachments[2].imageView = m_ResourceManager->GetGBuffer().pbrImageView;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = m_ResourceManager->GetDepthImageView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = { {0,0},m_SwapChain->GetSwapChainExtent() };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 2;
    renderInfo.colorAttachmentCount = colorAttachments.size();
    renderInfo.pColorAttachments = colorAttachments.data();
    renderInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(commandBuffer, &renderInfo);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_SwapChain->GetSwapChainExtent().width);
    viewport.height = static_cast<float>(m_SwapChain->GetSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_SwapChain->GetSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_PipelineManager->GetGBufferPipeline());

    vkCmdBindIndexBuffer(commandBuffer, m_ResourceManager->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    const auto& meshes = m_ResourceManager->GetMeshes();
    const auto& pushConstants = m_ResourceManager->GetPushConstants();

    for (size_t i = 0; i < meshes.size(); ++i) {
        const auto& mesh = meshes[i];

        if (i < pushConstants.size()) {
            vkCmdPushConstants(
                commandBuffer,
                m_PipelineManager->GetGBufferPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(PushConstantData),
                &pushConstants[i]
            );
        }

        auto universalDescriptors = m_ResourceManager->GetUniversalDescriptorSet(m_CurrentFrame);
        auto gBufferDescriptors = m_ResourceManager->GetGBufferDescriptorSet(m_CurrentFrame);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_PipelineManager->GetGBufferPipelineLayout(), 0, 1,
            &universalDescriptors, 0, nullptr);

        // Bind GBuffer-specific descriptor set (Set 1)
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_PipelineManager->GetGBufferPipelineLayout(), 1, 1,
            &gBufferDescriptors, 0, nullptr);

        vkCmdDrawIndexed(commandBuffer, mesh.indexCount, 1, mesh.indexOffset, 0, 0);
    }

    vkCmdEndRendering(commandBuffer);

}

void Renderer::RenderLightingPass(VkCommandBuffer commandBuffer)
{
    VkRenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachmentInfo.imageView = m_ResourceManager->GetHdrBuffer().imageView;
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    auto renderArea = VkRect2D{ VkOffset2D{},m_SwapChain->GetSwapChainExtent() };
    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachmentInfo;
    renderInfo.pDepthAttachment = nullptr;
    renderInfo.pStencilAttachment = nullptr;
    renderInfo.renderArea = renderArea;

    vkCmdBeginRendering(commandBuffer, &renderInfo);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_SwapChain->GetSwapChainExtent().width);
    viewport.height = static_cast<float>(m_SwapChain->GetSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_SwapChain->GetSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineManager->GetLightingPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_PipelineManager->GetLightingPipelineLayout(), 0, 1,
        &m_ResourceManager->GetLightingDescriptorSet(), 0, nullptr);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRendering(commandBuffer);
}

void Renderer::RenderToneMapping(VkCommandBuffer commandBuffer, uint32_t imageIndex,float deltaTime)
{

    VkRenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachmentInfo.imageView = m_SwapChain->GetSwapChainImageViews()[imageIndex];
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    auto renderArea = VkRect2D{ VkOffset2D{},m_SwapChain->GetSwapChainExtent() };
    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachmentInfo;
    renderInfo.pDepthAttachment = nullptr;
    renderInfo.pStencilAttachment = nullptr;
    renderInfo.renderArea = renderArea;

    vkCmdBeginRendering(commandBuffer, &renderInfo);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_SwapChain->GetSwapChainExtent().width);
    viewport.height = static_cast<float>(m_SwapChain->GetSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = m_SwapChain->GetSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    TonemappingPushConstants pushConstants;
    pushConstants.averageLuminance = 1.4;
    pushConstants.exposureMode = 1;

    vkCmdPushConstants(commandBuffer,
        m_PipelineManager->GetToneMappingPipelineLayout(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(TonemappingPushConstants),
        &pushConstants);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineManager->GetToneMappingPipeline());
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_PipelineManager->GetToneMappingPipelineLayout(), 0, 1,
        &m_ResourceManager->GetToneMappingDescriptorSet(), 0, nullptr);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRendering(commandBuffer);
}

void Renderer::RecordDeferredCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, float deltaTime)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    m_ResourceManager->TransitionImageLayout(m_ResourceManager->GetDepthImage(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                             VK_PIPELINE_STAGE_2_NONE,
                                             VK_ACCESS_2_NONE,
                                             VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
		                                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    );

    RenderDepthPrepass(commandBuffer);

    m_ResourceManager->TransitionImageLayoutInline(
        commandBuffer,
        m_ResourceManager->GetGBuffer().albedo,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    m_ResourceManager->TransitionImageLayoutInline(
        commandBuffer,
        m_ResourceManager->GetGBuffer().normal,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    m_ResourceManager->TransitionImageLayoutInline(
        commandBuffer,
        m_ResourceManager->GetGBuffer().pbr,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    RenderGBufferPass(commandBuffer);

    m_ResourceManager->TransitionImageLayoutInline(
        commandBuffer,
        m_ResourceManager->GetGBuffer().albedo,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    m_ResourceManager->TransitionImageLayoutInline(
        commandBuffer,
        m_ResourceManager->GetGBuffer().normal,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    m_ResourceManager->TransitionImageLayoutInline(
        commandBuffer,
        m_ResourceManager->GetGBuffer().pbr,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    m_ResourceManager->TransitionImageLayoutInline(
        commandBuffer,
        m_ResourceManager->GetDepthImage(),
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    m_ResourceManager->TransitionImageLayoutInline(
        commandBuffer,
        m_ResourceManager->GetHdrBuffer().image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );


    RenderLightingPass(commandBuffer);

    m_ResourceManager->TransitionImageLayoutInline(
        commandBuffer,
        m_ResourceManager->GetHdrBuffer().image,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        VK_ACCESS_2_SHADER_READ_BIT
    );

    m_ResourceManager->TransitionImageLayout(*m_SwapChain->GetSwapChainImages()[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_NONE,
        VK_ACCESS_2_NONE,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    RenderToneMapping(commandBuffer, imageIndex,deltaTime);

    m_ResourceManager->TransitionImageLayout(*m_SwapChain->GetSwapChainImages()[imageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_2_NONE,
        VK_ACCESS_2_NONE
    );

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}