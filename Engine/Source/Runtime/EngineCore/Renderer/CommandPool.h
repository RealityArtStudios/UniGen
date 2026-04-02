// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

class VulkanInstance;

class CommandPool
{
public:
	CommandPool(VulkanInstance* vulkanInstance);
	~CommandPool();

	vk::raii::CommandPool& GetCommandPool() { return VulkanCommandPool; }
	const vk::raii::CommandPool& GetCommandPool() const { return VulkanCommandPool; }
	uint32_t GetQueueFamilyIndex() const { return queueFamilyIndex; }

private:
	VulkanInstance* VulkanInstanceWrapper = nullptr;
	vk::raii::CommandPool VulkanCommandPool = nullptr;
	uint32_t queueFamilyIndex = ~0;
};