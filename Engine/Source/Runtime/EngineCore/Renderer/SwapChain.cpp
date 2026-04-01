// Copyright (c) CreationArtStudios, Khairol Anwar

#define GLFW_INCLUDE_VULKAN
#include "SwapChain.h"
#include "VulkanInstance/VulkanInstance.h"
#include "../Window.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <limits>
#include <ranges>

SwapChain::SwapChain(VulkanInstance* vulkanInstance, Window* window)
	: VulkanInstanceWrapper(vulkanInstance), RendererWindow(window)
{
}

SwapChain::~SwapChain()
{
	Cleanup();
}

void SwapChain::Create()
{
	CreateSwapChain();
	CreateImageViews();
}

void SwapChain::Cleanup()
{
	VulkanSwapChainImageViews.clear();
	VulkanSwapChain = nullptr;
}

void SwapChain::Recreate()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(RendererWindow->getGLFWwindow(), &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(RendererWindow->getGLFWwindow(), &width, &height);
		glfwWaitEvents();
	}

	VulkanInstanceWrapper->GetLogicalDevice().waitIdle();

	Cleanup();
	Create();
}

void SwapChain::CreateSwapChain()
{
	auto surfaceCapabilities = VulkanInstanceWrapper->GetPhysicalDevice().getSurfaceCapabilitiesKHR(*VulkanInstanceWrapper->GetSurface());
	VulkanSwapChainExtent = chooseSwapExtent(surfaceCapabilities);
	VulkanSwapChainSurfaceFormat = chooseSwapSurfaceFormat(VulkanInstanceWrapper->GetPhysicalDevice().getSurfaceFormatsKHR(*VulkanInstanceWrapper->GetSurface()));

	vk::SwapchainCreateInfoKHR swapChainCreateInfo;
	swapChainCreateInfo.surface = *VulkanInstanceWrapper->GetSurface();
	swapChainCreateInfo.minImageCount = chooseSwapMinImageCount(surfaceCapabilities);
	swapChainCreateInfo.imageFormat = VulkanSwapChainSurfaceFormat.format;
	swapChainCreateInfo.imageColorSpace = VulkanSwapChainSurfaceFormat.colorSpace;
	swapChainCreateInfo.imageExtent = VulkanSwapChainExtent;
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	swapChainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
	swapChainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapChainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	swapChainCreateInfo.presentMode = chooseSwapPresentMode(VulkanInstanceWrapper->GetPhysicalDevice().getSurfacePresentModesKHR(*VulkanInstanceWrapper->GetSurface()));
	swapChainCreateInfo.clipped = true;

	VulkanSwapChain = vk::raii::SwapchainKHR(VulkanInstanceWrapper->GetLogicalDevice(), swapChainCreateInfo);
	VulkanSwapChainImages = VulkanSwapChain.getImages();
}

void SwapChain::CreateImageViews()
{
	assert(VulkanSwapChainImageViews.empty());

	vk::ImageViewCreateInfo imageViewCreateInfo;
	imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
	imageViewCreateInfo.format = VulkanSwapChainSurfaceFormat.format;
	imageViewCreateInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };

	for (auto& image : VulkanSwapChainImages)
	{
		imageViewCreateInfo.image = image;
		VulkanSwapChainImageViews.emplace_back(VulkanInstanceWrapper->GetLogicalDevice(), imageViewCreateInfo);
	}
}

vk::Extent2D SwapChain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(RendererWindow->getGLFWwindow(), &width, &height);

		vk::Extent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = glm::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = glm::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

vk::raii::ImageView SwapChain::CreateImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags)
{
	vk::ImageViewCreateInfo viewInfo;
	viewInfo.image = image;
	viewInfo.viewType = vk::ImageViewType::e2D;
	viewInfo.format = format;
	viewInfo.subresourceRange = { aspectFlags, 0, 1, 0, 1 };

	return vk::raii::ImageView(VulkanInstanceWrapper->GetLogicalDevice(), viewInfo);
}