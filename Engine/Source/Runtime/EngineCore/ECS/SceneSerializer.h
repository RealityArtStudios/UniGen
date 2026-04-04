// Copyright (c) CreationArtStudios

#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <iostream>

#include "ECS.h"
#include "Scene.h"
#include "Components.h"

class SceneSerializer
{
public:
    SceneSerializer(EngineScene* scene) : ScenePtr(scene) {}

    bool Load(const std::filesystem::path& filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            std::cerr << "[SceneSerializer] File not found: " << filepath << std::endl;
            return false;
        }

        std::ifstream file(filepath);
        if (!file.is_open())
        {
            std::cerr << "[SceneSerializer] Could not open file: " << filepath << std::endl;
            return false;
        }
        
        Scene& scene = ScenePtr->GetInternalScene();
        std::vector<EntityID> entitiesToDestroy;
        auto view = scene.CreateView<>();
        for (EntityID entity : view)
        {
            entitiesToDestroy.push_back(entity);
        }
        for (EntityID entity : entitiesToDestroy)
        {
            scene.DestroyEntity(entity);
        }

        std::string line;
        std::string currentSection;
        std::string currentModelPath;
        std::string currentTexturePath;
        std::string currentEntityName;
        uint64_t currentEntityID = 0;

        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '#')
                continue;

            // Trim leading spaces for checking
            std::string trimmedLine = line;
            while (!trimmedLine.empty() && trimmedLine[0] == ' ')
                trimmedLine.erase(trimmedLine.begin());

            // Check for YAML list item (- )
            if (trimmedLine.size() >= 2 && trimmedLine[0] == '-' && trimmedLine[1] == ' ')
            {
                if (currentSection == "Entities" && !currentModelPath.empty())
                {
                    ScenePtr->CreateEntityWithMesh(currentModelPath, currentTexturePath, currentEntityName, currentEntityID);
                    currentModelPath = "";
                    currentTexturePath = "";
                    currentEntityName = "";
                    currentEntityID = 0;
                }
                continue;
            }

            if (line.back() == ':')
            {
                if (currentSection == "Entities" && !currentModelPath.empty())
                {
                    ScenePtr->CreateEntityWithMesh(currentModelPath, currentTexturePath, currentEntityName, currentEntityID);
                }
                currentSection = trim(line.substr(0, line.size() - 1));
                currentModelPath = "";
                currentTexturePath = "";
                currentEntityName = "";
                currentEntityID = 0;
                continue;
            }

            size_t colonPos = line.find(':');
            if (colonPos == std::string::npos)
                continue;

            std::string key = trim(line.substr(0, colonPos));
            std::string value = trim(line.substr(colonPos + 1));

            std::string trimmedKey = key;
            while (!trimmedKey.empty() && trimmedKey[0] == ' ')
                trimmedKey.erase(trimmedKey.begin());
            while (!trimmedKey.empty() && trimmedKey.back() == ' ')
                trimmedKey.pop_back();

            if (trimmedKey == "Name" && currentSection == "Scene")
            {
                SceneName = value;
            }
            else if (trimmedKey == "EntityID" && currentSection == "Entities")
            {
                currentEntityID = std::stoull(value);
            }
            else if (trimmedKey == "ModelPath" && currentSection == "Entities")
            {
                currentModelPath = value;
            }
            else if (trimmedKey == "TexturePath" && currentSection == "Entities")
            {
                currentTexturePath = value;
            }
        }
        
        if (!currentModelPath.empty())
        {
            ScenePtr->CreateEntityWithMesh(currentModelPath, currentTexturePath, currentEntityName, currentEntityID);
        }

        file.close();
        std::cout << "[SceneSerializer] Loaded scene: " << SceneName << " from " << filepath << std::endl;
        return true;
    }

    bool Save(const std::filesystem::path& filepath)
    {
        std::ofstream file(filepath);
        if (!file.is_open())
        {
            std::cerr << "[SceneSerializer] Could not create file: " << filepath << std::endl;
            return false;
        }

        file << "Scene:\n";
        file << "  Name: " << SceneName << "\n";
        file << "  Entities:\n";

        Scene& scene = ScenePtr->GetInternalScene();
        auto view = scene.CreateView<MeshComponent, MaterialComponent>();

        for (EntityID entity : view)
        {
            MeshComponent* mesh = scene.GetComponent<MeshComponent>(entity);
            MaterialComponent* material = scene.GetComponent<MaterialComponent>(entity);
            NameComponent* nameComp = scene.GetComponent<NameComponent>(entity);

            if (mesh)
            {
                std::string modelPath = mesh->ModelPath;
                std::string texturePath = material ? material->TexturePath : "";
                uint64_t entityId = nameComp ? nameComp->EntityID : GetEntityIndex(entity);

                file << "    - EntityID: " << entityId << "\n";
                file << "      ModelPath: " << modelPath << "\n";
                file << "      TexturePath: " << texturePath << "\n";
            }
        }

        file.close();
        std::cout << "[SceneSerializer] Saved scene: " << SceneName << " to " << filepath << std::endl;
        return true;
    }

    void SetSceneName(const std::string& name) { SceneName = name; }
    std::string GetSceneName() const { return SceneName; }

private:
    static std::string trim(const std::string& str)
    {
        size_t start = str.find_first_not_of(" \t");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t");
        return str.substr(start, end - start + 1);
    }

    EngineScene* ScenePtr = nullptr;
    std::string SceneName = "Untitled";
};
