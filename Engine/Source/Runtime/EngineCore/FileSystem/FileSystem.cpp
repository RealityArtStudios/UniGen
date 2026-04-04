// Copyright (c) CreationArtStudios, Khairol Anwar

#include "FileSystem.h"
#include <fstream>
#include <GLFW/glfw3.h>

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

bool FileSystem::OpenFileDialog(const char* filter, const char* title, std::filesystem::path& outPath)
{
    OPENFILENAMEA ofn;
    CHAR szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.lpstrTitle = title;

    if (GetOpenFileNameA(&ofn) == TRUE)
    {
        outPath = szFile;
        return true;
    }
    return false;
}

bool FileSystem::OpenFilesDialog(const char* filter, const char* title, std::vector<std::filesystem::path>& outPaths)
{
    OPENFILENAMEA ofn;
    CHAR szFile[16384] = { 0 };
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;
    ofn.lpstrTitle = title;

    if (GetOpenFileNameA(&ofn) == TRUE)
    {
        std::string filePath = szFile;
        
        // Check if multiple files (separated by null)
        if (filePath.find('\0') != std::string::npos)
        {
            std::string dir = filePath.substr(0, filePath.find('\0'));
            size_t pos = dir.size() + 1;
            while (pos < filePath.size() && pos != std::string::npos)
            {
                size_t nextNull = filePath.find('\0', pos);
                if (nextNull == std::string::npos)
                    nextNull = filePath.size();
                std::string filename = filePath.substr(pos, nextNull - pos);
                outPaths.push_back(std::filesystem::path(dir) / filename);
                pos = nextNull + 1;
            }
        }
        else
        {
            outPaths.push_back(filePath);
        }
        return true;
    }
    return false;
}

bool FileSystem::SaveFileDialog(const char* filter, const char* title, const char* defaultExt, std::filesystem::path& outPath)
{
    OPENFILENAMEA ofn;
    CHAR szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = defaultExt;

    if (GetSaveFileNameA(&ofn) == TRUE)
    {
        outPath = szFile;
        return true;
    }
    return false;
}
