// Copyright (c) CreationArtStudios, Khairol Anwar

#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <windows.h>
#include <commdlg.h>

class FileSystem
{
public:
    FileSystem();
    ~FileSystem();
    //FileManager
    std::vector<char> ReadFile(const std::string& filename);
    
    // File dialogs
    static bool OpenFileDialog(const char* filter, const char* title, std::filesystem::path& outPath);
    static bool OpenFilesDialog(const char* filter, const char* title, std::vector<std::filesystem::path>& outPaths);
    static bool SaveFileDialog(const char* filter, const char* title, const char* defaultExt, std::filesystem::path& outPath);
};
