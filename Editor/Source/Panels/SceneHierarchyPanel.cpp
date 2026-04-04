// Copyright (c) CreationArtStudios

#include "SceneHierarchyPanel.h"

#include "Runtime/EngineCore/ECS/SceneSerializer.h"

#include <imgui.h>
#include <iostream>
#include <algorithm>

static char modelPathBuffer[512] = "";
static char texturePathBuffer[512] = "";
static char entityNameBuffer[128] = "NewEntity";

std::vector<std::string> SceneHierarchyPanel::GetModelFiles()
{
    std::vector<std::string> models;
    std::vector<std::filesystem::path> searchPaths = {
        "Engine/Content/Models",
        "Game/Content/Models",
        "../Engine/Content/Models",
        "../Game/Content/Models"
    };
    
    for (const auto& searchPath : searchPaths)
    {
        if (std::filesystem::exists(searchPath))
        {
            for (auto& entry : std::filesystem::directory_iterator(searchPath))
            {
                if (entry.is_regular_file())
                {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".glb" || ext == ".gltf" || ext == ".obj" || ext == ".fbx")
                    {
                        std::filesystem::path relPath = std::filesystem::relative(entry.path(), std::filesystem::current_path());
                        models.push_back(relPath.string());
                    }
                }
            }
        }
    }
    
    if (models.empty())
    {
        std::cerr << "[SceneHierarchyPanel] No model files found in any search path" << std::endl;
    }
    return models;
}

std::vector<std::string> SceneHierarchyPanel::GetTextureFiles()
{
    std::vector<std::string> textures;
    std::vector<std::filesystem::path> searchPaths = {
        "Engine/Content/Textures",
        "Game/Content/Textures",
        "../Engine/Content/Textures",
        "../Game/Content/Textures"
    };
    
    for (const auto& searchPath : searchPaths)
    {
        if (std::filesystem::exists(searchPath))
        {
            for (auto& entry : std::filesystem::directory_iterator(searchPath))
            {
                if (entry.is_regular_file())
                {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".ktx2")
                    {
                        std::filesystem::path relPath = std::filesystem::relative(entry.path(), std::filesystem::current_path());
                        textures.push_back(relPath.string());
                    }
                }
            }
        }
    }
    
    if (textures.empty())
    {
        std::cerr << "[SceneHierarchyPanel] No texture files found in any search path" << std::endl;
    }
    return textures;
}

