// Copyright (c) CreationArtStudios, Khairol Anwar

#include "Buffer.h"
#include "VulkanInstance/VulkanInstance.h"

#include <stdexcept>
#include <cstring>
#include "Renderer.h"
BufferManager::BufferManager(VulkanInstance* vulkanInstance)
	: VulkanInstanceWrapper(vulkanInstance)
{
}

BufferManager::~BufferManager()
{
}

void BufferManager::CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory)
{
	vk::BufferCreateInfo bufferInfo;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = vk::SharingMode::eExclusive;

	buffer = vk::raii::Buffer(VulkanInstanceWrapper->GetLogicalDevice(), bufferInfo);

	vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
	vk::MemoryAllocateInfo allocInfo;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

	bufferMemory = vk::raii::DeviceMemory(VulkanInstanceWrapper->GetLogicalDevice(), allocInfo);
	buffer.bindMemory(*bufferMemory, 0);
}

void BufferManager::CopyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size, vk::raii::CommandPool& commandPool)
{
	vk::CommandBufferAllocateInfo allocInfo;
	allocInfo.commandPool = commandPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = 1;

	vk::raii::CommandBuffer commandCopyBuffer = std::move(VulkanInstanceWrapper->GetLogicalDevice().allocateCommandBuffers(allocInfo).front());

	vk::CommandBufferBeginInfo CommandBufferBeginInfo;
	CommandBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	commandCopyBuffer.begin(CommandBufferBeginInfo);

	vk::BufferCopy copybuffer;
	copybuffer.size = size;

	commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, copybuffer);
	commandCopyBuffer.end();

	vk::SubmitInfo SubmitInfo;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &*commandCopyBuffer;

	VulkanInstanceWrapper->GetGraphicsQueue().submit(SubmitInfo, nullptr);
	VulkanInstanceWrapper->GetGraphicsQueue().waitIdle();
}

void BufferManager::CreateVertexBuffer(const std::vector<Mesh::Vertex>& vertices, vk::raii::Buffer& vertexBuffer, vk::raii::DeviceMemory& vertexBufferMemory, vk::raii::CommandPool& commandPool, vk::raii::Queue& graphicsQueue)
{
	vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	vk::raii::Buffer stagingBuffer({});
	vk::raii::DeviceMemory stagingBufferMemory({});
	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

	void* dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
	memcpy(dataStaging, vertices.data(), bufferSize);
	stagingBufferMemory.unmapMemory();

	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer, vertexBufferMemory);

	CopyBuffer(stagingBuffer, vertexBuffer, bufferSize, commandPool);
}

void BufferManager::CreateIndexBuffer(const std::vector<uint32_t>& indices, vk::raii::Buffer& indexBuffer, vk::raii::DeviceMemory& indexBufferMemory, vk::raii::CommandPool& commandPool, vk::raii::Queue& graphicsQueue)
{
	vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	vk::raii::Buffer stagingBuffer({});
	vk::raii::DeviceMemory stagingBufferMemory({});
	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

	void* data = stagingBufferMemory.mapMemory(0, bufferSize);
	memcpy(data, indices.data(), (size_t)bufferSize);
	stagingBufferMemory.unmapMemory();

	CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer, indexBufferMemory);

	CopyBuffer(stagingBuffer, indexBuffer, bufferSize, commandPool);
}

void BufferManager::CreateUniformBuffers(uint32_t count, std::vector<vk::raii::Buffer>& uniformBuffers, std::vector<vk::raii::DeviceMemory>& uniformBuffersMemory, std::vector<void*>& uniformBuffersMapped)
{
	uniformBuffers.clear();
	uniformBuffersMemory.clear();
	uniformBuffersMapped.clear();

	for (size_t i = 0; i < count; i++)
	{
		vk::DeviceSize bufferSize = sizeof(glm::mat4) * 3;
		vk::raii::Buffer buffer({});
		vk::raii::DeviceMemory bufferMem({});
		CreateBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, buffer, bufferMem);
		uniformBuffers.emplace_back(std::move(buffer));
		uniformBuffersMemory.emplace_back(std::move(bufferMem));
		uniformBuffersMapped.emplace_back(uniformBuffersMemory[i].mapMemory(0, bufferSize));
	}
}

uint32_t BufferManager::FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
	vk::PhysicalDeviceMemoryProperties memProperties = VulkanInstanceWrapper->GetPhysicalDevice().getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}

vk::raii::CommandBuffer BufferManager::beginSingleTimeCommands(vk::raii::CommandPool& commandPool)
{
	vk::CommandBufferAllocateInfo allocInfo;
	allocInfo.commandPool = commandPool;
	allocInfo.level = vk::CommandBufferLevel::ePrimary;
	allocInfo.commandBufferCount = 1;

	vk::raii::CommandBuffer commandBuffer = std::move(VulkanInstanceWrapper->GetLogicalDevice().allocateCommandBuffers(allocInfo).front());

	vk::CommandBufferBeginInfo beginInfo;
	beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

	commandBuffer.begin(beginInfo);

	return commandBuffer;
}

void BufferManager::endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer)
{
	commandBuffer.end();

	vk::SubmitInfo SubmitInfo;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &*commandBuffer;

	VulkanInstanceWrapper->GetGraphicsQueue().submit(SubmitInfo, nullptr);
	VulkanInstanceWrapper->GetGraphicsQueue().waitIdle();
}
