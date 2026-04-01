// Copyright (c) CreationArtStudios, Khairol Anwar

#include "Shader.h"

Shader::Shader()
{
}

vk::raii::ShaderModule Shader::CreateShaderModule(const std::vector<char>& code, const vk::raii::Device& VulkanLogicalDevice) const
{
    vk::ShaderModuleCreateInfo CreateInfo;
    CreateInfo.codeSize = code.size() * sizeof(char);
    CreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    vk::raii::ShaderModule     ShaderModule{ VulkanLogicalDevice, CreateInfo };

    return ShaderModule;
} 
