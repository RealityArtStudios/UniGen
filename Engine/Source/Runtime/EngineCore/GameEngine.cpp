// Copyright (c) CreationArtStudios, Khairol Anwar

#include "Runtime/EngineCore/GameEngine.h"

#include <iostream>

GameEngine::GameEngine(int width, int height, int argc, char** argv)
    : m_WindowWidth(width)
    , m_WindowHeight(height)
{
    m_CommandLineArgs.Count = argc;
    m_CommandLineArgs.Args = argv;
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
    int width = m_WindowWidth > 0 ? m_WindowWidth : 1280;
    int height = m_WindowHeight > 0 ? m_WindowHeight : 720;
    m_Window = new Window("UniGen Editor", width, height);
    m_Renderer = std::make_unique<Renderer>(m_Window);

    std::cout << "Initializing GameEngine..." << std::endl;
    m_Renderer->Initialize();
}

void GameEngine::MainLoop()
{
    while (bIsRunning && !m_Window->closed()) {
        m_Window->Update();
        
        float deltaTime = m_Window->deltaTime;
        
        UpdateLayers(deltaTime);
        OnUpdate(deltaTime);
        
        m_Renderer->Render();
        
        RenderLayers();
        OnRender(deltaTime);
    }
    m_Renderer->GetVulkanInstance()->GetLogicalDevice().waitIdle();
}

void GameEngine::Shutdown()
{
    bIsRunning = false;
}

void GameEngine::Cleanup()
{
    m_Renderer->GetImGuiSystem()->Cleanup();
    m_Renderer->Shutdown();
    delete m_Window;
    m_Window = nullptr;
}

void GameEngine::PushLayer(Layer* layer)
{
    m_LayerStack.push_back(layer);
}

void GameEngine::PushLayerDirect(Layer* layer)
{
    m_LayerStack.push_back(layer);
}

void GameEngine::UpdateLayers(float deltaTime)
{
    for (Layer* layer : m_LayerStack)
    {
        layer->OnUpdate(deltaTime);
    }
}

void GameEngine::RenderLayers()
{
    for (Layer* layer : m_LayerStack)
    {
        layer->OnRender();
    }
}
