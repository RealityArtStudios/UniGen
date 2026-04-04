// Copyright (c) CreationArtStudios

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>

#include <filesystem>

#include "Runtime/EngineCore/ECS/ECS.h"
#include "Runtime/EngineCore/ECS/Scene.h"
#include "Runtime/EngineCore/ECS/Components.h"

class SceneHierarchyPanel
{
public:
    SceneHierarchyPanel() = default;
    ~SceneHierarchyPanel() = default;

    void SetScene(EngineScene* scene) { ScenePtr = scene; }
    void SetSelectedEntity(EntityID entity) { SelectedEntity = entity; }
    EntityID GetSelectedEntity() const { return SelectedEntity; }
    void OnImGuiRender();
    void LoadScene(const std::filesystem::path& filepath);
    void SaveScene(const std::filesystem::path& filepath);
    void SaveCurrentScene();

    static std::vector<std::string> GetModelFiles();
    static std::vector<std::string> GetTextureFiles();

    std::function<void()> OnReloadScene;
    std::function<void(EntityID)> OnEntitySelected;

    std::string GetSceneName() const { return CurrentSceneName; }
    void SetSceneName(const std::string& name) { CurrentSceneName = name; }
    std::filesystem::path GetCurrentScenePath() const { return CurrentScenePath; }

private:
    EngineScene* ScenePtr = nullptr;
    EntityID SelectedEntity = INVALID_ENTITY;
    std::string entityNameBuffer = "Entity";
    std::string CurrentSceneName = "Untitled";
    std::filesystem::path CurrentScenePath;
};
