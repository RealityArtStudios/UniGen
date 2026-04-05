// Copyright (c) CreationArtStudios, Khairol Anwar
#pragma once

#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// ─────────────────────────────────────────────────────────────────────────────
//  GltfMaterial
//  All data extracted from a single tinygltf::Material entry.
//  Paths are resolved to absolute/relative-to-exe strings ready for stbi_load.
// ─────────────────────────────────────────────────────────────────────────────
struct GltfMaterial
{
    std::string Name = "Unnamed";

    // ── Texture paths (empty = texture not present, use factor only) ──────────
    std::string BaseColorTexturePath;
    std::string MetallicRoughnessTexturePath;
    std::string NormalMapPath;
    std::string EmissiveTexturePath;
    std::string OcclusionTexturePath;

    // ── PBR factors ───────────────────────────────────────────────────────────
    glm::vec4 BaseColorFactor   = glm::vec4(1.0f);
    float     MetallicFactor    = 1.0f;
    float     RoughnessFactor   = 1.0f;
    glm::vec3 EmissiveFactor    = glm::vec3(0.0f);
    float     OcclusionStrength = 1.0f;
    float     NormalScale       = 1.0f;

    // ── Surface state ─────────────────────────────────────────────────────────
    bool DoubleSided = false;

    enum class AlphaMode { Opaque, Mask, Blend } AlphaMode = AlphaMode::Opaque;
    float AlphaCutoff = 0.5f;
};
