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

	bool OpenProject();
	bool OpenProject(const std::filesystem::path& path);

private:
	GameEngine* m_Engine = nullptr;
};