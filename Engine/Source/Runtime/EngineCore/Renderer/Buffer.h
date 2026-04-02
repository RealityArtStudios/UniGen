// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#include <memory>
#include <vector>

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

#include "Mesh.h"

class VulkanInstance;

class BufferManager
{
public:
	BufferManager(VulkanInstance* vulkanInstance);
	~BufferManager();

	void CreateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Buffer& buffer, vk::raii::DeviceMemory& bufferMemory);
	void CopyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size, vk::raii::CommandPool& commandPool);

	void CreateVertexBuffer(const std::vector<Mesh::Vertex>& vertices, vk::raii::Buffer& vertexBuffer, vk::raii::DeviceMemory& vertexBufferMemory, vk::raii::CommandPool& commandPool, vk::raii::Queue& graphicsQueue);
	void CreateIndexBuffer(const std::vector<uint32_t>& indices, vk::raii::Buffer& indexBuffer, vk::raii::DeviceMemory& indexBufferMemory, vk::raii::CommandPool& commandPool, vk::raii::Queue& graphicsQueue);
	void CreateUniformBuffers(uint32_t count, std::vector<vk::raii::Buffer>& uniformBuffers, std::vector<vk::raii::DeviceMemory>& uniformBuffersMemory, std::vector<void*>& uniformBuffersMapped);

	uint32_t FindMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

	vk::raii::CommandBuffer beginSingleTimeCommands(vk::raii::CommandPool& commandPool);

	void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer);
private:
	VulkanInstance* VulkanInstanceWrapper = nullptr;
};
