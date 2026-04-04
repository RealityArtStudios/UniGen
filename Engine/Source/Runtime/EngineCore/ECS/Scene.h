// Copyright (c) CreationArtStudios

#pragma once
#include "ECS.h"
#include "Components.h"

class EngineScene
{
public:
    EngineScene() = default;

    EntityID CreateEmptyEntity()
    {
        return InternalScene.CreateEntity();
    }

    EntityID CreateEntityWithMesh(const std::string& modelPath, const std::string& texturePath, const std::string& name = "", uint64_t entityId = 0)
    {
        EntityID entity = InternalScene.CreateEntity();
        
        MeshComponent* mesh = InternalScene.AddComponent<MeshComponent>(entity);
        mesh->ModelPath = modelPath;
        
        MaterialComponent* material = InternalScene.AddComponent<MaterialComponent>(entity);
        material->TexturePath = texturePath;
        
        if (!name.empty() || entityId != 0)
        {
            NameComponent* nameComp = InternalScene.AddComponent<NameComponent>(entity);
            nameComp->Name = name;
            nameComp->EntityID = entityId != 0 ? entityId : GetEntityIndex(entity);
        }
        
        return entity;
    }

    Scene& GetInternalScene() { return InternalScene; }

private:
    Scene InternalScene;
};
