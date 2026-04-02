// Copyright (c) CreationArtStudios, Khairol Anwar

#include "CommandPool.h"
#include "VulkanInstance/VulkanInstance.h"

CommandPool::CommandPool(VulkanInstance* vulkanInstance)
	: VulkanInstanceWrapper(vulkanInstance)
{
	vk::CommandPoolCreateInfo poolInfo;
	poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
	poolInfo.queueFamilyIndex = VulkanInstanceWrapper->GetQueueFamilyIndex();

	VulkanCommandPool = vk::raii::CommandPool(VulkanInstanceWrapper->GetLogicalDevice(), poolInfo);
	queueFamilyIndex = VulkanInstanceWrapper->GetQueueFamilyIndex();
}

CommandPool::~CommandPool()
{
}