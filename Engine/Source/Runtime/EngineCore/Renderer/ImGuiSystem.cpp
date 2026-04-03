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

	// Enable viewports for docking/floating windows
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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
	
	// Setup PipelineInfoForViewports for secondary viewports (docking/multiviewport)
	init_info.PipelineInfoForViewports = init_info.PipelineInfoMain;
	
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

	// Create sampler for viewport images
	vk::SamplerCreateInfo samplerInfo;
	samplerInfo.magFilter = vk::Filter::eLinear;
	samplerInfo.minFilter = vk::Filter::eLinear;
	samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.borderColor = vk::BorderColor::eFloatTransparentBlack;
	
	viewportSampler = vk::raii::Sampler(renderer->GetVulkanInstance()->GetLogicalDevice(), samplerInfo);

	initialized = true;
	return true;
}

void ImGuiSystem::Cleanup()
{
	if (!initialized) {
		return;
	}

	renderer->GetVulkanInstance()->GetLogicalDevice().waitIdle();

	viewportDescriptors.clear();
	viewportSampler = nullptr;

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();

	initialized = false;
}

void ImGuiSystem::SetupViewportDescriptor(VkDescriptorSet descriptor)
{
	if (viewportDescriptors.empty()) {
		viewportDescriptors.push_back(descriptor);
	} else {
		viewportDescriptors[0] = descriptor;
	}
}

void ImGuiSystem::CleanupViewport()
{
	viewportDescriptors.clear();
}

void ImGuiSystem::NewFrame()
{
	if (!initialized) {
		return;
	}

	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Create full-screen dockspace
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("EditorDockSpace", nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
	ImGui::PopStyleVar(3);

	ImGuiID dockId = ImGui::GetID("EditorDock");
	ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	ImGui::End();

	// Create Viewport window
	ImGui::SetNextWindowDockID(dockId, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Viewport")) {
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImTextureID texId = reinterpret_cast<ImTextureID>(viewportDescriptors.empty() ? VK_NULL_HANDLE : viewportDescriptors[0]);
		ImGui::Image(texId, avail);
	}
	ImGui::End();

	// Scene Hierarchy panel
	ImGui::SetNextWindowDockID(dockId, ImGuiCond_FirstUseEver);
	ImGui::Begin("Scene Hierarchy");
	ImGui::Text("No objects in scene");
	ImGui::End();

	// Properties panel
	ImGui::SetNextWindowDockID(dockId, ImGuiCond_FirstUseEver);
	ImGui::Begin("Properties");
	ImGui::Text("No selection");
	ImGui::End();

	// Console panel
	ImGui::SetNextWindowDockID(dockId, ImGuiCond_FirstUseEver);
	ImGui::Begin("Console");
	ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Engine initialized...");
	ImGui::End();

	// Resources panel
	ImGui::SetNextWindowDockID(dockId, ImGuiCond_FirstUseEver);
	ImGui::Begin("Resources");
	ImGui::Text("No resources loaded");
	ImGui::End();

	// Menu bar
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Scene", "Ctrl+N")) {}
			if (ImGui::MenuItem("Open Scene", "Ctrl+O")) {}
			if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {}
			if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit", "Alt+F4")) {}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem("Undo", "Ctrl+Z")) {}
			if (ImGui::MenuItem("Redo", "Ctrl+Y")) {}
			ImGui::Separator();
			if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
			if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
			if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
			if (ImGui::MenuItem("Delete", "Del")) {}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View")) {
			if (ImGui::MenuItem("Viewport", "Ctrl+1")) {}
			if (ImGui::MenuItem("Scene Hierarchy", "Ctrl+2")) {}
			if (ImGui::MenuItem("Properties", "Ctrl+3")) {}
			if (ImGui::MenuItem("Console", "Ctrl+4")) {}
			if (ImGui::MenuItem("Resources", "Ctrl+5")) {}
			ImGui::Separator();
			if (ImGui::MenuItem("Toggle Fullscreen", "F11")) {}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Game")) {
			if (ImGui::MenuItem("Play", "F5")) {}
			if (ImGui::MenuItem("Pause", "F6")) {}
			if (ImGui::MenuItem("Stop", "F7")) {}
			ImGui::Separator();
			if (ImGui::MenuItem("Simulate", "F8")) {}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("Documentation")) {}
			if (ImGui::MenuItem("About")) {}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void ImGuiSystem::Render(vk::raii::CommandBuffer& commandBuffer, uint32_t frameIndex)
{
	if (!initialized) {
		return;
	}

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffer);

	// Update and Render additional Platform Windows (for multi-viewport/floating windows)
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
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