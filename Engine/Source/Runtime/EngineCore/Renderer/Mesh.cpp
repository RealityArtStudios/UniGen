// Copyright (c) CreationArtStudios, Khairol Anwar

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "Mesh.h"

Mesh::Mesh(const std::string& modelPath)
{
	LoadFromGLTF(modelPath);
}

void Mesh::LoadFromGLTF(const std::string& filepath)
{
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);

	if (!warn.empty())
	{
		std::cout << "glTF warning: " << warn << std::endl;
	}

	if (!err.empty())
	{
		std::cout << "glTF error: " << err << std::endl;
	}

	if (!ret)
	{
		throw std::runtime_error("Failed to load glTF model: " + filepath);
	}

	Vertices.clear();
	Indices.clear();

	for (const auto& mesh : model.meshes)
	{
		for (const auto& primitive : mesh.primitives)
		{
			const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
			const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
			const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

			const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
			const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
			const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

			bool hasTexCoords = primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end();
			const tinygltf::Accessor* texCoordAccessor = nullptr;
			const tinygltf::BufferView* texCoordBufferView = nullptr;
			const tinygltf::Buffer* texCoordBuffer = nullptr;

			if (hasTexCoords)
			{
				texCoordAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
				texCoordBufferView = &model.bufferViews[texCoordAccessor->bufferView];
				texCoordBuffer = &model.buffers[texCoordBufferView->buffer];
			}

			uint32_t baseVertex = static_cast<uint32_t>(Vertices.size());

			for (size_t i = 0; i < posAccessor.count; i++)
			{
				Vertex vertex{};

				const float* pos = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset + i * 12]);
				vertex.pos = { pos[0], -pos[2], pos[1] };

				if (hasTexCoords)
				{
					const float* texCoord = reinterpret_cast<const float*>(&texCoordBuffer->data[texCoordBufferView->byteOffset + texCoordAccessor->byteOffset + i * 8]);
					vertex.texCoord = { texCoord[0], texCoord[1] };
				}
				else
				{
					vertex.texCoord = { 0.0f, 0.0f };
				}

				vertex.color = { 1.0f, 1.0f, 1.0f };

				Vertices.push_back(vertex);
			}

			const unsigned char* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];
			size_t indexCount = indexAccessor.count;
			size_t indexStride = 0;

			if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
			{
				indexStride = sizeof(uint16_t);
			}
			else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
			{
				indexStride = sizeof(uint32_t);
			}
			else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
			{
				indexStride = sizeof(uint8_t);
			}
			else
			{
				throw std::runtime_error("Unsupported index component type");
			}

			Indices.reserve(Indices.size() + indexCount);

			for (size_t i = 0; i < indexCount; i++)
			{
				uint32_t index = 0;

				if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				{
					index = *reinterpret_cast<const uint16_t*>(indexData + i * indexStride);
				}
				else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
				{
					index = *reinterpret_cast<const uint32_t*>(indexData + i * indexStride);
				}
				else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
				{
					index = *reinterpret_cast<const uint8_t*>(indexData + i * indexStride);
				}

				Indices.push_back(baseVertex + index);
			}
		}
	}
}
