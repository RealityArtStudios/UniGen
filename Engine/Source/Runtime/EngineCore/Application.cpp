// Copyright (c) CreationArtStudios, Khairol Anwar

#include "Runtime/EngineCore/Application.h"

Application::Application(const ApplicationSpecification& spec)
    : m_Specification(spec)
{
    std::cout << "Initializing application: " << spec.Name << std::endl;
    m_GameEngine = std::make_unique<GameEngine>(spec.Width, spec.Height, 0, nullptr);
    s_Instance = this;
}

Application::~Application()
{
}

void Application::Run()
{
    for (Layer* layer : m_LayerStack)
    {
        layer->OnAttach();
        m_GameEngine->PushLayerDirect(layer);
    }
    
    m_GameEngine->Run();
}

void Application::PushLayer(Layer* layer)
{
    m_LayerStack.push_back(layer);
}

void Application::PushOverlay(Layer* overlay)
{
    m_LayerStack.push_back(overlay);
}