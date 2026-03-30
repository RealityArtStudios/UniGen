// Copyright (c) CreationArtStudios, Khairol Anwar

#include "Runtime/EngineCore/GameEngine.h"

GameEngine::GameEngine()
{
    Initialize();
}

GameEngine::~GameEngine()
{
    Cleanup();
}

void GameEngine::Run()
{
    MainLoop();
    Cleanup();
}

void GameEngine::Initialize()
{
    m_Window = new Window("CreationArtEngine", 1920, 1080);

    std::cout << "Initializing GameEngine..." << std::endl;
}

void GameEngine::MainLoop()
{
    while (bIsRunning && !m_Window->closed()) {
        m_Window->Update();
        
        OnUpdate(m_Window->deltaTime);
        OnRender(m_Window->deltaTime);
    }
}

void GameEngine::Shutdown()
{
    bIsRunning = false;
}

void GameEngine::Cleanup()
{
    delete m_Window;
    m_Window = nullptr;
}
