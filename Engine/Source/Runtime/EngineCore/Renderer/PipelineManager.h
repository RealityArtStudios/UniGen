// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#include <memory>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class VulkanInstance;
class SwapChain;
class Mesh;
class Shader;
class FileSystem;

class PipelineManager
{
public:
	PipelineManager(VulkanInstance* vulkanInstance, SwapChain* swapChain);
	~PipelineManager();

	void CreateDescriptorSetLayout();
	void CreateGraphicsPipeline();

	vk::raii::Pipeline& GetGraphicsPipeline() { return VulkanGraphicsPipeline; }
	vk::raii::PipelineLayout& GetPipelineLayout() { return VulkanPipelineLayout; }
	vk::raii::DescriptorSetLayout& GetDescriptorSetLayout() { return VulkanDescriptorSetLayout; }
	vk::Format GetDepthFormat() const { return depthFormat; }

	vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);
	vk::Format findDepthFormat();
	bool hasStencilComponent(vk::Format format);

	VulkanInstance* VulkanInstanceWrapper = nullptr;
	SwapChain* SwapChainWrapper = nullptr;

	vk::raii::DescriptorSetLayout VulkanDescriptorSetLayout = nullptr;
	vk::raii::PipelineLayout VulkanPipelineLayout = nullptr;
	vk::raii::Pipeline VulkanGraphicsPipeline = nullptr;
	vk::Format depthFormat = vk::Format::eUndefined;
};