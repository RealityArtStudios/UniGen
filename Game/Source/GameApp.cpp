// Copyright (c) CreationArtStudios, Khairol Anwar

#include "Runtime/EngineCore/Application.h"
#include "Runtime/EngineCore/ECS/SceneSerializer.h"

class GameApp : public Application
{
public:
	GameApp()
	{
		LoadDefaultScene();
	}

	virtual ~GameApp()
	{
		// Cleanup your game here
	}

private:
    void LoadDefaultScene()
    {
        // Path is relative to the working directory set in the spec.
        // Game/Content/Sponza.ungscene references
        //   ..\Engine\Content\Models\Sponza\Sponza.gltf
        // so the exe must run from the repo root (or Game/) — adjust as needed.
        constexpr const char* SCENE_PATH = "Content/Sponza.ungscene";

        EngineScene* scene = &m_GameEngine->GetRenderer()->GetScene();
        SceneSerializer serializer(scene);

        if (serializer.Load(SCENE_PATH))
        {
            m_GameEngine->GetRenderer()->ReloadSceneData();
            std::cout << "[GameApp] Loaded scene: " << SCENE_PATH << std::endl;
        }
        else
        {
            std::cerr << "[GameApp] Failed to load scene: " << SCENE_PATH << std::endl;
        }
    }
protected:
	// Override any Applications methods if needed
	void Run() override
	{
		// Your game run logic here
		Application::Run();
	}
};

Application* CreateApplication(int argc, char** argv)
{
	return new GameApp();
}