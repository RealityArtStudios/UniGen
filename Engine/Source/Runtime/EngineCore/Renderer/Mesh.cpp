// Copyright (c) CreationArtStudios, Khairol Anwar

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "Mesh.h"
#include <filesystem>

Mesh::Mesh(const std::string& modelPath)
{
	LoadFromGLTF(modelPath);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

// Resolve a GLTF texture index to an absolute file path.
// Returns empty string when the texture / image is absent or has no URI.
static std::string ResolveTexturePath(
	const tinygltf::Model& model,
	int                    texIdx,
	const std::string&     gltfDir)
{
	if (texIdx < 0 || texIdx >= static_cast<int>(model.textures.size()))
		return {};

	int imgIdx = model.textures[texIdx].source;
	if (imgIdx < 0 || imgIdx >= static_cast<int>(model.images.size()))
		return {};

	const std::string& uri = model.images[imgIdx].uri;
	if (uri.empty())
		return {};

	// tinygltf may URL-encode spaces etc.; use filesystem to normalise
	return (std::filesystem::path(gltfDir) / uri).string();
}

// ─────────────────────────────────────────────────────────────────────────────
void Mesh::LoadFromGLTF(const std::string& filepath)
{
	tinygltf::Model    model;
	tinygltf::TinyGLTF loader;
	std::string        err, warn;

	bool ret = false;
	if (filepath.size() >= 4 && filepath.substr(filepath.size() - 4) == ".glb")
		ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
	else
		ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);

	if (!warn.empty()) std::cout << "[Mesh] glTF warning: " << warn << std::endl;
	if (!err.empty())  std::cout << "[Mesh] glTF error: "   << err  << std::endl;
	if (!ret)          throw std::runtime_error("Failed to load glTF: " + filepath);

	Vertices.clear();
	Indices.clear();
	SubMeshes.clear();
	Materials.clear();

	// ── 1. Extract materials ──────────────────────────────────────────────────
	const std::string gltfDir = std::filesystem::path(filepath).parent_path().string();

	for (const auto& mat : model.materials)
	{
		GltfMaterial gMat;
		gMat.Name = mat.name.empty() ? "Unnamed" : mat.name;

		const auto& pbr = mat.pbrMetallicRoughness;

		gMat.BaseColorTexturePath          = ResolveTexturePath(model, pbr.baseColorTexture.index,         gltfDir);
		gMat.MetallicRoughnessTexturePath  = ResolveTexturePath(model, pbr.metallicRoughnessTexture.index, gltfDir);
		gMat.NormalMapPath                 = ResolveTexturePath(model, mat.normalTexture.index,             gltfDir);
		gMat.EmissiveTexturePath           = ResolveTexturePath(model, mat.emissiveTexture.index,           gltfDir);
		gMat.OcclusionTexturePath          = ResolveTexturePath(model, mat.occlusionTexture.index,          gltfDir);

		gMat.BaseColorFactor = glm::vec4(
			static_cast<float>(pbr.baseColorFactor[0]),
			static_cast<float>(pbr.baseColorFactor[1]),
			static_cast<float>(pbr.baseColorFactor[2]),
			static_cast<float>(pbr.baseColorFactor[3]));
		gMat.MetallicFactor  = static_cast<float>(pbr.metallicFactor);
		gMat.RoughnessFactor = static_cast<float>(pbr.roughnessFactor);
		gMat.EmissiveFactor  = glm::vec3(
			static_cast<float>(mat.emissiveFactor[0]),
			static_cast<float>(mat.emissiveFactor[1]),
			static_cast<float>(mat.emissiveFactor[2]));

		gMat.DoubleSided  = mat.doubleSided;
		gMat.AlphaCutoff  = static_cast<float>(mat.alphaCutoff);

		if      (mat.alphaMode == "OPAQUE") gMat.AlphaMode = GltfMaterial::AlphaMode::Opaque;
		else if (mat.alphaMode == "MASK")   gMat.AlphaMode = GltfMaterial::AlphaMode::Mask;
		else if (mat.alphaMode == "BLEND")  gMat.AlphaMode = GltfMaterial::AlphaMode::Blend;

		Materials.push_back(std::move(gMat));
	}

	std::cout << "[Mesh] Extracted " << Materials.size() << " material(s) from " << filepath << std::endl;

	// ── 2. Load geometry — one SubMesh per primitive ──────────────────────────
	for (const auto& mesh : model.meshes)
	{
		for (const auto& primitive : mesh.primitives)
		{
			// Skip degenerate primitives
			if (primitive.indices < 0)                                                      continue;
			if (primitive.mode   != TINYGLTF_MODE_TRIANGLES)                               continue;
			if (primitive.attributes.find("POSITION") == primitive.attributes.end())       continue;

			// ── Index accessor ────────────────────────────────────────────────
			const auto& indexAccessor    = model.accessors   [primitive.indices];
			const auto& indexBufferView  = model.bufferViews [indexAccessor.bufferView];
			const auto& indexBuffer      = model.buffers     [indexBufferView.buffer];

			// ── Position accessor ─────────────────────────────────────────────
			const auto& posAccessor   = model.accessors  [primitive.attributes.at("POSITION")];
			const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
			const auto& posBuffer     = model.buffers    [posBufferView.buffer];

			// ── TexCoord accessor (optional) ──────────────────────────────────
			bool              hasTexCoords       = primitive.attributes.count("TEXCOORD_0") > 0;
			const tinygltf::Accessor*   texCoordAccessor  = nullptr;
			const tinygltf::BufferView* texCoordBufferView = nullptr;
			const tinygltf::Buffer*     texCoordBuffer     = nullptr;

			if (hasTexCoords)
			{
				texCoordAccessor   = &model.accessors  [primitive.attributes.at("TEXCOORD_0")];
				texCoordBufferView = &model.bufferViews[texCoordAccessor->bufferView];
				texCoordBuffer     = &model.buffers    [texCoordBufferView->buffer];
			}

			const uint32_t baseVertex = static_cast<uint32_t>(Vertices.size());

			// ── Vertices ──────────────────────────────────────────────────────
			const size_t posStride = (posBufferView.byteStride > 0)
			                       ? posBufferView.byteStride
			                       : (3 * sizeof(float));

			for (size_t i = 0; i < posAccessor.count; ++i)
			{
				Vertex vertex{};

				const float* pos = reinterpret_cast<const float*>(
					&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset + i * posStride]);
				// GLTF is Y-up right-handed; convert to Z-up
				vertex.pos = { pos[0], -pos[2], pos[1] };

				if (hasTexCoords)
				{
					const size_t texStride = (texCoordBufferView->byteStride > 0)
					                       ? texCoordBufferView->byteStride
					                       : (2 * sizeof(float));
					const float* uv = reinterpret_cast<const float*>(
						&texCoordBuffer->data[
							texCoordBufferView->byteOffset + texCoordAccessor->byteOffset + i * texStride]);
					vertex.texCoord = { uv[0], uv[1] };
				}

				vertex.color = { 1.0f, 1.0f, 1.0f };
				Vertices.push_back(vertex);
			}

			// ── Indices ───────────────────────────────────────────────────────
			const uint32_t firstIndex  = static_cast<uint32_t>(Indices.size());
			const size_t   indexCount  = indexAccessor.count;

			size_t indexStride = 0;
			switch (indexAccessor.componentType)
			{
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  indexStride = sizeof(uint8_t);  break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: indexStride = sizeof(uint16_t); break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   indexStride = sizeof(uint32_t); break;
				default: throw std::runtime_error("[Mesh] Unsupported index component type");
			}

			Indices.reserve(Indices.size() + indexCount);
			const unsigned char* indexData =
				&indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];

			for (size_t i = 0; i < indexCount; ++i)
			{
				uint32_t index = 0;
				switch (indexAccessor.componentType)
				{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						index = *reinterpret_cast<const uint8_t* >(indexData + i * indexStride); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						index = *reinterpret_cast<const uint16_t*>(indexData + i * indexStride); break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						index = *reinterpret_cast<const uint32_t*>(indexData + i * indexStride); break;
				}
				Indices.push_back(baseVertex + index);
			}

			// ── Record sub-mesh ───────────────────────────────────────────────
			SubMesh sm;
			sm.FirstIndex    = firstIndex;
			sm.IndexCount    = static_cast<uint32_t>(indexCount);
			sm.MaterialIndex = primitive.material; // -1 when unassigned
			SubMeshes.push_back(sm);
		}
	}

	std::cout << "[Mesh] Loaded " << Vertices.size() << " vertices, "
	          << Indices.size()   << " indices, "
	          << SubMeshes.size() << " sub-mesh(es)" << std::endl;
}
