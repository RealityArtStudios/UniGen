// Copyright (c) CreationArtStudios

#pragma once
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

class Renderer;
class Window;

class ContentBrowserPanel;
class SceneHierarchyPanel;
class PerformancePanel;
class ProjectLauncherPanel;

class ImGuiSystem
{
public:
    ImGuiSystem();
    ~ImGuiSystem();

    bool Initialize(Renderer* renderer, Window* window);
    void Cleanup();
    void NewFrame();
    void Render(vk::raii::CommandBuffer& commandBuffer, uint32_t frameIndex);

    void HandleResize(uint32_t width, uint32_t height);

    bool WantCaptureKeyboard() const;
    bool WantCaptureMouse()    const;

    void SetupViewportDescriptor(VkDescriptorSet descriptor);
    void CleanupViewport();

    // ── Editor panels ─────────────────────────────────────────────────────────
    void SetContentBrowser(std::unique_ptr<ContentBrowserPanel> panel);
    void SetSceneHierarchy(std::unique_ptr<SceneHierarchyPanel> panel);
    void SetPerformancePanel(std::unique_ptr<PerformancePanel>  panel);
    void RefreshContentBrowserIcons();
    void SetCurrentSceneName(const std::string& name) { currentSceneName = name; }
    SceneHierarchyPanel* GetSceneHierarchy() const { return sceneHierarchyPanel.get(); }

    // ── Project launcher ──────────────────────────────────────────────────────
    // Pass nullptr to dismiss the launcher and reveal the normal editor UI.
    // Safe to call from within the launcher's own callbacks – the dismissal is
    // deferred until after OnRender() fully returns (see NewFrame()).
    void SetProjectLauncher(std::unique_ptr<ProjectLauncherPanel> panel);
    ProjectLauncherPanel* GetProjectLauncher() const { return projectLauncherPanel.get(); }

private:
    ImGuiContext* context  = nullptr;
    Renderer*     renderer = nullptr;
    vk::raii::DescriptorPool descriptorPool = nullptr;
    bool initialized = false;

    std::vector<VkDescriptorSet> viewportDescriptors;
    vk::raii::Sampler viewportSampler = nullptr;

    std::unique_ptr<ContentBrowserPanel>  contentBrowser;
    std::unique_ptr<SceneHierarchyPanel>  sceneHierarchyPanel;
    std::unique_ptr<PerformancePanel>     performancePanel;
    std::unique_ptr<ProjectLauncherPanel> projectLauncherPanel;

    // Set to true by SetProjectLauncher(nullptr) so that NewFrame() knows the
    // launcher was intentionally dismissed during its own OnRender() call.
    bool m_LauncherShouldDismiss = false;

    std::string currentSceneName = "Untitled";
    std::string lastSaveMessage;
    float       lastSaveTime = 0.0f;
};
