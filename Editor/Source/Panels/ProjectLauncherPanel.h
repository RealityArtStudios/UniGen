#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  ProjectLauncherPanel
//  Shown at startup when no project is passed via command line or the user
//  cancels the open-file dialog.  Lets the user either browse to an existing
//  .ungproject or create a fresh one from a built-in template.
// ─────────────────────────────────────────────────────────────────────────────
class ProjectLauncherPanel
{
public:
    struct TemplateInfo
    {
        std::string name;
        std::string description;
        std::string icon; // placeholder for future icon path
    };

    ProjectLauncherPanel();
    ~ProjectLauncherPanel() = default;

    // Called every ImGui frame while the launcher is active.
    void OnRender();

    // Fired when the user successfully picks / creates a project.
    // Callers should hide the launcher and initialise editor panels.
    std::function<void(std::filesystem::path /*projectFilePath*/)> OnOpenProject;
    std::function<void(std::string /*name*/, std::filesystem::path /*location*/)> OnCreateProject;

private:
    void RenderOpenTab();
    void RenderCreateTab();

    bool BrowseForProjectFile(std::filesystem::path& outPath);
    bool BrowseForFolder(std::filesystem::path& outPath);

    // ── Tab state ─────────────────────────────────────────────────────────────
    int m_ActiveTab = 0; // 0 = Open, 1 = Create

    // ── Create-tab state ──────────────────────────────────────────────────────
    char                  m_ProjectName[128] = "MyProject";
    std::filesystem::path m_ProjectLocation;
    std::string           m_LocationDisplay  = "No location selected";
    int                   m_SelectedTemplate = 0;
    std::string           m_ErrorMessage;

    std::vector<TemplateInfo> m_Templates;
};
