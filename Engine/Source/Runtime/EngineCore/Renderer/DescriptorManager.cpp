// Copyright (c) CreationArtStudios, Khairol Anwar

#include "DescriptorManager.h"
#include "VulkanInstance/VulkanInstance.h"
#include "PipelineManager.h"
#include "TextureManager.h"
#include "Mesh.h"
#include <cstring>

DescriptorManager::DescriptorManager(VulkanInstance* vulkanInstance, uint32_t maxFramesInFlight)
	: VulkanInstanceWrapper(vulkanInstance), MaxFramesInFlight(maxFramesInFlight)
{
}

DescriptorManager::~DescriptorManager()
{
}

void DescriptorManager::CreateDescriptorPool()
{
	std::array poolSize{
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MaxFramesInFlight),
		vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MaxFramesInFlight)
	};
	vk::DescriptorPoolCreateInfo poolInfo;
	poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
	poolInfo.maxSets = MaxFramesInFlight;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSize.size());
	poolInfo.pPoolSizes = poolSize.data();

	VulkanDescriptorPool = vk::raii::DescriptorPool(VulkanInstanceWrapper->GetLogicalDevice(), poolInfo);
}

void DescriptorManager::CreateDescriptorSets(PipelineManager* pipelineManager, TextureManager* textureManager, Mesh* mesh)
{
	std::vector<vk::DescriptorSetLayout> layouts(MaxFramesInFlight, *pipelineManager->GetDescriptorSetLayout());
	vk::DescriptorSetAllocateInfo allocInfo;
	allocInfo.descriptorPool = VulkanDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	VulkanDescriptorSets.clear();
	VulkanDescriptorSets = VulkanInstanceWrapper->GetLogicalDevice().allocateDescriptorSets(allocInfo);

	for (size_t i = 0; i < MaxFramesInFlight; i++)
	{
		vk::DescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = UniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(Mesh::UniformBufferObject);

		vk::DescriptorImageInfo imageInfo;
		imageInfo.sampler = *textureManager->GetTextureSampler();
		imageInfo.imageView = *textureManager->GetTextureImageView();
		imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		vk::WriteDescriptorSet bufferdescriptorWrite;
		bufferdescriptorWrite.dstSet = VulkanDescriptorSets[i];
		bufferdescriptorWrite.dstBinding = 0;
		bufferdescriptorWrite.dstArrayElement = 0;
		bufferdescriptorWrite.descriptorCount = 1;
		bufferdescriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
		bufferdescriptorWrite.pBufferInfo = &bufferInfo;

		vk::WriteDescriptorSet imageinfodescriptorWrite;
		imageinfodescriptorWrite.dstSet = VulkanDescriptorSets[i];
		imageinfodescriptorWrite.dstBinding = 1;
		imageinfodescriptorWrite.dstArrayElement = 0;
		imageinfodescriptorWrite.descriptorCount = 1;
		imageinfodescriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
		imageinfodescriptorWrite.pImageInfo = &imageInfo;

		std::array descriptorWrites{ bufferdescriptorWrite , imageinfodescriptorWrite };
		VulkanInstanceWrapper->GetLogicalDevice().updateDescriptorSets(descriptorWrites, {});
	}
}

void DescriptorManager::UpdateUniformBuffer(uint32_t currentImage, const Mesh::UniformBufferObject& ubo)
{
	memcpy(UniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void DescriptorManager::SetUniformBuffers(std::vector<vk::raii::Buffer>& buffers, std::vector<vk::raii::DeviceMemory>& memories, std::vector<void*>& mapped)
{
	UniformBuffers = std::move(buffers);
	UniformBuffersMemory = std::move(memories);
	UniformBuffersMapped = std::move(mapped);
}