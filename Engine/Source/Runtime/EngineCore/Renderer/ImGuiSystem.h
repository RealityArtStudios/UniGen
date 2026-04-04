// Copyright (c) CreationArtStudios

#pragma once
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

class Renderer;
class Window;

class ContentBrowserPanel;

class ImGuiSystem
{
public:
	ImGuiSystem();
	~ImGuiSystem();

	bool Initialize(Renderer* renderer, Window* window);
	void Cleanup();
	void NewFrame();
	void Render(vk::raii::CommandBuffer& commandBuffer, uint32_t frameIndex);

	void HandleResize(uint32_t width, uint32_t height);

	bool WantCaptureKeyboard() const;
	bool WantCaptureMouse() const;

	void SetupViewportDescriptor(VkDescriptorSet descriptor);
	void CleanupViewport();
	void SetContentBrowser(std::unique_ptr<ContentBrowserPanel> panel);
	void RefreshContentBrowserIcons();

private:
	ImGuiContext* context = nullptr;
	Renderer* renderer = nullptr;
	vk::raii::DescriptorPool descriptorPool = nullptr;
	bool initialized = false;

	std::vector<VkDescriptorSet> viewportDescriptors;
	vk::raii::Sampler viewportSampler = nullptr;

	std::unique_ptr<ContentBrowserPanel> contentBrowser;
};