void SceneHierarchyPanel::OnImGuiRender()
{
    if (!ScenePtr)
    {
        ImGui::Begin("Scene Hierarchy");
        ImGui::Text("No scene loaded");
        ImGui::End();
        return;
    }

    ImGui::Begin("Scene Hierarchy");

    ImGui::Text("Add Entity");
    ImGui::SameLine();
    if (ImGui::Button("+"))
    {
        SelectedEntity = ScenePtr->CreateEmptyEntity();
        if (OnReloadScene)
        {
            OnReloadScene();
        }
        if (OnEntitySelected)
        {
            OnEntitySelected(SelectedEntity);
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("-") && SelectedEntity != INVALID_ENTITY)
    {
        ScenePtr->GetInternalScene().DestroyEntity(SelectedEntity);
        SelectedEntity = INVALID_ENTITY;
        if (OnReloadScene)
        {
            OnReloadScene();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        std::filesystem::path defaultPath = "../Game/Content/Scenes/Default.ungscene";
        if (std::filesystem::exists(defaultPath))
        {
            LoadScene(defaultPath);
            if (OnReloadScene)
            {
                OnReloadScene();
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Entities");

    Scene& scene = ScenePtr->GetInternalScene();
    
    std::vector<EntityID> allEntities;
    auto view = scene.CreateView<>();
    for (EntityID entity : view)
    {
        allEntities.push_back(entity);
    }

    std::sort(allEntities.begin(), allEntities.end(), [](EntityID a, EntityID b) {
        return GetEntityIndex(a) < GetEntityIndex(b);
    });

    for (EntityID entity : allEntities)
    {
        MeshComponent* mesh = scene.GetComponent<MeshComponent>(entity);
        NameComponent* nameComp = scene.GetComponent<NameComponent>(entity);
        std::string name = "Entity " + std::to_string(GetEntityIndex(entity));
        
        if (nameComp && !nameComp->Name.empty())
        {
            name = nameComp->Name;
        }
        else if (mesh && !mesh->ModelPath.empty())
        {
            size_t pos = mesh->ModelPath.find_last_of("/\\");
            if (pos != std::string::npos)
            {
                name = mesh->ModelPath.substr(pos + 1);
            }
            else
            {
                name = mesh->ModelPath;
            }
        }

        bool isSelected = (entity == SelectedEntity);
        if (ImGui::Selectable(name.c_str(), isSelected))
        {
            SelectedEntity = entity;
            if (OnEntitySelected)
            {
                OnEntitySelected(entity);
            }
        }
    }

    ImGui::End();

    ImGui::Begin("Properties");
    
    if (SelectedEntity != INVALID_ENTITY && scene.IsEntityValid(SelectedEntity))
    {
        ImGui::Text("Entity ID: %llu", SelectedEntity);
        ImGui::Separator();

        MeshComponent* mesh = scene.GetComponent<MeshComponent>(SelectedEntity);
        MaterialComponent* material = scene.GetComponent<MaterialComponent>(SelectedEntity);

        if (ImGui::CollapsingHeader("Mesh Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static int selectedModelIndex = -1;
            std::vector<std::string> modelFiles = GetModelFiles();
            
            if (mesh)
            {
                char pathBuffer[512];
                strncpy_s(pathBuffer, mesh->ModelPath.c_str(), sizeof(pathBuffer) - 1);
                pathBuffer[sizeof(pathBuffer) - 1] = '\0';
                
                if (ImGui::InputText("Model Path", pathBuffer, sizeof(pathBuffer)))
                {
                    mesh->ModelPath = pathBuffer;
                }
                
                if (!modelFiles.empty())
                {
                    ImGui::Text("Or select from content:");
                    std::vector<const char*> modelNames;
                    for (const auto& m : modelFiles)
                    {
                        modelNames.push_back(m.c_str());
                    }
                    
                    if (ImGui::Combo("##ModelCombo", &selectedModelIndex, modelNames.data(), modelNames.size()))
                    {
                        if (selectedModelIndex >= 0 && selectedModelIndex < (int)modelFiles.size())
                        {
                            mesh->ModelPath = modelFiles[selectedModelIndex];
                            if (OnReloadScene)
                            {
                                OnReloadScene();
                            }
                        }
                    }
                }
            }
            else
            {
                ImGui::Text("(No mesh)");
                if (ImGui::Button("Add Mesh Component"))
                {
                    mesh = scene.AddComponent<MeshComponent>(SelectedEntity);
                    mesh->ModelPath = "";
                    if (OnReloadScene)
                    {
                        OnReloadScene();
                    }
                }
            }
        }

        if (ImGui::CollapsingHeader("Material Component", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static int selectedTextureIndex = -1;
            std::vector<std::string> textureFiles = GetTextureFiles();
            
            if (material)
            {
                char texBuffer[512];
                strncpy_s(texBuffer, material->TexturePath.c_str(), sizeof(texBuffer) - 1);
                texBuffer[sizeof(texBuffer) - 1] = '\0';
                
                if (ImGui::InputText("Texture Path", texBuffer, sizeof(texBuffer)))
                {
                    material->TexturePath = texBuffer;
                }
                
                if (!textureFiles.empty())
                {
                    ImGui::Text("Or select from content:");
                    std::vector<const char*> textureNames;
                    for (const auto& t : textureFiles)
                    {
                        textureNames.push_back(t.c_str());
                    }
                    
                    if (ImGui::Combo("##TextureCombo", &selectedTextureIndex, textureNames.data(), textureFiles.size()))
                    {
                        if (selectedTextureIndex >= 0 && selectedTextureIndex < (int)textureFiles.size())
                        {
                            material->TexturePath = textureFiles[selectedTextureIndex];
                            if (OnReloadScene)
                            {
                                OnReloadScene();
                            }
                        }
                    }
                }
            }
            else
            {
                ImGui::Text("(No material)");
                if (ImGui::Button("Add Material Component"))
                {
                    material = scene.AddComponent<MaterialComponent>(SelectedEntity);
                    material->TexturePath = "";
                }
            }
        }

        if (ImGui::Button("Delete Entity"))
        {
            Scene& scene = ScenePtr->GetInternalScene();
            scene.DestroyEntity(SelectedEntity);
            SelectedEntity = INVALID_ENTITY;
            if (OnReloadScene)
            {
                OnReloadScene();
            }
        }
        
        static bool needsReload = false;
        if (mesh && ImGui::IsItemEdited())
        {
            needsReload = true;
        }
        if (material && ImGui::IsItemEdited())
        {
            needsReload = true;
        }
        if (needsReload && OnReloadScene)
        {
            OnReloadScene();
            needsReload = false;
        }
    }
    else
    {
        ImGui::Text("Select an entity to view properties");
    }

    ImGui::End();
}

void SceneHierarchyPanel::LoadScene(const std::filesystem::path& filepath)
{
    if (!ScenePtr)
        return;

    CurrentScenePath = filepath;
    SceneSerializer serializer(ScenePtr);
    if (serializer.Load(filepath))
    {
        CurrentSceneName = serializer.GetSceneName();
        std::cout << "[SceneHierarchyPanel] Loaded scene from: " << filepath << " (" << CurrentSceneName << ")" << std::endl;
    }
}

void SceneHierarchyPanel::SaveScene(const std::filesystem::path& filepath)
{
    if (!ScenePtr)
        return;

    CurrentScenePath = filepath;
    SceneSerializer serializer(ScenePtr);
    serializer.SetSceneName(CurrentSceneName);
    if (serializer.Save(filepath))
    {
        CurrentSceneName = serializer.GetSceneName();
        std::cout << "[SceneHierarchyPanel] Saved scene to: " << filepath << " (" << CurrentSceneName << ")" << std::endl;
    }
}

void SceneHierarchyPanel::SaveCurrentScene()
{
    if (!ScenePtr || CurrentScenePath.empty())
        return;

    SceneSerializer serializer(ScenePtr);
    serializer.SetSceneName(CurrentSceneName);
    if (serializer.Save(CurrentScenePath))
    {
        std::cout << "[SceneHierarchyPanel] Saved current scene: " << CurrentScenePath << std::endl;
    }
}
