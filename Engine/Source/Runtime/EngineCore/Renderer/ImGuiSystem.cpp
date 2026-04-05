// Copyright (c) CreationArtStudios

#include "ImGuiSystem.h"

#include "../Window.h"
#include "Renderer.h"

#include "../../../../Editor/Source/Panels/ContentBrowserPanel.h"
#include "../../../../Editor/Source/Panels/SceneHierarchyPanel.h"
#include "../../../../Editor/Source/Panels/PerformancePanel.h"
#include "../../../../Editor/Source/Panels/ProjectLauncherPanel.h"

#include <iostream>

ImGuiSystem::ImGuiSystem()  = default;
ImGuiSystem::~ImGuiSystem() { Cleanup(); }

// ─────────────────────────────────────────────────────────────────────────────
bool ImGuiSystem::Initialize(Renderer* renderer, Window* window)
{
    if (initialized)
        return true;

    this->renderer = renderer;

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForVulkan(window->getGLFWwindow(), true);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance       = *renderer->GetVulkanInstance()->GetInstance();
    init_info.PhysicalDevice = *renderer->GetVulkanInstance()->GetPhysicalDevice();
    init_info.Device         = *renderer->GetVulkanInstance()->GetLogicalDevice();
    init_info.QueueFamily    = renderer->GetVulkanInstance()->GetQueueFamilyIndex();
    init_info.Queue          = *renderer->GetVulkanInstance()->GetGraphicsQueue();
    init_info.PipelineCache  = VK_NULL_HANDLE;
    init_info.DescriptorPool = VK_NULL_HANDLE;
    init_info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;
    init_info.MinImageCount  = 2;
    init_info.ImageCount     = 2;

    init_info.PipelineInfoMain.Subpass     = 0;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    init_info.UseDynamicRendering = true;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat =
        VK_FORMAT_D32_SFLOAT;

    init_info.PipelineInfoForViewports = init_info.PipelineInfoMain;

    init_info.CheckVkResultFn = [](VkResult err) {
        if (err != VK_SUCCESS)
            std::cerr << "[ImGui Vulkan] Error: " << err << std::endl;
    };

    if (!ImGui_ImplVulkan_Init(&init_info))
    {
        std::cerr << "Failed to initialize ImGui Vulkan backend" << std::endl;
        return false;
    }

    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter    = vk::Filter::eLinear;
    samplerInfo.minFilter    = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.borderColor  = vk::BorderColor::eFloatTransparentBlack;

    viewportSampler = vk::raii::Sampler(
        renderer->GetVulkanInstance()->GetLogicalDevice(), samplerInfo);

    initialized = true;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
void ImGuiSystem::Cleanup()
{
    if (!initialized) return;

    renderer->GetVulkanInstance()->GetLogicalDevice().waitIdle();
    viewportDescriptors.clear();
    viewportSampler = nullptr;

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    initialized = false;
}

// ─────────────────────────────────────────────────────────────────────────────
void ImGuiSystem::SetupViewportDescriptor(VkDescriptorSet descriptor)
{
    if (viewportDescriptors.empty()) viewportDescriptors.push_back(descriptor);
    else                             viewportDescriptors[0] = descriptor;
}

void ImGuiSystem::CleanupViewport() { viewportDescriptors.clear(); }

// ── Panel setters ─────────────────────────────────────────────────────────────
void ImGuiSystem::SetContentBrowser(std::unique_ptr<ContentBrowserPanel> panel)
{
    contentBrowser = std::move(panel);
}
void ImGuiSystem::RefreshContentBrowserIcons()
{
    if (contentBrowser) contentBrowser->RefreshIcons();
}
void ImGuiSystem::SetSceneHierarchy(std::unique_ptr<SceneHierarchyPanel> panel)
{
    sceneHierarchyPanel = std::move(panel);
}
void ImGuiSystem::SetPerformancePanel(std::unique_ptr<PerformancePanel> panel)
{
    performancePanel = std::move(panel);
}

// ─────────────────────────────────────────────────────────────────────────────
//  SetProjectLauncher
//  Called with nullptr to dismiss the launcher.  We record the intent via a
//  flag so NewFrame()'s move-to-local pattern can react correctly even when
//  the call arrives from inside OnRender().
// ─────────────────────────────────────────────────────────────────────────────
void ImGuiSystem::SetProjectLauncher(std::unique_ptr<ProjectLauncherPanel> panel)
{
    projectLauncherPanel = std::move(panel);

    // If the caller passed nullptr they want the launcher gone.
    // NewFrame() checks this flag after OnRender() returns.
    if (!projectLauncherPanel)
        m_LauncherShouldDismiss = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NewFrame
// ─────────────────────────────────────────────────────────────────────────────
void ImGuiSystem::NewFrame()
{
    if (!initialized) return;

    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ── Project launcher ──────────────────────────────────────────────────────
    // When the launcher is active it takes over the entire frame.
    //
    // The tricky part: the launcher's OnRender() fires a std::function callback
    // (OnOpenProject / OnCreateProject) that in turn calls
    // SetProjectLauncher(nullptr) to dismiss itself.  That would destroy the
    // panel – and its stored std::function – while we are still executing the
    // lambda closure inside that std::function.  On MSVC debug builds, captured
    // variables are re-read from the closure object on every access; a freed
    // closure therefore reads back as 0xDDDDDDDD, causing an immediate crash.
    //
    // Solution – "move to local" pattern:
    //   1. Move projectLauncherPanel into a local unique_ptr.
    //      The member is now null; the panel lives in `localPanel`.
    //   2. Call localPanel->OnRender().  The closure object is inside localPanel
    //      so it stays alive for the entire call, even if SetProjectLauncher(nullptr)
    //      is called from within the callback.
    //   3. After OnRender() returns, check m_LauncherShouldDismiss:
    //      - true  → the callback dismissed the launcher → let localPanel expire.
    //      - false → nothing happened this frame → restore to projectLauncherPanel
    //                so it's shown again next frame.
    if (projectLauncherPanel)
    {
        m_LauncherShouldDismiss = false;                     // reset before render
        auto localPanel = std::move(projectLauncherPanel);   // keep panel alive
        localPanel->OnRender();                              // callbacks may fire

        if (!m_LauncherShouldDismiss)
            projectLauncherPanel = std::move(localPanel);   // restore if not dismissed
        // else: localPanel expires here – panel destroyed safely after callbacks done

        return;
    }

    // ── Normal editor UI ──────────────────────────────────────────────────────

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 0.0f));

    ImGui::Begin("EditorDockSpace", nullptr,
        ImGuiWindowFlags_NoDocking  | ImGuiWindowFlags_NoTitleBar  |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar(3);

    ImGuiID dockId = ImGui::GetID("EditorDock");
    ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();

    // ── Viewport window ───────────────────────────────────────────────────────
    ImGui::SetNextWindowDockID(dockId, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Viewport"))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::Text("Level: ");
        ImGui::SameLine();
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));

        static char levelNameBuffer[128] = "Untitled";
        if (sceneHierarchyPanel)
        {
            std::string sn = sceneHierarchyPanel->GetSceneName();
            if (!sn.empty())
                strncpy_s(levelNameBuffer, sn.c_str(), sizeof(levelNameBuffer) - 1);
        }
        else if (!currentSceneName.empty())
        {
            strncpy_s(levelNameBuffer, currentSceneName.c_str(), sizeof(levelNameBuffer) - 1);
        }

        ImGui::PushItemWidth(200);
        if (ImGui::InputText("##LevelName", levelNameBuffer, sizeof(levelNameBuffer)))
        {
            if (sceneHierarchyPanel)
                sceneHierarchyPanel->SetSceneName(levelNameBuffer);
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();

        if (ImGui::Button("Save"))
        {
            if (sceneHierarchyPanel)
            {
                sceneHierarchyPanel->SetSceneName(levelNameBuffer);
                std::filesystem::path sp = sceneHierarchyPanel->GetCurrentScenePath();
                if (sp.empty())
                {
                    sp = "Game/Content/Scenes/";
                    std::filesystem::create_directories(sp);
                    sp /= std::string(levelNameBuffer) + ".ungscene";
                }
                sceneHierarchyPanel->SaveScene(sp);
                currentSceneName = levelNameBuffer;
                lastSaveMessage  = "Saved: " + std::string(levelNameBuffer);
                lastSaveTime     = ImGui::GetTime();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save All"))
        {
            if (sceneHierarchyPanel)
            {
                auto sp = sceneHierarchyPanel->GetCurrentScenePath();
                if (!sp.empty())
                {
                    sceneHierarchyPanel->SaveCurrentScene();
                    lastSaveMessage = "Saved: " + sceneHierarchyPanel->GetSceneName();
                    lastSaveTime    = ImGui::GetTime();
                }
            }
        }
        ImGui::PopStyleColor();

        if (!lastSaveMessage.empty())
        {
            if (ImGui::GetTime() - lastSaveTime < 3.0f)
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                ImGui::Text("%s", lastSaveMessage.c_str());
                ImGui::PopStyleColor();
            }
            else { lastSaveMessage.clear(); }
        }

        ImGui::Separator();

        ImVec2    avail = ImGui::GetContentRegionAvail();
        ImTextureID texId = reinterpret_cast<ImTextureID>(
            viewportDescriptors.empty() ? VK_NULL_HANDLE : viewportDescriptors[0]);
        ImGui::Image(texId, avail);
    }
    ImGui::End();

    if (contentBrowser)     contentBrowser->OnRender();
    if (sceneHierarchyPanel) sceneHierarchyPanel->OnImGuiRender();
    if (performancePanel)   performancePanel->OnImGuiRender();
}

// ─────────────────────────────────────────────────────────────────────────────
void ImGuiSystem::Render(vk::raii::CommandBuffer& commandBuffer, uint32_t)
{
    if (!initialized) return;

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffer);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

void ImGuiSystem::HandleResize(uint32_t, uint32_t) {}

bool ImGuiSystem::WantCaptureKeyboard() const
{
    return initialized && ImGui::GetIO().WantCaptureKeyboard;
}
bool ImGuiSystem::WantCaptureMouse() const
{
    return initialized && ImGui::GetIO().WantCaptureMouse;
}
