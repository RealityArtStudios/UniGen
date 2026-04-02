// Copyright (c) CreationArtStudios, Khairol Anwar

#include "TextureManager.h"
#include "VulkanInstance/VulkanInstance.h"
#include "SwapChain.h"
#include "Buffer.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

TextureManager::TextureManager(VulkanInstance* vulkanInstance, SwapChain* swapChain, BufferManager* bufferManager, vk::raii::CommandPool& commandPool)
	: vulkanInstance(vulkanInstance)
	, swapChain(swapChain)
	, bufferManager(bufferManager)
	, commandPool(commandPool)
{
}

TextureManager::~TextureManager()
{
}

void TextureManager::Initialize(const std::string& texturePath)
{
	CreateTextureImage(texturePath);
	CreateTextureImageView();
	CreateTextureSampler();
}

void TextureManager::Shutdown()
{
}

void TextureManager::CreateTextureImage(const std::string& texturePath)
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(texturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	vk::DeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	vk::raii::Buffer stagingBuffer({});
	vk::raii::DeviceMemory stagingBufferMemory({});

	bufferManager->CreateBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

	void* data = stagingBufferMemory.mapMemory(0, imageSize);
	memcpy(data, pixels, imageSize);
	stagingBufferMemory.unmapMemory();

	stbi_image_free(pixels);

	CreateImage(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage, textureImageMemory);

	TransitionImageLayout(textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
	CopyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	TransitionImageLayout(textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
}

void TextureManager::CreateImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory)
{
	vk::ImageCreateInfo imageInfo;
	imageInfo.imageType = vk::ImageType::e2D;
	imageInfo.format = format;
	imageInfo.extent = vk::Extent3D{ width, height, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = vk::SampleCountFlagBits::e1;
	imageInfo.tiling = tiling;
	imageInfo.usage = usage;
	imageInfo.sharingMode = vk::SharingMode::eExclusive;
	imageInfo.initialLayout = vk::ImageLayout::eUndefined;

	image = vk::raii::Image(vulkanInstance->GetLogicalDevice(), imageInfo);

	vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
	vk::MemoryAllocateInfo allocInfo;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = bufferManager->FindMemoryType(memRequirements.memoryTypeBits, properties);

	imageMemory = vk::raii::DeviceMemory(vulkanInstance->GetLogicalDevice(), allocInfo);
	image.bindMemory(imageMemory, 0);
}

void TextureManager::CreateTextureImageView()
{
	textureImageView = swapChain->CreateImageView(textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
}

void TextureManager::CreateTextureSampler()
{
	vk::PhysicalDeviceProperties properties = vulkanInstance->GetPhysicalDevice().getProperties();
	vk::SamplerCreateInfo samplerInfo;
	samplerInfo.magFilter = vk::Filter::eLinear;
	samplerInfo.minFilter = vk::Filter::eLinear;
	samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
	samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
	samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
	samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.anisotropyEnable = vk::True;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	samplerInfo.compareEnable = vk::False;
	samplerInfo.compareOp = vk::CompareOp::eAlways;

	textureSampler = vk::raii::Sampler(vulkanInstance->GetLogicalDevice(), samplerInfo);
}

void TextureManager::CopyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height)
{
	vk::raii::CommandBuffer commandBuffer = bufferManager->beginSingleTimeCommands(commandPool);

	vk::BufferImageCopy region;
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource = { vk::ImageAspectFlagBits::eColor, 0, 0, 1 };
	region.imageOffset = vk::Offset3D{ 0, 0, 0 };
	region.imageExtent = vk::Extent3D{ width, height, 1 };

	commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, { region });
	bufferManager->endSingleTimeCommands(commandBuffer);
}

void TextureManager::TransitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
	vk::raii::CommandBuffer commandBuffer = bufferManager->beginSingleTimeCommands(commandPool);
	vk::ImageMemoryBarrier barrier;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.image = image;
	barrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

	vk::PipelineStageFlags sourceStage;
	vk::PipelineStageFlags destinationStage;

	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
	{
		barrier.srcAccessMask = {};
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
		destinationStage = vk::PipelineStageFlagBits::eTransfer;
	}
	else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
	{
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		sourceStage = vk::PipelineStageFlagBits::eTransfer;
		destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
	}
	else
	{
		throw std::invalid_argument("unsupported layout transition!");
	}
	commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);

	bufferManager->endSingleTimeCommands(commandBuffer);
}
