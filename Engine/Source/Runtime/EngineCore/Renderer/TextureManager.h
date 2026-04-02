// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <string>
#include <memory>

class VulkanInstance;
class SwapChain;
class BufferManager;

class TextureManager
{
public:
	TextureManager(VulkanInstance* vulkanInstance, SwapChain* swapChain, BufferManager* bufferManager, vk::raii::CommandPool& commandPool);
	~TextureManager();

	void Initialize(const std::string& texturePath);
	void Shutdown();

	vk::raii::Image& GetTextureImage() { return textureImage; }
	vk::raii::ImageView& GetTextureImageView() { return textureImageView; }
	vk::raii::Sampler& GetTextureSampler() { return textureSampler; }

	void CopyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height);
	void TransitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

private:
	void CreateTextureImage(const std::string& texturePath);
	void CreateTextureImageView();
	void CreateTextureSampler();
	void CreateImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling,
		vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
		vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory);

	VulkanInstance* vulkanInstance = nullptr;
	SwapChain* swapChain = nullptr;
	BufferManager* bufferManager = nullptr;
	vk::raii::CommandPool& commandPool;

	vk::raii::Image textureImage = nullptr;
	vk::raii::DeviceMemory textureImageMemory = nullptr;
	vk::raii::ImageView textureImageView = nullptr;
	vk::raii::Sampler textureSampler = nullptr;
};
