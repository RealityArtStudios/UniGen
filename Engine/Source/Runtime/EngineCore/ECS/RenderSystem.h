// Copyright (c) CreationArtStudios

#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <string>

#include "ECS.h"
#include "Components.h"

class Renderer;
class BufferManager;
class TextureManager;
class CommandPool;
class PipelineManager;
class DescriptorManager;
class Mesh;

class RenderSystem
{
public:
    struct MeshRenderData
    {
        std::unique_ptr<Mesh> MeshData;
        std::unique_ptr<TextureManager> TextureManager;
        
        vk::raii::Buffer VertexBuffer = nullptr;
        vk::raii::DeviceMemory VertexBufferMemory = nullptr;
        vk::raii::Buffer IndexBuffer = nullptr;
        vk::raii::DeviceMemory IndexBufferMemory = nullptr;
        
        vk::raii::DescriptorPool DescriptorPool = nullptr;
        std::vector<vk::raii::DescriptorSet> DescriptorSets;
        std::vector<vk::raii::Buffer> UniformBuffers;
        std::vector<vk::raii::DeviceMemory> UniformBuffersMemory;
        std::vector<void*> UniformBuffersMapped;
    };

    RenderSystem(Renderer* renderer);
    ~RenderSystem();

    void Initialize();
    void Shutdown();
    void Render();

    MeshRenderData* GetOrLoadMesh(const std::string& modelPath, const std::string& texturePath);
    void RenderMesh(MeshRenderData* meshData);

    Renderer* GetRenderer() const { return RendererPtr; }

private:
    Renderer* RendererPtr = nullptr;
    std::unordered_map<std::string, std::unique_ptr<MeshRenderData>> LoadedMeshes;
};
