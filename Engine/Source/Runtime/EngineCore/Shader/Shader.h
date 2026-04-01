// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#include <vulkan/vulkan_raii.hpp>

class Shader
{
public:
    Shader();
    
    //ShaderManager
    vk::raii::ShaderModule CreateShaderModule(const std::vector<char>& code,const vk::raii::Device& VulkanLogicalDevice) const;
};