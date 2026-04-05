#include "EditorLayer.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <GLFW/glfw3.h>
#define WIN32_LEAN_AND_MEAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <windows.h>
#include <commdlg.h>

#include "../../Engine/Source/Runtime/EngineCore/Application.h"
#include "../../Engine/Source/Runtime/Project/Project.h"
#include "Panels/ContentBrowserPanel.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/PerformancePanel.h"
#include "Panels/ProjectLauncherPanel.h"

EditorLayer::EditorLayer()  = default;
EditorLayer::~EditorLayer() = default;

// ─────────────────────────────────────────────────────────────────────────────
//  Layer lifecycle
// ─────────────────────────────────────────────────────────────────────────────
void EditorLayer::OnAttach()
{
    m_Engine = Application::Get().GetGameEngine();

    auto& args = m_Engine->GetCommandLineArgs();
    if (args.Count > 1)
    {
        std::filesystem::path projectPath = args.Args[1];
        if (OpenProject(projectPath))
        {
            InitializeEditorPanels();
            return;
        }
        std::cout << "[EditorLayer] Command-line project invalid, showing launcher." << std::endl;
    }

    ShowProjectLauncher();
}

void EditorLayer::OnDetach() {}
void EditorLayer::OnUpdate(float) {}
void EditorLayer::OnRender() {}

