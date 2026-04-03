#include "Runtime/EngineCore/Application.h"

#include "EditorLayer.h"



	class EditorApp : public Application
	{
	public:
		EditorApp(const ApplicationSpecification& spec)
			: Application(spec)
		{
			PushLayer(new EditorLayer());
		}
	};

	Application* CreateApplication(int argc, char** argv)
	{
		Application::ApplicationSpecification spec;
		spec.Name = "UniGen Editor";
		spec.WorkingDirectory = ".";
		spec.Width = 1920;
		spec.Height = 1080;
		
		return new EditorApp(spec);
	}