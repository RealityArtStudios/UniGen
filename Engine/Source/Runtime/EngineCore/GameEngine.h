// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#include <memory>
#include <vector>
#include "Window.h"
#include "Renderer/Renderer.h"

class Layer
{
public:
    virtual ~Layer() = default;
    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender() {}
};

struct GameEngineCommandLineArgs
{
    int Count = 0;
    char** Args = nullptr;

    const char* operator[](int index) const
    {
        return Args[index];
    }
};

struct GameEngine
{
public:
    GameEngine(int width = 1280, int height = 720, int argc = 0, char** argv = nullptr);
    ~GameEngine();

    void Run();
    void Shutdown();

    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender(float deltaTime) {}

    bool IsRunning() const { return bIsRunning; }
    void Stop() { bIsRunning = false; }

    Window* GetWindow() const { return m_Window; }
    Renderer* GetRenderer() const { return m_Renderer.get(); }
    const GameEngineCommandLineArgs& GetCommandLineArgs() const { return m_CommandLineArgs; }

    void PushLayer(Layer* layer);
    void PushLayerDirect(Layer* layer);
    void UpdateLayers(float deltaTime);
    void RenderLayers();

private:
    void Initialize();
    void MainLoop();
    void Cleanup();

    bool bIsRunning = true;
    Window* m_Window = nullptr;
    int m_WindowWidth = 1280;
    int m_WindowHeight = 720;
    std::unique_ptr<Renderer> m_Renderer;
    std::vector<Layer*> m_LayerStack;
    GameEngineCommandLineArgs m_CommandLineArgs;
};
