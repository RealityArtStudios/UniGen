// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once
#include <vector>
#include <array>
#include <memory>
#include <string>
#include <cstring>
#include <iostream>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan.hpp>
#else
import vulkan_hpp;
#endif

#include <tiny_gltf.h>
#include "Material.h"

class Mesh
{
public:
	// ── Vertex layout ─────────────────────────────────────────────────────────
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec2 texCoord;

		static vk::VertexInputBindingDescription GetBindingDescription()
		{
			return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
		}

		static std::array<vk::VertexInputAttributeDescription, 3> GetAttributeDescriptions()
		{
			return {
				vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
				vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
				vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat,    offsetof(Vertex, texCoord))
			};
		}

		bool operator==(const Vertex& other) const
		{
			return pos == other.pos && color == other.color && texCoord == other.texCoord;
		}
	};

	// ── Uniform buffer ────────────────────────────────────────────────────────
	struct UniformBufferObject
	{
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 proj;
	};

	// ── Sub-mesh: a slice of the merged index buffer with a material index ────
	struct SubMesh
	{
		uint32_t FirstIndex    = 0;   // byte offset into the merged index buffer
		uint32_t IndexCount    = 0;   // number of indices for this draw call
		int      MaterialIndex = -1;  // index into Materials[]; -1 = use fallback
	};

	// ── Construction ──────────────────────────────────────────────────────────
	Mesh() = default;
	explicit Mesh(const std::string& modelPath);

	void LoadFromGLTF(const std::string& filepath);

	// ── Geometry accessors ────────────────────────────────────────────────────
	const std::vector<Vertex>&   GetVertices()   const { return Vertices; }
	const std::vector<uint32_t>& GetIndices()    const { return Indices; }
	uint32_t                     GetVertexCount() const { return static_cast<uint32_t>(Vertices.size()); }
	uint32_t                     GetIndexCount()  const { return static_cast<uint32_t>(Indices.size()); }

	// ── Sub-mesh / material accessors ─────────────────────────────────────────
	const std::vector<SubMesh>&      GetSubMeshes() const { return SubMeshes; }
	const std::vector<GltfMaterial>& GetMaterials() const { return Materials; }

	// ── Vulkan input descriptions ─────────────────────────────────────────────
	vk::VertexInputBindingDescription                  GetBindingDescription()    const { return Vertex::GetBindingDescription(); }
	std::array<vk::VertexInputAttributeDescription, 3> GetAttributeDescriptions() const { return Vertex::GetAttributeDescriptions(); }

private:
	std::vector<Vertex>       Vertices;
	std::vector<uint32_t>     Indices;
	std::vector<SubMesh>      SubMeshes;
	std::vector<GltfMaterial> Materials;
};

template <>
struct std::hash<Mesh::Vertex>
{
	size_t operator()(Mesh::Vertex const& vertex) const noexcept
	{
		return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1)
		     ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
	}
};
