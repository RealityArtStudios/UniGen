#include "EditorLayer.h"

#include <filesystem>

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

#include <iostream>

EditorLayer::EditorLayer()
{
}

EditorLayer::~EditorLayer()
{
}

void EditorLayer::OnAttach()
{
	m_Engine = Application::Get().GetGameEngine();
	
	auto& args = m_Engine->GetCommandLineArgs();
	if (args.Count > 1)
	{
		std::filesystem::path projectPath = args.Args[1];
		OpenProject(projectPath);
	}
	else
	{
		if (!OpenProject())
		{
			std::cout << "No project selected, closing editor..." << std::endl;
			m_Engine->Stop();
		}
	}

	auto contentBrowser = std::make_unique<ContentBrowserPanel>(m_Engine->GetRenderer());
	contentBrowser->SetBaseDirectory(Project::GetActive()->GetAssetDirectory());
	contentBrowser->OnLoadScene = [this](std::filesystem::path path) {
		auto sceneHierarchy = m_Engine->GetRenderer()->GetImGuiSystem()->GetSceneHierarchy();
		if (sceneHierarchy)
		{
			sceneHierarchy->LoadScene(path);
			m_Engine->GetRenderer()->ReloadSceneData();
			m_Engine->GetRenderer()->GetImGuiSystem()->SetCurrentSceneName(sceneHierarchy->GetSceneName());
		}
	};
	contentBrowser->OnSaveScene = [this](std::filesystem::path path) {
		auto sceneHierarchy = m_Engine->GetRenderer()->GetImGuiSystem()->GetSceneHierarchy();
		if (sceneHierarchy)
		{
			sceneHierarchy->SaveScene(path);
		}
	};
	m_Engine->GetRenderer()->GetImGuiSystem()->SetContentBrowser(std::move(contentBrowser));

	auto sceneHierarchy = std::make_unique<SceneHierarchyPanel>();
	sceneHierarchy->SetScene(&m_Engine->GetRenderer()->GetScene());
	sceneHierarchy->OnReloadScene = [this]() {
		m_Engine->GetRenderer()->ReloadSceneData();
	};
	sceneHierarchy->OnEntitySelected = [this](EntityID entity) {
		std::cout << "[EditorLayer] Entity selected: " << entity << std::endl;
	};
	m_Engine->GetRenderer()->GetImGuiSystem()->SetSceneHierarchy(std::move(sceneHierarchy));
	
	m_Engine->GetRenderer()->ReloadSceneData();
}

void EditorLayer::OnDetach()
{
}

void EditorLayer::OnUpdate(float deltaTime)
{
}

void EditorLayer::OnRender()
{
}

bool EditorLayer::OpenProject()
{
	OPENFILENAMEA ofn;
	CHAR szFile[260] = { 0 };
	ZeroMemory(&ofn, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	GLFWwindow* glfwWindow = (GLFWwindow*)m_Engine->GetWindow()->getGLFWwindow();
	ofn.hwndOwner = glfwGetWin32Window(glfwWindow);
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = "UniGen Project (*.ungproject)\0*.ungproject\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameA(&ofn) == TRUE)
	{
		return OpenProject(std::filesystem::path(szFile));
	}

	return false;
}

bool EditorLayer::OpenProject(const std::filesystem::path& path)
{
	if (Project::Load(path))
	{
		auto& config = Project::GetActive()->GetConfig();
		std::string title = config.Name + " - UniGen Editor";
		m_Engine->GetWindow()->SetTitle(title.c_str());
		std::cout << "Loaded project: " << config.Name << std::endl;
		return true;
	}
	return false;
}