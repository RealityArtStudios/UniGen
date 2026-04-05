// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#include <vector>
#include <memory>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

class VulkanInstance;
class PipelineManager;
class TextureManager;
class Mesh;

#include "Mesh.h"

class DescriptorManager
{
public:
	DescriptorManager(VulkanInstance* vulkanInstance, uint32_t maxFramesInFlight);
	~DescriptorManager();

	void CreateDescriptorPool();
	void CreateDescriptorSets(PipelineManager* pipelineManager, TextureManager* textureManager, Mesh* mesh);

	vk::raii::DescriptorPool& GetDescriptorPool()                      { return VulkanDescriptorPool; }
	vk::raii::DescriptorSet&  GetDescriptorSet(uint32_t index)         { return VulkanDescriptorSets[index]; }
	vk::raii::DescriptorSet&  GetCurrentDescriptorSet(uint32_t frame)  { return VulkanDescriptorSets[frame]; }

	void UpdateUniformBuffer(uint32_t currentImage, const Mesh::UniformBufferObject& ubo);

	void SetUniformBuffers(std::vector<vk::raii::Buffer>& buffers,
	                       std::vector<vk::raii::DeviceMemory>& memories,
	                       std::vector<void*>& mapped);

	// Expose the raw VkBuffer handle so Renderer can reference the shared UBO
	// when building per-material descriptor sets.
	vk::Buffer GetUniformBuffer(uint32_t frameIndex) const
	{
		return *UniformBuffers[frameIndex];
	}

private:
	VulkanInstance* VulkanInstanceWrapper = nullptr;
	uint32_t        MaxFramesInFlight     = 2;

	vk::raii::DescriptorPool             VulkanDescriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> VulkanDescriptorSets;

	std::vector<vk::raii::Buffer>       UniformBuffers;
	std::vector<vk::raii::DeviceMemory> UniformBuffersMemory;
	std::vector<void*>                  UniformBuffersMapped;
};
