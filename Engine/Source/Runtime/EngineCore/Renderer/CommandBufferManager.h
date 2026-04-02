// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

class VulkanInstance;
class CommandPool;

class CommandBufferManager
{
public:
	CommandBufferManager(VulkanInstance* vulkanInstance, CommandPool* commandPool, uint32_t bufferCount);
	~CommandBufferManager();

	vk::raii::CommandBuffer& GetCommandBuffer(uint32_t index) { return VulkanCommandBuffers[index]; }
	vk::raii::CommandBuffer& GetCurrentCommandBuffer(uint32_t frameIndex) { return VulkanCommandBuffers[frameIndex]; }
	const std::vector<vk::raii::CommandBuffer>& GetCommandBuffers() const { return VulkanCommandBuffers; }
	uint32_t GetBufferCount() const { return static_cast<uint32_t>(VulkanCommandBuffers.size()); }

private:
	VulkanInstance* VulkanInstanceWrapper = nullptr;
	CommandPool* CommandPoolWrapper = nullptr;
	std::vector<vk::raii::CommandBuffer> VulkanCommandBuffers;
};