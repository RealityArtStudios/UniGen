// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once
#include <memory>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <ranges>
#include <filesystem>

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
#include "Material.h"
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

#include <atomic>
#include <array>

class Window;

// ─────────────────────────────────────────────────────────────────────────────
struct FrameStats
{
	std::atomic<float>    frameTimeMs  = 0.0f;
	std::atomic<float>    fps          = 0.0f;
	std::atomic<float>    gpuTimeMs    = 0.0f;
	std::atomic<uint64_t> frameCount   = 0;

	std::atomic<float> cpuWaitGpuMs = 0.0f;
	std::atomic<float> cpuAcquireMs = 0.0f;
	std::atomic<float> cpuRecordMs  = 0.0f;
	std::atomic<float> cpuSubmitMs  = 0.0f;
	std::atomic<float> cpuPresentMs = 0.0f;

	std::atomic<float> gameThreadMs   = 0.0f;
	std::atomic<float> renderThreadMs = 0.0f;

	std::atomic<uint64_t> gpuMemoryUsed   = 0;
	std::atomic<uint64_t> gpuMemoryBudget = 0;
	std::atomic<uint64_t> systemMemoryUsed = 0;

	float    GetFrameTime()  const { return frameTimeMs.load(); }
	float    GetFPS()        const { return fps.load(); }
	float    GetGPUTime()    const { return gpuTimeMs.load(); }
	uint64_t GetFrameCount() const { return frameCount.load(); }

	float GetCPUWaitGpu() const { return cpuWaitGpuMs.load(); }
	float GetCPUAcquire() const { return cpuAcquireMs.load(); }
	float GetCPURecord()  const { return cpuRecordMs.load(); }
	float GetCPUSubmit()  const { return cpuSubmitMs.load(); }
	float GetCPUPresent() const { return cpuPresentMs.load(); }

	float GetGameThread()   const { return gameThreadMs.load(); }
	float GetRenderThread() const { return renderThreadMs.load(); }

	uint64_t GetGPUMemoryUsed()   const { return gpuMemoryUsed.load(); }
	uint64_t GetGPUMemoryBudget() const { return gpuMemoryBudget.load(); }
	uint64_t GetSystemMemoryUsed() const { return systemMemoryUsed.load(); }

	void SetGameThreadTime(float ms)   { gameThreadMs.store(ms); }
	void SetRenderThreadTime(float ms) { renderThreadMs.store(ms); }
};

// ─────────────────────────────────────────────────────────────────────────────
class Renderer
{
public:
	Renderer(Window* InWindow);
	virtual ~Renderer();

	void Initialize();
	void Render();
	void Shutdown();
	void ReloadSceneData();

	// ── Core accessors ────────────────────────────────────────────────────────
	VulkanInstance* GetVulkanInstance()       const { return VulkanInstanceWrapper.get(); }
	SwapChain*      GetSwapChain()            const { return SwapChainWrapper.get(); }
	ImGuiSystem*    GetImGuiSystem()          const { return ImGuiSystemWrapper.get(); }
	uint32_t        GetCurrentFrameIndex()    const { return frameIndex; }
	EngineScene&    GetScene()                      { return SceneWrapper; }
	FrameStats&     GetFrameStats()                 { return frameStats; }

	vk::raii::Buffer&    GetVertexBuffer()    { return VulkanVertexBuffer; }
	vk::raii::Buffer&    GetIndexBuffer()     { return VulkanIndexBuffer; }
	vk::raii::ImageView& GetRenderTargetImageView() { return renderTargetImageView; }
	vk::raii::Sampler&   GetRenderTargetSampler()   { return renderTargetSampler; }
	Mesh&                GetMesh()            { return *MeshData; }

	void SetGameThreadTime(float ms) { frameStats.gameThreadMs.store(ms); }

	// ── Material system ───────────────────────────────────────────────────────
	// Returns pointer to the GltfMaterial list of the currently loaded mesh,
	// or nullptr when no mesh is loaded.
	const std::vector<GltfMaterial>* GetLoadedMaterials() const
	{
		if (!MeshData) return nullptr;
		return &MeshData->GetMaterials();
	}

protected:
	// ── Vulkan helpers ────────────────────────────────────────────────────────
	void CreateDepthResources();
	void CreateRenderTarget();
	void CreateImage(uint32_t width, uint32_t height, vk::Format format,
	                 vk::ImageTiling tiling, vk::ImageUsageFlags usage,
	                 vk::MemoryPropertyFlags properties,
	                 vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);
	void UpdateUniformBuffer(uint32_t currentImage);
	void CreateSyncObjects();
	void recordCommandBuffer(uint32_t imageIndex);
	void transition_image_layout(vk::Image image,
	                             vk::ImageLayout old_layout, vk::ImageLayout new_layout,
	                             vk::AccessFlags2 src_access_mask, vk::AccessFlags2 dst_access_mask,
	                             vk::PipelineStageFlags2 src_stage_mask,
	                             vk::PipelineStageFlags2 dst_stage_mask,
	                             vk::ImageAspectFlags image_aspect_flags);
	void CreateTimestampQueryPool();
	void CalculateFrameStats();
	void UpdateMemoryStats();
	static void CreateDefaultTexture(const std::string& path);
	
	vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates,
	                               vk::ImageTiling tiling,
	                               vk::FormatFeatureFlags features)
	{
		for (const auto fmt : candidates)
		{
			vk::FormatProperties props = VulkanInstanceWrapper->GetPhysicalDevice().getFormatProperties(fmt);
			if (tiling == vk::ImageTiling::eLinear  && (props.linearTilingFeatures  & features) == features) return fmt;
			if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) return fmt;
		}
		throw std::runtime_error("failed to find supported format!");
	}

	// ── Per-material GPU resources ────────────────────────────────────────────
	// One entry per GltfMaterial in the loaded mesh.  Each holds its own
	// TextureManager (base-color texture) and MAX_FRAMES_IN_FLIGHT descriptor
	// sets that reference the shared UBO + that material's texture.
	struct PerMaterialGpuData
	{
		std::unique_ptr<TextureManager>      TextureMgr;
		vk::raii::DescriptorPool             DescPool = nullptr;
		std::vector<vk::raii::DescriptorSet> DescSets;
	};

	// Helper: build one PerMaterialGpuData for a given texture path.
	PerMaterialGpuData BuildMaterialGpuData(const std::string& texturePath);

	// ── Core Vulkan objects ───────────────────────────────────────────────────
	std::unique_ptr<VulkanInstance>      VulkanInstanceWrapper;
	std::unique_ptr<SwapChain>           SwapChainWrapper;
	std::unique_ptr<CommandPool>         CommandPoolWrapper;
	std::unique_ptr<CommandBufferManager> CommandBufferManagerWrapper;
	std::unique_ptr<DescriptorManager>   DescriptorManagerWrapper;
	std::unique_ptr<BufferManager>       BufferManagerWrapper;
	std::unique_ptr<TextureManager>      TextureManagerWrapper; // kept as legacy fallback
	std::unique_ptr<PipelineManager>     PipelineManagerWrapper;
	std::unique_ptr<ImGuiSystem>         ImGuiSystemWrapper;
	std::unique_ptr<Mesh>                MeshData;

	// Per-material GPU data — must be declared after the above managers
	// so it is destroyed first (TextureManagers reference CommandPool).
	std::vector<PerMaterialGpuData>      m_MaterialGpuData;

	EngineScene SceneWrapper;

	std::vector<vk::raii::Semaphore> VulkanPresentCompleteSemaphores;
	std::vector<vk::raii::Semaphore> VulkanRenderFinishedSemaphores;
	std::vector<vk::raii::Fence>     inFlightFences;
	vk::raii::Fence                  VulkanDrawFence = nullptr;

	vk::raii::Buffer       VulkanVertexBuffer       = nullptr;
	vk::raii::DeviceMemory VulkanVertexBufferMemory = nullptr;
	vk::raii::Buffer       VulkanIndexBuffer        = nullptr;
	vk::raii::DeviceMemory VulkanIndexBufferMemory  = nullptr;

	uint32_t frameIndex = 0;

	std::vector<const char*> VulkanRequiredDeviceExtension = {
		vk::KHRSwapchainExtensionName,
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName };

	vk::raii::Image        depthImage            = nullptr;
	vk::raii::DeviceMemory depthImageImageMemory = nullptr;
	vk::raii::ImageView    depthImageView        = nullptr;

	vk::raii::Image        renderTargetImage       = nullptr;
	vk::raii::DeviceMemory renderTargetImageMemory = nullptr;
	vk::raii::ImageView    renderTargetImageView   = nullptr;
	vk::raii::Sampler      renderTargetSampler     = nullptr;

	FrameStats frameStats;

	vk::raii::QueryPool      renderTimestampQueryPool = nullptr;
	std::array<uint64_t, 2>  timestampResults         = {};

	// ── Orbit camera state ────────────────────────────────────────────────────
	float     m_CamDistance = 10.0f;
	float     m_CamYaw      = 0.0f;
	float     m_CamPitch    = 25.0f;
	glm::vec3 m_CamTarget   = glm::vec3(0.0f, 0.0f, 1.5f);

private:
	Window* RendererWindow = nullptr;
};
