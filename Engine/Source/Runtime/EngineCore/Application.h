// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#include <memory>
#include <vector>
#include <string>
#include "GameEngine.h"

class Application
{
public:
    struct ApplicationSpecification
    {
        std::string Name = "UniGen Application";
        std::string WorkingDirectory = ".";
        std::string ProjectPath = "";
        int Width = 0;
        int Height = 0;
    };

	Application(const ApplicationSpecification& spec = ApplicationSpecification());
	virtual ~Application();

    virtual void Run();

    void PushLayer(Layer* layer);
    void PushOverlay(Layer* overlay);

    GameEngine* GetGameEngine() const { return m_GameEngine.get(); }

    static Application& Get() { return *s_Instance; }

protected:
    std::unique_ptr<GameEngine> m_GameEngine;
    std::vector<Layer*> m_LayerStack;
    ApplicationSpecification m_Specification;

    inline static Application* s_Instance = nullptr;
};
