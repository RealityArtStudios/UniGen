// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#include <vector>
#include <string>

class FileSystem
{
public:
    FileSystem();
    ~FileSystem();
    //FileManager
    std::vector<char> ReadFile(const std::string& filename);
};
