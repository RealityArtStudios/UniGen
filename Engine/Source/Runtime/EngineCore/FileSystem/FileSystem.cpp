// Copyright (c) CreationArtStudios, Khairol Anwar

#include "FileSystem.h"
#include <fstream>

FileSystem::FileSystem()
{
}

FileSystem::~FileSystem()
{
}

std::vector<char> FileSystem::ReadFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file!");
    }
    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
}