// ─────────────────────────────────────────────────────────────────────────────
//  Project-launcher overlay
// ─────────────────────────────────────────────────────────────────────────────
void EditorLayer::ShowProjectLauncher()
{
    auto launcher = std::make_unique<ProjectLauncherPanel>();

    // ── IMPORTANT – call order in both lambdas ─────────────────────────────────
    //
    // The lambda's closure (which holds the captured 'this') is stored inside
    // the ProjectLauncherPanel's OnOpenProject / OnCreateProject std::function.
    // Calling SetProjectLauncher(nullptr) destroys the panel, which destroys
    // the std::function, which destroys the closure.
    //
    // MSVC re-reads captured values from the closure object on every access in
    // debug builds.  If the closure has already been freed (0xDD fill pattern),
    // 'this' reads back as 0xDDDDDDDDDDDDDDDD → crash in InitializeEditorPanels.
    //
    // Fix: finish ALL work that needs 'this' (InitializeEditorPanels) BEFORE
    // destroying the closure (SetProjectLauncher(nullptr)).
    // ──────────────────────────────────────────────────────────────────────────

    launcher->OnOpenProject = [this](std::filesystem::path path)
    {
        if (OpenProject(path))
        {
            InitializeEditorPanels();                                          // 1. use 'this' first
            m_Engine->GetRenderer()->GetImGuiSystem()->SetProjectLauncher(nullptr); // 2. then dismiss
        }
    };

    launcher->OnCreateProject = [this](std::string name, std::filesystem::path location)
    {
        std::filesystem::path projectFile = CreateNewProject(name, location);
        if (!projectFile.empty() && OpenProject(projectFile))
        {
            InitializeEditorPanels();                                          // 1. use 'this' first
            m_Engine->GetRenderer()->GetImGuiSystem()->SetProjectLauncher(nullptr); // 2. then dismiss
        }
    };

    m_Engine->GetRenderer()->GetImGuiSystem()->SetProjectLauncher(std::move(launcher));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Editor-panel initialisation (runs once a project is loaded)
// ─────────────────────────────────────────────────────────────────────────────
void EditorLayer::InitializeEditorPanels()
{
    auto contentBrowser = std::make_unique<ContentBrowserPanel>(m_Engine->GetRenderer());
    contentBrowser->SetBaseDirectory(Project::GetActive()->GetAssetDirectory());

    contentBrowser->OnLoadScene = [this](std::filesystem::path path)
    {
        auto* sh = m_Engine->GetRenderer()->GetImGuiSystem()->GetSceneHierarchy();
        if (sh)
        {
            sh->LoadScene(path);
            m_Engine->GetRenderer()->ReloadSceneData();
            m_Engine->GetRenderer()->GetImGuiSystem()->SetCurrentSceneName(sh->GetSceneName());
        }
    };

    contentBrowser->OnSaveScene = [this](std::filesystem::path path)
    {
        auto* sh = m_Engine->GetRenderer()->GetImGuiSystem()->GetSceneHierarchy();
        if (sh) sh->SaveScene(path);
    };

    m_Engine->GetRenderer()->GetImGuiSystem()->SetContentBrowser(std::move(contentBrowser));

    auto sceneHierarchy = std::make_unique<SceneHierarchyPanel>();
    sceneHierarchy->SetScene(&m_Engine->GetRenderer()->GetScene());
    sceneHierarchy->OnReloadScene   = [this]() { m_Engine->GetRenderer()->ReloadSceneData(); };
    sceneHierarchy->OnEntitySelected = [](EntityID entity)
    {
        std::cout << "[EditorLayer] Entity selected: " << entity << std::endl;
    };

    m_Engine->GetRenderer()->GetImGuiSystem()->SetSceneHierarchy(std::move(sceneHierarchy));

    auto performancePanel = std::make_unique<PerformancePanel>(m_Engine->GetRenderer());
    m_Engine->GetRenderer()->GetImGuiSystem()->SetPerformancePanel(std::move(performancePanel));

    m_Engine->GetRenderer()->ReloadSceneData();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Open project (native file dialog)
// ─────────────────────────────────────────────────────────────────────────────
bool EditorLayer::OpenProject()
{
    OPENFILENAMEA ofn   = {};
    CHAR szFile[260]    = {};
    ofn.lStructSize     = sizeof(OPENFILENAME);
    ofn.lpstrFile       = szFile;
    ofn.nMaxFile        = sizeof(szFile);
    ofn.lpstrFilter     = "UniGen Project (*.ungproject)\0*.ungproject\0";
    ofn.nFilterIndex    = 1;
    ofn.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    GLFWwindow* glfwWin = static_cast<GLFWwindow*>(m_Engine->GetWindow()->getGLFWwindow());
    ofn.hwndOwner       = glfwGetWin32Window(glfwWin);

    if (GetOpenFileNameA(&ofn))
        return OpenProject(std::filesystem::path(szFile));

    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Open project (by path)
// ─────────────────────────────────────────────────────────────────────────────
bool EditorLayer::OpenProject(const std::filesystem::path& path)
{
    if (!Project::Load(path))
        return false;

    auto& config = Project::GetActive()->GetConfig();
    std::string t = config.Name + " - UniGen Editor";
    m_Engine->GetWindow()->SetTitle(t.c_str());
    std::cout << "[EditorLayer] Loaded project: " << config.Name << std::endl;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Create new project from blank template
// ─────────────────────────────────────────────────────────────────────────────
std::filesystem::path EditorLayer::CreateNewProject(const std::string&           name,
                                                    const std::filesystem::path& location)
{
    namespace fs = std::filesystem;
    fs::path projectDir = location / name;

    try
    {
        fs::create_directories(projectDir);
        fs::create_directories(projectDir / "Content");
        fs::create_directories(projectDir / "Content" / "Scenes");
        fs::create_directories(projectDir / "Content" / "Models");
        fs::create_directories(projectDir / "Content" / "Textures");
        fs::create_directories(projectDir / "Source");

        fs::path projectFile = projectDir / (name + ".ungproject");
        {
            std::ofstream f(projectFile);
            if (!f.is_open()) throw std::runtime_error("Cannot write project file.");
            f << "Project:\n"
              << "  Name: " << name << "\n"
              << "  StartScene: \"Content/Scenes/Main.ungscene\"\n"
              << "  AssetDirectory: \"Content\"\n";
        }
        {
            std::ofstream f(projectDir / "Content" / "Scenes" / "Main.ungscene");
            f << "Scene:\n  Name: Main\n  Entities: []\n";
        }
        {
            std::ofstream f(projectDir / "Source" / "GameApp.cpp");
            f << "// " << name << " - GameApp\n"
              << "#include \"Runtime/EngineCore/Application.h\"\n\n"
              << "class " << name << "App : public Application {};\n\n"
              << "Application* CreateApplication(int argc, char** argv)\n"
              << "{\n    return new " << name << "App();\n}\n";
        }

        std::cout << "[EditorLayer] Created project '" << name << "' at " << projectDir << std::endl;
        return projectFile;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[EditorLayer] Failed to create project: " << e.what() << std::endl;
        try { fs::remove_all(projectDir); } catch (...) {}
        return {};
    }
}
