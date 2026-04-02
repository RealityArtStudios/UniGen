// Copyright (c) CreationArtStudios, Khairol Anwar

#include "CommandBufferManager.h"
#include "VulkanInstance/VulkanInstance.h"
#include "CommandPool.h"

CommandBufferManager::CommandBufferManager(VulkanInstance* vulkanInstance, CommandPool* commandPool, uint32_t bufferCount)
	: VulkanInstanceWrapper(vulkanInstance), CommandPoolWrapper(commandPool)
{
	vk::CommandBufferAllocateInfo allocInfo;
	allocInfo.commandPool = CommandPoolWrapper->GetCommandPool();
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = bufferCount;

	VulkanCommandBuffers = vk::raii::CommandBuffers(VulkanInstanceWrapper->GetLogicalDevice(), allocInfo);
}

CommandBufferManager::~CommandBufferManager()
{
}