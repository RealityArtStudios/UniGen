// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once
#include <memory>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Window;

class VulkanInstance
{
public:
	VulkanInstance();
	~VulkanInstance();

	void CreateInstance(Window* window);
	void CreateSurface(Window* window);
	void SetupDebugMessenger();
	void Shutdown();
	vk::raii::Queue& GetGraphicsQueue() { return VulkanGraphicsQueue; }

	vk::raii::Instance& GetInstance() { return m_VulkanInstance; }
	vk::raii::PhysicalDevice& GetPhysicalDevice() { return VulkanPhysicalDevice; }
	vk::raii::Device& GetLogicalDevice() { return VulkanLogicalDevice; }

	void CreateLogicalDevice();
	vk::raii::SurfaceKHR& GetSurface() { return VulkanSurface; }
	vk::raii::Context& GetContext() { return VulkanContext; }

	vk::raii::DebugUtilsMessengerEXT& GetDebugMessenger() { return VulkanDebugMessenger; }
	uint32_t GetQueueFamilyIndex() const { return queueIndex; }

private:
	void PickPhysicalDevice(Window* window);
	std::vector<char const*> getRequiredExtensions();

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*);

	vk::raii::Context VulkanContext;
	vk::raii::Instance m_VulkanInstance = nullptr;
	vk::raii::DebugUtilsMessengerEXT VulkanDebugMessenger = nullptr;
	vk::raii::PhysicalDevice VulkanPhysicalDevice = nullptr;
	vk::raii::Device VulkanLogicalDevice = nullptr;
	vk::raii::SurfaceKHR VulkanSurface = nullptr;
	vk::raii::Queue VulkanGraphicsQueue = nullptr;
	uint32_t queueIndex = ~0;
	Window* RendererWindow = nullptr;

	std::vector<const char*> VulkanRequiredDeviceExtension = { vk::KHRSwapchainExtensionName,
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName };
};