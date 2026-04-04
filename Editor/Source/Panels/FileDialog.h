#pragma once

#include <filesystem>
#include <string>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#   include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <vulkan/vulkan.h>

class Renderer;

// ─────────────────────────────────────────────────────────────────────────────
//  PanelTexture
// ─────────────────────────────────────────────────────────────────────────────
struct PanelTexture
{
    vk::raii::Image        image      = nullptr;
    vk::raii::DeviceMemory memory     = nullptr;
    vk::raii::ImageView    imageView  = nullptr;
    vk::raii::Sampler      sampler    = nullptr;
    VkDescriptorSet        descriptor = VK_NULL_HANDLE;

    bool IsValid() const { return descriptor != VK_NULL_HANDLE; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  FileDialog
//  Modern ImGui-based file browser
// ─────────────────────────────────────────────────────────────────────────────
class FileDialog
{
public:
    FileDialog() = default;
    
    static bool OpenFile(
        const char* title,
        const char* startingDir,
        const char* filters,
        std::filesystem::path& outPath,
        bool allowMultiple = false);

    static bool OpenFolder(
        const char* title,
        const char* startingDir,
        std::filesystem::path& outPath);

private:
    static std::string s_Title;
    static std::filesystem::path s_StartPath;
    static std::vector<std::filesystem::path> s_SelectedPaths;
    static std::string s_CurrentPath;
    static std::string s_Filter;
    static bool s_AllowMultiple;
    static bool s_IsFolderPicker;
    static bool s_Open;
    static std::vector<std::string> s_ParsedFilters;
    static std::string s_SelectedFilter;
    
    static bool Render(const char* label);
    static void ParseFilters(const char* filters);
};
