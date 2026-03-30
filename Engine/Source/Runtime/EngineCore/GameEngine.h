// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#include <memory>
#include "Window.h"

struct GameEngine
{
public:
    GameEngine();
    ~GameEngine();

    void Run();
    void Shutdown();

    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender(float deltaTime) {}

    bool IsRunning() const { return bIsRunning; }
    void Stop() { bIsRunning = false; }

    Window* GetWindow() const { return m_Window; }

private:
    void Initialize();
    void MainLoop();
    void Cleanup();

    bool bIsRunning = true;
    Window* m_Window = nullptr;
};
