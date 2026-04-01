// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#include <memory>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

class VulkanInstance;
class Window;

class SwapChain
{
public:
	SwapChain(VulkanInstance* vulkanInstance, Window* window);
	~SwapChain();

	void Create();
	void Cleanup();
	void Recreate();

	vk::raii::SwapchainKHR& GetSwapChain() { return VulkanSwapChain; }
	vk::Extent2D GetExtent() const { return VulkanSwapChainExtent; }
	vk::SurfaceFormatKHR GetFormat() const { return VulkanSwapChainSurfaceFormat; }
	vk::raii::ImageView& GetImageView(uint32_t index) { return VulkanSwapChainImageViews[index]; }
	const std::vector<vk::Image>& GetImages() const { return VulkanSwapChainImages; }
	const std::vector<vk::raii::ImageView>& GetImageViews() const { return VulkanSwapChainImageViews; }

	void CreateSwapChain();
	void CreateImageViews();
	vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
	vk::raii::ImageView CreateImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags);

	static uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surfaceCapabilities)
	{
		auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
		if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
		{
			minImageCount = surfaceCapabilities.maxImageCount;
		}
		return minImageCount;
	}

	static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats)
	{
		assert(!availableFormats.empty());
		const auto formatIt = std::ranges::find_if(
			availableFormats,
			[](const auto& format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
		return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
	}

	static vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
	{
		assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
		return std::ranges::any_of(availablePresentModes,
			[](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
			vk::PresentModeKHR::eMailbox :
			vk::PresentModeKHR::eFifo;
	}

	VulkanInstance* VulkanInstanceWrapper = nullptr;
	Window* RendererWindow = nullptr;

	vk::raii::SwapchainKHR VulkanSwapChain = nullptr;
	std::vector<vk::Image> VulkanSwapChainImages;
	vk::SurfaceFormatKHR VulkanSwapChainSurfaceFormat;
	vk::Extent2D VulkanSwapChainExtent;
	std::vector<vk::raii::ImageView> VulkanSwapChainImageViews;
};