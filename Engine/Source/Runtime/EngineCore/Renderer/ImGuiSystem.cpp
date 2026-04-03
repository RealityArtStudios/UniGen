// Copyright (c) CreationArtStudios

#include "ImGuiSystem.h"
#include "Renderer.h"
#include "../Window.h"
#include "SwapChain.h"
#include <iostream>

ImGuiSystem::ImGuiSystem()
{
}

ImGuiSystem::~ImGuiSystem()
{
	Cleanup();
}

bool ImGuiSystem::Initialize(Renderer* renderer, Window* window)
{
	if (initialized) {
		return true;
	}

	this->renderer = renderer;

	ImGui::CreateContext();

	ImGui_ImplGlfw_InitForVulkan(window->getGLFWwindow(), true);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = *renderer->GetVulkanInstance()->GetInstance();
	init_info.PhysicalDevice = *renderer->GetVulkanInstance()->GetPhysicalDevice();
	init_info.Device = *renderer->GetVulkanInstance()->GetLogicalDevice();
	init_info.QueueFamily = renderer->GetVulkanInstance()->GetQueueFamilyIndex();
	init_info.Queue = *renderer->GetVulkanInstance()->GetGraphicsQueue();
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = VK_NULL_HANDLE;
	init_info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	
	// Pipeline settings in PipelineInfoMain
	init_info.PipelineInfoMain.Subpass = 0;
	init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	
	// Dynamic rendering setup
	init_info.UseDynamicRendering = true;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	// Use swapchain's format - must match what swapchain actually uses (SRGB per SwapChain.cpp)
	// surface query returns 44 (UNORM) but actual swapchain uses 50 (SRGB)
	VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
	
	// Tell ImGui to create its main pipeline after initialization so formats match
	init_info.CheckVkResultFn = [](VkResult err) {
		if (err != VK_SUCCESS) {
			std::cerr << "[ImGui Vulkan] Error: " << err << std::endl;
		}
	};

	if (!ImGui_ImplVulkan_Init(&init_info)) {
		std::cerr << "Failed to initialize ImGui Vulkan backend" << std::endl;
		return false;
	}

	initialized = true;
	return true;
}

void ImGuiSystem::Cleanup()
{
	if (!initialized) {
		return;
	}

	renderer->GetVulkanInstance()->GetLogicalDevice().waitIdle();

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();

	initialized = false;
}

void ImGuiSystem::NewFrame()
{
	if (!initialized) {
		return;
	}

	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::ShowDemoWindow();
}

void ImGuiSystem::Render(vk::raii::CommandBuffer& commandBuffer, uint32_t frameIndex)
{
	if (!initialized) {
		return;
	}

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffer);
}

void ImGuiSystem::HandleResize(uint32_t width, uint32_t height)
{
	if (!initialized) {
		return;
	}
}

bool ImGuiSystem::WantCaptureKeyboard() const
{
	if (!initialized) {
		return false;
	}
	return ImGui::GetIO().WantCaptureKeyboard;
}

bool ImGuiSystem::WantCaptureMouse() const
{
	if (!initialized) {
		return false;
	}
	return ImGui::GetIO().WantCaptureMouse;
}