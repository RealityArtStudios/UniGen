#pragma once

#include "Runtime/EngineCore/Application.h"
#include "Runtime/EngineCore/GameEngine.h"

class EditorLayer : public Layer
{
public:
    EditorLayer();
    ~EditorLayer();

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;

    // ── Project helpers ────────────────────────────────────────────────────────
    // Open an existing .ungproject.
    // Returns true on success; false if the file was invalid or the dialog was
    // cancelled.
    bool OpenProject();
    bool OpenProject(const std::filesystem::path& path);

    // Create a new project from the blank template at location/name/, then open
    // it.  Returns the path to the generated .ungproject file, or an empty path
    // on failure.
    std::filesystem::path CreateNewProject(const std::string& name,
                                           const std::filesystem::path& location);

private:
    // ── Startup flow ──────────────────────────────────────────────────────────
    // Show the project-launcher overlay (no project loaded yet).
    void ShowProjectLauncher();

    // Wire up all editor panels – called once a valid project is active.
    void InitializeEditorPanels();

    GameEngine* m_Engine = nullptr;
};
