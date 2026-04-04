// Copyright (c) CreationArtStudios

#pragma once
#include <string>
#include <cstdint>

struct MeshComponent
{
    std::string ModelPath;
};

struct MaterialComponent
{
    std::string TexturePath;
};

struct NameComponent
{
    std::string Name;
    uint64_t EntityID = 0;
};
