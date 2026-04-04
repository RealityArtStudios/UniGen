// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once
#include <memory>

//TODO: Will move this to precompiled header in the future
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <fstream>
#include <ranges>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "VulkanInstance/VulkanInstance.h"
#include "SwapChain.h"
#include "Buffer.h"
#include "TextureManager.h"
#include "Mesh.h"
#include "PipelineManager.h"
#include "CommandPool.h"
#include "CommandBufferManager.h"
#include "DescriptorManager.h"
#include "ImGuiSystem.h"

#ifndef STB_IMAGE_STATIC
#define STB_IMAGE_STATIC
#endif
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb_image.h>

#include "Runtime/EngineCore/ECS/ECS.h"
#include "Runtime/EngineCore/ECS/Components.h"
#include "Runtime/EngineCore/ECS/Scene.h"

class Window;

class Renderer
{
public:
	Renderer(Window* InWindow);
	virtual ~Renderer();

	void Initialize();
	void Render();
	void Shutdown();
	VulkanInstance* GetVulkanInstance() const { return VulkanInstanceWrapper.get(); }
	SwapChain* GetSwapChain() const { return SwapChainWrapper.get(); }
	uint32_t GetCurrentFrameIndex() const { return frameIndex; }
	ImGuiSystem* GetImGuiSystem() const { return ImGuiSystemWrapper.get(); }

	EngineScene& GetScene() { return SceneWrapper; }
	Mesh& GetMesh() { return *MeshData; }
	vk::raii::Buffer& GetVertexBuffer() { return VulkanVertexBuffer; }
	vk::raii::Buffer& GetIndexBuffer() { return VulkanIndexBuffer; }

	void ReloadSceneData();

	vk::raii::ImageView& GetRenderTargetImageView() { return renderTargetImageView; }
	vk::raii::Sampler& GetRenderTargetSampler() { return renderTargetSampler; }
protected:
	void CreateDepthResources();
	void CreateImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling,
		vk::ImageUsageFlags usage,
		vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);
	void UpdateUniformBuffer(uint32_t currentImage);
	void CreateSyncObjects();
	void recordCommandBuffer(uint32_t imageIndex);
	void CreateRenderTarget();
	void transition_image_layout(vk::Image               image, vk::ImageLayout old_layout, vk::ImageLayout new_layout,
		vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
		vk::PipelineStageFlags2 src_stage_mask, vk::PipelineStageFlags2 dst_stage_mask, vk::ImageAspectFlags    image_aspect_flags);

	// Depth Buffering (3D)
	vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
	{
		for (const auto format : candidates) {
			vk::FormatProperties props = VulkanInstanceWrapper->GetPhysicalDevice().getFormatProperties(format);

			if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
				return format;
			}
			if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}
protected:
	//Vulkan
	std::unique_ptr<VulkanInstance> VulkanInstanceWrapper;
	std::unique_ptr<SwapChain> SwapChainWrapper;
	std::unique_ptr<CommandPool> CommandPoolWrapper;
	std::unique_ptr<CommandBufferManager> CommandBufferManagerWrapper;
	std::unique_ptr<DescriptorManager> DescriptorManagerWrapper;
	std::unique_ptr<BufferManager> BufferManagerWrapper;
	std::unique_ptr<TextureManager> TextureManagerWrapper;
	std::unique_ptr<PipelineManager> PipelineManagerWrapper;
	std::unique_ptr<ImGuiSystem> ImGuiSystemWrapper;
	std::unique_ptr<Mesh> MeshData;
	EngineScene SceneWrapper;
	std::vector < vk::raii::Semaphore> VulkanPresentCompleteSemaphores;
	std::vector < vk::raii::Semaphore> VulkanRenderFinishedSemaphores;
	vk::raii::Fence     VulkanDrawFence = nullptr;
	vk::raii::Buffer VulkanVertexBuffer = nullptr;
	vk::raii::DeviceMemory VulkanVertexBufferMemory = nullptr;
	vk::raii::Buffer VulkanIndexBuffer = nullptr;
	vk::raii::DeviceMemory VulkanIndexBufferMemory = nullptr;
	std::vector<vk::raii::Fence>     inFlightFences;
	uint32_t                         frameIndex = 0;

	std::vector<const char*> VulkanRequiredDeviceExtension = { vk::KHRSwapchainExtensionName,
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName };

	vk::raii::Image depthImage = nullptr;
	vk::raii::DeviceMemory depthImageImageMemory = nullptr;
	vk::raii::ImageView depthImageView = nullptr;

	vk::raii::Image renderTargetImage = nullptr;
	vk::raii::DeviceMemory renderTargetImageMemory = nullptr;
	vk::raii::ImageView renderTargetImageView = nullptr;
	vk::raii::Sampler renderTargetSampler = nullptr;
private:

	Window* RendererWindow = nullptr;
};

