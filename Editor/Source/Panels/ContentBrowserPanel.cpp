// Copyright (c) CreationArtStudios, Khairol Anwar

// Disable Windows max/min macros
#define NOMINMAX

#include "ContentBrowserPanel.h"

// Engine headers (adjust relative paths if the panel lives somewhere else)
#include "Runtime/EngineCore/Renderer/Renderer.h"
#include "Runtime/EngineCore/Renderer/VulkanInstance/VulkanInstance.h"
#include "Runtime/EngineCore/FileSystem/FileSystem.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
    uint32_t FindMemType(VulkanInstance* vi,
        uint32_t typeFilter,
        vk::MemoryPropertyFlags props)
    {
        auto memProps = vi->GetPhysicalDevice().getMemoryProperties();
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        {
            if ((typeFilter & (1u << i)) &&
                (memProps.memoryTypes[i].propertyFlags & props) == props)
                return i;
        }
        throw std::runtime_error("[ContentBrowser] No suitable Vulkan memory type found.");
    }
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────
ContentBrowserPanel::ContentBrowserPanel(Renderer* renderer)
    : m_Renderer(renderer)
{
    m_SearchBuffer[0] = '\0';

    m_FolderIcon = LoadTextureFromFile("../Engine/SourceFiles/Icons/Folders/Folder_Base_256x.png");
    m_FileIcon = LoadTextureFromFile("../Engine/SourceFiles/ContentBrowser/ThumbnailCube.png");
    m_MenuIcon = LoadTextureFromFile("../Engine/SourceFiles/Icons/menu.png");

    if (!m_FolderIcon.IsValid())
        m_FolderIcon = MakeSolidColourTexture(220, 180, 50);

    if (!m_FileIcon.IsValid())
        m_FileIcon = MakeSolidColourTexture(180, 180, 200);

    // Initialize default filters
    m_AvailableFilters = {
        // Asset Types
        {"Blueprint", "Asset Types", ".ungblueprint", false},
        {"Material", "Asset Types", ".ungmaterial", false},
        {"Static Mesh", "Asset Types", ".gltf;.glb;.obj;.fbx", false},
        {"Texture", "Asset Types", ".png;.jpg;.jpeg;.tga", false},
        {"Sound", "Asset Types", ".wav;.mp3;.ogg", false},
        {"Level", "Asset Types", ".ungscene", false},
        
        // Other Filters
        {"Recently Opened", "Other Filters", "", false},
        {"Recently Created", "Other Filters", "", false},
        {"Modified", "Other Filters", "", false},
    };

    // Initialize default collections
    m_Collections = {
        {"Favorites", Collection::Type::Local, {}},
    };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────
void ContentBrowserPanel::SetBaseDirectory(const std::filesystem::path& path)
{
    m_BaseDirectory = path;
    m_CurrentDirectory = path;
    m_NavigationHistory.clear();
    m_NavigationHistory.push_back(path);
    m_HistoryIndex = 0;
}

void ContentBrowserPanel::RefreshIcons()
{
    m_FolderIcon = LoadTextureFromFile("../Engine/SourceFiles/Icons/Folders/Folder_Base_256x.png");
    m_FileIcon = LoadTextureFromFile("../Engine/SourceFiles/ContentBrowser/ThumbnailCube.png");
    m_MenuIcon = LoadTextureFromFile("../Engine/SourceFiles/Icons/menu.png");

    if (!m_FolderIcon.IsValid())
        m_FolderIcon = MakeSolidColourTexture(220, 180, 50);

    if (!m_FileIcon.IsValid())
        m_FileIcon = MakeSolidColourTexture(180, 180, 200);
}

void ContentBrowserPanel::OnRender()
{
    if (m_BaseDirectory.empty())
        return;

    ImGui::Begin("Content Browser");

    float panelWidth = ImGui::GetWindowWidth();
    float sourcesPanelWidth = m_ShowSourcesPanel ? 200.0f : 0.0f;

    // ── SOURCES PANEL ──────────────────────────────────────────────────────────
    if (m_ShowSourcesPanel)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
        ImGui::BeginChild("SourcesPanel", ImVec2(sourcesPanelWidth, -50), true);
        
        // Header with background
        if (ImGui::CollapsingHeader("PROJECT", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent();
            
            // Search within sources
            static char sourcesSearch[128] = "";
            ImGui::SetNextItemWidth(sourcesPanelWidth - 40);
            ImGui::InputTextWithHint("##SourcesSearch", "Search...", sourcesSearch, sizeof(sourcesSearch));
            
            // Root folder with icon
            bool isRootSelected = m_CurrentDirectory == m_BaseDirectory;
            ImGui::PushStyleColor(ImGuiCol_Text, isRootSelected ? ImVec4(0.4f, 0.7f, 1.0f, 1.0f) : ImGui::GetStyle().Colors[ImGuiCol_Text]);
            
            if (ImGui::Selectable("###Assets", isRootSelected))
            {
                m_CurrentDirectory = m_BaseDirectory;
                m_NavigationHistory.erase(m_NavigationHistory.begin() + m_HistoryIndex + 1, m_NavigationHistory.end());
                m_NavigationHistory.push_back(m_CurrentDirectory);
                ++m_HistoryIndex;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Assets");
            ImGui::PopStyleColor();
            
            // List subdirectories
            for (auto& entry : std::filesystem::directory_iterator(m_BaseDirectory))
            {
                if (!entry.is_directory()) continue;
                
                std::string folderName = entry.path().filename().string();
                
                // Filter by search
                if (sourcesSearch[0] != '\0') 
                {
                    std::string lowerFolder, lowerSearch;
                    lowerFolder.resize(folderName.size());
                    lowerSearch.resize(strlen(sourcesSearch));
                    std::transform(folderName.begin(), folderName.end(), lowerFolder.begin(), ::tolower);
                    std::transform(sourcesSearch, sourcesSearch + strlen(sourcesSearch), lowerSearch.begin(), ::tolower);
                    if (lowerFolder.find(lowerSearch) == std::string::npos)
                        continue;
                }
                
                bool isSelected = (m_CurrentDirectory == entry.path());
                ImGui::PushStyleColor(ImGuiCol_Text, isSelected ? ImVec4(0.4f, 0.7f, 1.0f, 1.0f) : ImGui::GetStyle().Colors[ImGuiCol_Text]);
                
                if (ImGui::Selectable(folderName.c_str(), isSelected))
                {
                    m_CurrentDirectory = entry.path();
                    m_NavigationHistory.erase(m_NavigationHistory.begin() + m_HistoryIndex + 1, m_NavigationHistory.end());
                    m_NavigationHistory.push_back(m_CurrentDirectory);
                    ++m_HistoryIndex;
                }
                
                ImGui::PopStyleColor();
            }
            
            ImGui::Unindent();
        }

        ImGui::Separator();

        // Collections section
        if (ImGui::CollapsingHeader("COLLECTIONS", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (auto& collection : m_Collections)
            {
                std::string label = collection.name;
                bool isSelected = (m_SelectedCollection == &collection);
                
                ImGui::PushStyleColor(ImGuiCol_Text, isSelected ? ImVec4(1.0f, 0.8f, 0.4f, 1.0f) : ImGui::GetStyle().Colors[ImGuiCol_Text]);
                
                if (ImGui::Selectable(label.c_str(), isSelected))
                {
                    m_SelectedCollection = &collection;
                }
                
                ImGui::PopStyleColor();
            }

            // Add new collection button
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            if (ImGui::Selectable("+ Add Collection", false))
            {
                m_Collections.push_back({"New Collection", Collection::Type::Local, {}});
            }
            ImGui::PopStyleColor();
        }
        
        ImGui::EndChild(); // SourcesPanel
        ImGui::PopStyleColor(); // ChildBg
        ImGui::PopStyleVar(); // ChildRounding
        
        ImGui::SameLine();
    }

    // ── MAIN CONTENT AREA ───────────────────────────────────────────────────────
    float mainAreaWidth = panelWidth - sourcesPanelWidth - (m_ShowSourcesPanel ? 8 : 0);
    ImGui::BeginChild("MainArea", ImVec2(mainAreaWidth, -50), false);

    // ── TOOLBAR ─────────────────────────────────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    
    // Asset Control Buttons
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.33f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
    
    if (ImGui::Button("+ Add", ImVec2(60, 24)))
    {
        OpenAddDialog();
    }
    ImGui::SameLine();
    if (ImGui::Button("Import", ImVec2(70, 24)))
    {
        OpenImportDialog();
    }
    ImGui::SameLine();
    if (ImGui::Button("Create", ImVec2(70, 24)))
    {
        ImGui::OpenPopup("CreateAssetPopup");
    }
    ImGui::SameLine(0, 12);
    ImGui::PopStyleColor(3); // Pop toolbar buttons
    ImGui::PopStyleVar(2); // Pop ItemSpacing, FrameRounding
    
    // Vertical separator
    ImVec2 minPos = ImGui::GetItemRectMin();
    ImVec2 maxPos = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(minPos.x, minPos.y + 2),
        ImVec2(minPos.x, maxPos.y - 2),
        ImColor(ImVec4(0.30f, 0.30f, 0.35f, 1.0f)), 1.0f);
    ImGui::SameLine();
    
    // History navigation
    bool canGoBack = m_HistoryIndex > 0;
    bool canGoForward = m_HistoryIndex < (int)m_NavigationHistory.size() - 1;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.22f, 0.27f, 1.0f));
    
    if (canGoBack)
    {
        if (ImGui::ArrowButton("##Back", ImGuiDir_Left))
        {
            --m_HistoryIndex;
            m_CurrentDirectory = m_NavigationHistory[m_HistoryIndex];
        }
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);
        ImGui::ArrowButton("##Back", ImGuiDir_Left);
        ImGui::PopStyleVar();
    }
    ImGui::SameLine();
    if (canGoForward)
    {
        if (ImGui::ArrowButton("##Forward", ImGuiDir_Right))
        {
            ++m_HistoryIndex;
            m_CurrentDirectory = m_NavigationHistory[m_HistoryIndex];
        }
    }
    else
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);
        ImGui::ArrowButton("##Forward", ImGuiDir_Right);
        ImGui::PopStyleVar();
    }
    
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    // Breadcrumb
    if (ImGui::Button("Assets"))
    {
        m_CurrentDirectory = m_BaseDirectory;
        m_NavigationHistory.erase(m_NavigationHistory.begin() + m_HistoryIndex + 1, m_NavigationHistory.end());
        m_NavigationHistory.push_back(m_CurrentDirectory);
        ++m_HistoryIndex;
    }

    std::filesystem::path relativePath = std::filesystem::relative(m_CurrentDirectory, m_BaseDirectory);
    for (auto& part : relativePath)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("/");
        ImGui::SameLine();
        std::string partStr = part.string();
        if (ImGui::Button(partStr.c_str()))
        {
            m_CurrentDirectory = m_BaseDirectory;
            for (auto& p : relativePath)
            {
                if (p == part) break;
                m_CurrentDirectory /= p;
            }
            m_NavigationHistory.erase(m_NavigationHistory.begin() + m_HistoryIndex + 1, m_NavigationHistory.end());
            m_NavigationHistory.push_back(m_CurrentDirectory);
            ++m_HistoryIndex;
        }
    }

    // Spacer and Settings button
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 40);
    
    if (ImGui::Button("###Settings", ImVec2(20, 20)))
    {
        ImGui::OpenPopup("SettingsPopup");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");

    // Settings dropdown menu
    if (ImGui::BeginPopup("SettingsPopup"))
    {
        ImGui::TextDisabled("View");
        ImGui::Separator();
        
        bool isTiles = m_ViewMode == ViewMode::Tiles;
        bool isList = m_ViewMode == ViewMode::List;
        bool isColumns = m_ViewMode == ViewMode::Columns;
        if (ImGui::MenuItem("Tiles", "##ViewMode", &isTiles)) { m_ViewMode = ViewMode::Tiles; }
        if (ImGui::MenuItem("List", "##ViewMode", &isList)) { m_ViewMode = ViewMode::List; }
        if (ImGui::MenuItem("Columns", "##ViewMode", &isColumns)) { m_ViewMode = ViewMode::Columns; }

        ImGui::Separator();
        ImGui::TextDisabled("Thumbnail Size");
        ImGui::Separator();
        if (ImGui::MenuItem("Tiny", "", m_ThumbnailSizePreset == ThumbnailSize::Tiny)) { m_ThumbnailSizePreset = ThumbnailSize::Tiny; m_ThumbnailSize = 48.0f; }
        if (ImGui::MenuItem("Small", "", m_ThumbnailSizePreset == ThumbnailSize::Small)) { m_ThumbnailSizePreset = ThumbnailSize::Small; m_ThumbnailSize = 64.0f; }
        if (ImGui::MenuItem("Medium", "", m_ThumbnailSizePreset == ThumbnailSize::Medium)) { m_ThumbnailSizePreset = ThumbnailSize::Medium; m_ThumbnailSize = 96.0f; }
        if (ImGui::MenuItem("Large", "", m_ThumbnailSizePreset == ThumbnailSize::Large)) { m_ThumbnailSizePreset = ThumbnailSize::Large; m_ThumbnailSize = 128.0f; }
        if (ImGui::MenuItem("Huge", "", m_ThumbnailSizePreset == ThumbnailSize::Huge)) { m_ThumbnailSizePreset = ThumbnailSize::Huge; m_ThumbnailSize = 192.0f; }

        ImGui::Separator();
        ImGui::TextDisabled("Options");
        ImGui::Separator();
        ImGui::MenuItem("Show Sources Panel", "", &m_ShowSourcesPanel);

        ImGui::EndPopup();
    }

    // ── SECOND ROW: Search and Filters ────────────────────────────────────────────
    ImGui::Separator();
    
    // Search bar with dark styling
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.17f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::SetNextItemWidth(280);
    ImGui::InputTextWithHint("##Search", "Search assets...", m_SearchBuffer, IM_ARRAYSIZE(m_SearchBuffer));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    // Filters button
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.22f, 0.27f, 1.0f));
    if (ImGui::Button("Filters", ImVec2(60, 24)))
    {
        ImGui::OpenPopup("FiltersPopup");
    }
    ImGui::PopStyleColor(2);

    // Show active filter chips
    for (size_t i = 0; i < m_ActiveFilters.size(); ++i)
    {
        if (m_ActiveFilters[i].enabled)
        {
            ImGui::SameLine();
            ImGui::PushID(("FilterChip" + std::to_string(i)).c_str());
            if (ImGui::Button(m_ActiveFilters[i].name.c_str()))
            {
                m_ActiveFilters[i].enabled = false;
            }
            ImGui::PopID();
        }
    }

    // Filters dropdown
    if (ImGui::BeginPopup("FiltersPopup"))
    {
        std::map<std::string, std::vector<Filter*>> categoryFilters;
        for (auto& filter : m_AvailableFilters)
        {
            categoryFilters[filter.category].push_back(&filter);
        }

        for (auto& [category, filters] : categoryFilters)
        {
            if (ImGui::BeginMenu(category.c_str()))
            {
                for (auto* filter : filters)
                {
                    bool enabled = filter->enabled;
                    if (ImGui::MenuItem(filter->name.c_str(), "", &enabled))
                    {
                        filter->enabled = enabled;
                        if (enabled)
                        {
                            bool found = false;
                            for (const auto& af : m_ActiveFilters)
                            {
                                if (af.name == filter->name)
                                {
                                    found = true;
                                    break;
                                }
                            }
                            if (!found)
                            {
                                m_ActiveFilters.push_back(*filter);
                            }
                        }
                        else
                        {
                            m_ActiveFilters.erase(
                                std::remove_if(m_ActiveFilters.begin(), m_ActiveFilters.end(),
                                    [&filter](const Filter& f) { return f.name == filter->name; }),
                                m_ActiveFilters.end());
                        }
                    }
                }
                ImGui::EndMenu();
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Clear All Filters"))
        {
            for (auto& filter : m_AvailableFilters)
            {
                filter.enabled = false;
            }
            m_ActiveFilters.clear();
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    // Copy search buffer to query
    m_SearchQuery = m_SearchBuffer;

    // ── ASSET VIEW ─────────────────────────────────────────────────────────────
    std::vector<std::filesystem::path> filteredEntries;
    for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
    {
        std::string filename = entry.path().filename().string();
        
        // Filter by search query (case-insensitive, supports advanced search syntax)
        if (!m_SearchQuery.empty())
        {
            if (!MatchesSearchExpression(m_SearchQuery, entry.path(), filename))
                continue;
        }

        // Apply collection filter (show only collection assets if collection is selected)
        if (m_SelectedCollection != nullptr)
        {
            bool foundInCollection = false;
            for (const auto& asset : m_SelectedCollection->assets)
            {
                if (asset == entry.path())
                {
                    foundInCollection = true;
                    break;
                }
            }
            if (!foundInCollection)
                continue;
        }

        // Apply active filters
        bool passesFilters = true;
        for (const auto& filter : m_ActiveFilters)
        {
            if (!filter.enabled) continue;

            if (filter.category == "Other Filters")
            {
                if (filter.name == "Recently Opened")
                {
                    if (!m_RecentAssets.empty())
                    {
                        bool found = false;
                        for (const auto& recent : m_RecentAssets)
                        {
                            if (recent == entry.path())
                            {
                                found = true;
                                break;
                            }
                        }
                        if (!found) passesFilters = false;
                    }
                    else
                    {
                        passesFilters = false;
                    }
                }
            }
            else if (!filter.extension.empty())
            {
                std::string ext = entry.path().extension().string();
                bool extMatch = false;
                size_t pos = 0;
                std::string exts = filter.extension;
                while ((pos = exts.find(';')) != std::string::npos)
                {
                    std::string token = exts.substr(0, pos);
                    if (ext == token)
                    {
                        extMatch = true;
                        break;
                    }
                    exts.erase(0, pos + 1);
                }
                if (!extMatch && !exts.empty())
                {
                    extMatch = (ext == exts);
                }
                if (!extMatch)
                    passesFilters = false;
            }
        }

        if (!passesFilters)
            continue;
            
        filteredEntries.push_back(entry.path());
    }

    // Sort: directories first, then files
    std::sort(filteredEntries.begin(), filteredEntries.end(), 
        [](const std::filesystem::path& a, const std::filesystem::path& b)
        {
            bool aIsDir = std::filesystem::is_directory(a);
            bool bIsDir = std::filesystem::is_directory(b);
            if (aIsDir != bIsDir) return aIsDir > bIsDir;
            return a.filename().string() < b.filename().string();
        });

    auto RenderAssetItem = [&](const std::filesystem::path& entryPath, const std::string& filenameStr, bool isDir)
    {
        ImGui::PushID(filenameStr.c_str());

        PanelTexture& icon = isDir ? m_FolderIcon : m_FileIcon;

        bool clicked = false;
        
        if (icon.IsValid())
        {
            clicked = ImGui::ImageButton(
                filenameStr.c_str(),
                reinterpret_cast<ImTextureID>(icon.descriptor),
                ImVec2(m_ThumbnailSize, m_ThumbnailSize),
                ImVec2(0, 0), ImVec2(1, 1),
                ImVec4(0.f, 0.f, 0.f, 0.f),
                ImVec4(1.f, 1.f, 1.f, 1.f));
        }
        else
        {
            clicked = ImGui::Button(isDir ? "[DIR]" : "[FILE]",
                ImVec2(m_ThumbnailSize, m_ThumbnailSize));
        }

        // Right-click context menu
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            ImGui::OpenPopup("AssetContextMenu");
        }

        if (ImGui::BeginPopup("AssetContextMenu"))
        {
            if (isDir)
            {
                if (ImGui::MenuItem("Open"))
                {
                    m_CurrentDirectory = entryPath;
                    m_NavigationHistory.erase(m_NavigationHistory.begin() + m_HistoryIndex + 1, m_NavigationHistory.end());
                    m_NavigationHistory.push_back(m_CurrentDirectory);
                    ++m_HistoryIndex;

                    m_RecentAssets.insert(m_RecentAssets.begin(), entryPath);
                    if (m_RecentAssets.size() > MAX_RECENT_ASSETS)
                        m_RecentAssets.pop_back();
                }
            }
            else
            {
                if (ImGui::MenuItem("Open"))
                {
                    m_RecentAssets.insert(m_RecentAssets.begin(), entryPath);
                    if (m_RecentAssets.size() > MAX_RECENT_ASSETS)
                        m_RecentAssets.pop_back();
                }

                std::string ext = entryPath.extension().string();
                if (ext == ".ungscene")
                {
                    if (ImGui::MenuItem("Load Scene"))
                    {
                        std::cout << "[ContentBrowser] Load scene: " << entryPath << std::endl;
                    }
                }

                if (ImGui::BeginMenu("Add to Collection"))
                {
                    for (auto& collection : m_Collections)
                    {
                        bool isInCollection = false;
                        for (const auto& asset : collection.assets)
                        {
                            if (asset == entryPath)
                            {
                                isInCollection = true;
                                break;
                            }
                        }
                        if (ImGui::MenuItem(collection.name.c_str(), "", &isInCollection))
                        {
                            if (isInCollection)
                            {
                                collection.assets.push_back(entryPath);
                            }
                            else
                            {
                                collection.assets.erase(
                                    std::remove_if(collection.assets.begin(), collection.assets.end(),
                                        [&entryPath](const std::filesystem::path& p) { return p == entryPath; }),
                                    collection.assets.end());
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
            }

            if (ImGui::MenuItem("Delete"))
            {
                m_SelectedAsset = entryPath;
            }
            ImGui::EndPopup();
        }

        // Double-click to navigate into directories or load scene
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (isDir)
            {
                m_CurrentDirectory = entryPath;
                m_NavigationHistory.erase(m_NavigationHistory.begin() + m_HistoryIndex + 1, m_NavigationHistory.end());
                m_NavigationHistory.push_back(m_CurrentDirectory);
                ++m_HistoryIndex;
            }
            else
            {
                std::string ext = entryPath.extension().string();
                if (ext == ".ungscene")
                {
                    std::cout << "[ContentBrowser] Load scene (double-click): " << entryPath << std::endl;
                }
            }
        }

        // Drag and drop
        if (ImGui::BeginDragDropSource())
        {
            std::filesystem::path relPath = std::filesystem::relative(entryPath, m_BaseDirectory);
            const std::wstring wstr = relPath.wstring();
            ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM",
                wstr.c_str(),
                (wstr.size() + 1) * sizeof(wchar_t));
            ImGui::Text("%s", filenameStr.c_str());
            ImGui::EndDragDropSource();
        }

        ImGui::PopID();
    };

    // Render based on view mode
    if (m_ViewMode == ViewMode::Tiles)
    {
        float cellSize = m_ThumbnailSize + m_Padding;
        float assetViewWidth = ImGui::GetContentRegionAvail().x;
        int   cols = std::max(1, static_cast<int>(assetViewWidth / cellSize));

        ImGui::Columns(cols, nullptr, false);

        for (auto& entryPath : filteredEntries)
        {
            std::string filenameStr = entryPath.filename().string();
            bool        isDir = std::filesystem::is_directory(entryPath);

            RenderAssetItem(entryPath, filenameStr, isDir);
            ImGui::TextWrapped("%s", filenameStr.c_str());
            ImGui::NextColumn();
        }

        ImGui::Columns(1);
    }
    else if (m_ViewMode == ViewMode::List)
    {
        ImGui::Columns(3, "AssetListHeader");
        
        // Header
        ImGui::Text("Name"); ImGui::NextColumn();
        ImGui::Text("Type"); ImGui::NextColumn();
        ImGui::Text("Size"); ImGui::NextColumn();
        ImGui::Separator();

        for (auto& entryPath : filteredEntries)
        {
            std::string filenameStr = entryPath.filename().string();
            bool        isDir = std::filesystem::is_directory(entryPath);

            ImGui::PushID(filenameStr.c_str());
            
            // Icon and name
            PanelTexture& icon = isDir ? m_FolderIcon : m_FileIcon;
            if (icon.IsValid())
            {
                ImGui::Image(
                    reinterpret_cast<ImTextureID>(icon.descriptor),
                    ImVec2(16.f, 16.f));
                ImGui::SameLine();
            }
            ImGui::Text("%s", filenameStr.c_str());
            ImGui::NextColumn();

            // Type
            ImGui::Text("%s", isDir ? "Folder" : "File");
            ImGui::NextColumn();

            // Size
            if (!isDir)
            {
                auto size = std::filesystem::file_size(entryPath);
                if (size < 1024)
                    ImGui::Text("%d B", (int)size);
                else if (size < 1024 * 1024)
                    ImGui::Text("%.1f KB", size / 1024.0);
                else
                    ImGui::Text("%.1f MB", size / (1024.0 * 1024.0));
            }
            else
            {
                ImGui::TextDisabled("-");
            }
            ImGui::NextColumn();

            // Row click
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                if (isDir)
                {
                    m_CurrentDirectory = entryPath;
                    m_NavigationHistory.erase(m_NavigationHistory.begin() + m_HistoryIndex + 1, m_NavigationHistory.end());
                    m_NavigationHistory.push_back(m_CurrentDirectory);
                    ++m_HistoryIndex;
                }
            }

            // Context menu
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                ImGui::OpenPopup("AssetContextMenu");
            }

            if (ImGui::BeginPopup("AssetContextMenu"))
            {
                if (isDir)
                {
                    if (ImGui::MenuItem("Open"))
                    {
                        m_CurrentDirectory = entryPath;
                        m_NavigationHistory.erase(m_NavigationHistory.begin() + m_HistoryIndex + 1, m_NavigationHistory.end());
                        m_NavigationHistory.push_back(m_CurrentDirectory);
                        ++m_HistoryIndex;
                    }
                }
                else
                {
                    if (ImGui::MenuItem("Open"))
                    {
                        m_RecentAssets.insert(m_RecentAssets.begin(), entryPath);
                        if (m_RecentAssets.size() > MAX_RECENT_ASSETS)
                            m_RecentAssets.pop_back();
                    }

                    if (ImGui::BeginMenu("Add to Collection"))
                    {
                        for (auto& collection : m_Collections)
                        {
                            bool isInCollection = false;
                            for (const auto& asset : collection.assets)
                            {
                                if (asset == entryPath)
                                {
                                    isInCollection = true;
                                    break;
                                }
                            }
                            if (ImGui::MenuItem(collection.name.c_str(), "", &isInCollection))
                            {
                                if (isInCollection)
                                    collection.assets.push_back(entryPath);
                                else
                                    collection.assets.erase(
                                        std::remove_if(collection.assets.begin(), collection.assets.end(),
                                            [&entryPath](const std::filesystem::path& p) { return p == entryPath; }),
                                        collection.assets.end());
                            }
                        }
                        ImGui::EndMenu();
                    }
                }

                if (ImGui::MenuItem("Delete"))
                {
                    m_SelectedAsset = entryPath;
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::Columns(1);
    }
    else if (m_ViewMode == ViewMode::Columns)
    {
        ImGui::Columns(5, "AssetColumnsHeader");
        
        // Header
        ImGui::Text("Name"); ImGui::NextColumn();
        ImGui::Text("Type"); ImGui::NextColumn();
        ImGui::Text("Size"); ImGui::NextColumn();
        ImGui::Text("Modified"); ImGui::NextColumn();
        ImGui::Text("Path"); ImGui::NextColumn();
        ImGui::Separator();

        for (auto& entryPath : filteredEntries)
        {
            std::string filenameStr = entryPath.filename().string();
            bool        isDir = std::filesystem::is_directory(entryPath);

            ImGui::PushID(filenameStr.c_str());
            
            // Name with icon
            PanelTexture& icon = isDir ? m_FolderIcon : m_FileIcon;
            if (icon.IsValid())
            {
                ImGui::Image(
                    reinterpret_cast<ImTextureID>(icon.descriptor),
                    ImVec2(16.f, 16.f));
                ImGui::SameLine();
            }
            ImGui::Text("%s", filenameStr.c_str());
            ImGui::NextColumn();

            // Type
            ImGui::Text("%s", isDir ? "Folder" : "File");
            ImGui::NextColumn();

            // Size
            if (!isDir)
            {
                auto size = std::filesystem::file_size(entryPath);
                if (size < 1024)
                    ImGui::Text("%d B", (int)size);
                else if (size < 1024 * 1024)
                    ImGui::Text("%.1f KB", size / 1024.0);
                else
                    ImGui::Text("%.1f MB", size / (1024.0 * 1024.0));
            }
            else
            {
                ImGui::TextDisabled("-");
            }
            ImGui::NextColumn();

            // Modified time
            auto time = std::filesystem::last_write_time(entryPath);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
            std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
            std::tm tm = {};
            localtime_s(&tm, &tt);
            char buf[32];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
            ImGui::Text("%s", buf);
            ImGui::NextColumn();

            // Path
            std::filesystem::path relPath = std::filesystem::relative(entryPath, m_BaseDirectory);
            ImGui::TextDisabled("%s", relPath.string().c_str());
            ImGui::NextColumn();

            // Context menu
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
            {
                ImGui::OpenPopup("AssetContextMenu");
            }

            if (ImGui::BeginPopup("AssetContextMenu"))
            {
                if (isDir)
                {
                    if (ImGui::MenuItem("Open"))
                    {
                        m_CurrentDirectory = entryPath;
                        m_NavigationHistory.erase(m_NavigationHistory.begin() + m_HistoryIndex + 1, m_NavigationHistory.end());
                        m_NavigationHistory.push_back(m_CurrentDirectory);
                        ++m_HistoryIndex;
                    }
                }
                else
                {
                    if (ImGui::MenuItem("Open"))
                    {
                        m_RecentAssets.insert(m_RecentAssets.begin(), entryPath);
                        if (m_RecentAssets.size() > MAX_RECENT_ASSETS)
                            m_RecentAssets.pop_back();
                    }

                    if (ImGui::BeginMenu("Add to Collection"))
                    {
                        for (auto& collection : m_Collections)
                        {
                            bool isInCollection = false;
                            for (const auto& asset : collection.assets)
                            {
                                if (asset == entryPath)
                                {
                                    isInCollection = true;
                                    break;
                                }
                            }
                            if (ImGui::MenuItem(collection.name.c_str(), "", &isInCollection))
                            {
                                if (isInCollection)
                                    collection.assets.push_back(entryPath);
                                else
                                    collection.assets.erase(
                                        std::remove_if(collection.assets.begin(), collection.assets.end(),
                                            [&entryPath](const std::filesystem::path& p) { return p == entryPath; }),
                                        collection.assets.end());
                            }
                        }
                        ImGui::EndMenu();
                    }
                }

                if (ImGui::MenuItem("Delete"))
                {
                    m_SelectedAsset = entryPath;
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::Columns(1);
    }

    // Asset count
    ImGui::TextDisabled("%d assets", (int)filteredEntries.size());

    // ── RIGHT-CLICK CONTEXT MENU ────────────────────────────────────────────────
    if (ImGui::BeginPopupContextWindow("ContentBrowserContext"))
    {
        if (ImGui::MenuItem("New Folder"))
        {
            std::filesystem::path newFolderPath = m_CurrentDirectory / "NewFolder";
            int counter = 1;
            while (std::filesystem::exists(newFolderPath))
            {
                newFolderPath = m_CurrentDirectory / ("NewFolder" + std::to_string(counter));
                ++counter;
            }
            std::filesystem::create_directory(newFolderPath);
        }

        if (ImGui::MenuItem("New C++ Class"))
        {
            // Create new C++ class
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Refresh"))
        {
            // Refresh happens automatically
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Toggle Sources Panel"))
        {
            m_ShowSourcesPanel = !m_ShowSourcesPanel;
        }

        ImGui::EndPopup();
    }

    // Delete confirmation dialog
    if (!m_SelectedAsset.empty())
    {
        ImGui::OpenPopup("DeleteConfirm");
    }
    
    if (ImGui::BeginPopupModal("DeleteConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        std::string filename = m_SelectedAsset.filename().string();
        ImGui::Text("Are you sure you want to delete?");
        ImGui::Text("%s", filename.c_str());
        ImGui::TextDisabled("This action cannot be undone.");
        
        ImGui::Separator();
        
        if (ImGui::Button("Delete", ImVec2(120, 0)))
        {
            try
            {
                if (std::filesystem::is_directory(m_SelectedAsset))
                {
                    std::filesystem::remove_all(m_SelectedAsset);
                }
                else
                {
                    std::filesystem::remove(m_SelectedAsset);
                }
                std::cout << "[ContentBrowser] Deleted: " << m_SelectedAsset << std::endl;
            }
            catch (const std::filesystem::filesystem_error& e)
            {
                std::cerr << "[ContentBrowser] Delete failed: " << e.what() << std::endl;
            }
            m_SelectedAsset.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            m_SelectedAsset.clear();
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }

    // Create Asset popup
    if (ImGui::BeginPopupModal("CreateAssetPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Create New Asset");
        ImGui::Separator();

        static char assetName[128] = "NewScene";
        static int selectedAssetType = 0;
        const char* assetTypes[] = { "Scene (.ungscene)", "Blueprint (.ungblueprint)", "Material (.ungmaterial)" };

        ImGui::Text("Name:");
        ImGui::SameLine();
        ImGui::InputText("##AssetName", assetName, sizeof(assetName));

        ImGui::Text("Type:");
        ImGui::SameLine();
        ImGui::Combo("##AssetType", &selectedAssetType, assetTypes, IM_ARRAYSIZE(assetTypes));

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            std::string name = assetName;
            if (!name.empty())
            {
                std::filesystem::path newAssetPath;
                std::string extension;

                switch (selectedAssetType)
                {
                    case 0: extension = ".ungscene"; break;
                    case 1: extension = ".ungblueprint"; break;
                    case 2: extension = ".ungmaterial"; break;
                    default: extension = ".ungscene"; break;
                }

                newAssetPath = m_CurrentDirectory / (name + extension);

                if (!std::filesystem::exists(newAssetPath))
                {
                    std::ofstream outFile(newAssetPath);
                    if (outFile.is_open())
                    {
                        if (selectedAssetType == 0)
                        {
                            outFile << "Scene:\n";
                            outFile << "  Name: " << name << "\n";
                            outFile << "  Entities: []\n";
                        }
                        else if (selectedAssetType == 1)
                        {
                            outFile << "Blueprint:\n";
                            outFile << "  Name: " << name << "\n";
                            outFile << "  Components: []\n";
                        }
                        else if (selectedAssetType == 2)
                        {
                            outFile << "Material:\n";
                            outFile << "  Name: " << name << "\n";
                            outFile << "  Shader: Default\n";
                            outFile << "  Properties:\n";
                            outFile << "    BaseColor: (1,1,1,1)\n";
                        }
                        outFile.close();
                        std::cout << "[ContentBrowser] Created asset: " << newAssetPath << std::endl;
                    }
                }
                else
                {
                    std::cerr << "[ContentBrowser] Asset already exists: " << newAssetPath << std::endl;
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::EndChild(); // MainArea

    ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Texture helpers
// ─────────────────────────────────────────────────────────────────────────────
PanelTexture ContentBrowserPanel::LoadTextureFromFile(const std::string& filepath)
{
    int w = 0, h = 0, ch = 0;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &w, &h, &ch, STBI_rgb_alpha);
    if (!pixels)
    {
        return PanelTexture{};
    }

    PanelTexture result = UploadToGPU(pixels, static_cast<uint32_t>(w),
        static_cast<uint32_t>(h));
    stbi_image_free(pixels);
    return result;
}

PanelTexture ContentBrowserPanel::MakeSolidColourTexture(uint8_t r, uint8_t g,
    uint8_t b, uint8_t a)
{
    constexpr uint32_t SIZE = 4;
    uint8_t pixels[SIZE * SIZE * 4];
    for (uint32_t i = 0; i < SIZE * SIZE; ++i)
    {
        pixels[i * 4 + 0] = r;
        pixels[i * 4 + 1] = g;
        pixels[i * 4 + 2] = b;
        pixels[i * 4 + 3] = a;
    }
    return UploadToGPU(pixels, SIZE, SIZE);
}

PanelTexture ContentBrowserPanel::UploadToGPU(const uint8_t* pixels,
    uint32_t       width,
    uint32_t       height)
{
    PanelTexture result;

    auto* vi = m_Renderer->GetVulkanInstance();
    auto& dev = vi->GetLogicalDevice();
    vk::DeviceSize imgSize = static_cast<vk::DeviceSize>(width) * height * 4;

    // Staging buffer
    vk::BufferCreateInfo stagingCI;
    stagingCI.size = imgSize;
    stagingCI.usage = vk::BufferUsageFlagBits::eTransferSrc;
    stagingCI.sharingMode = vk::SharingMode::eExclusive;

    vk::raii::Buffer stagingBuf(dev, stagingCI);
    auto stagingReq = stagingBuf.getMemoryRequirements();

    vk::MemoryAllocateInfo stagingAllocInfo;
    stagingAllocInfo.allocationSize = stagingReq.size;
    stagingAllocInfo.memoryTypeIndex = FindMemType(vi, stagingReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible |
        vk::MemoryPropertyFlagBits::eHostCoherent);

    vk::raii::DeviceMemory stagingMem(dev, stagingAllocInfo);
    stagingBuf.bindMemory(*stagingMem, 0);

    void* mapped = stagingMem.mapMemory(0, imgSize);
    std::memcpy(mapped, pixels, static_cast<size_t>(imgSize));
    stagingMem.unmapMemory();

    // Device-local image
    vk::ImageCreateInfo imgCI;
    imgCI.imageType = vk::ImageType::e2D;
    imgCI.format = vk::Format::eR8G8B8A8Srgb;
    imgCI.extent = vk::Extent3D{ width, height, 1 };
    imgCI.mipLevels = 1;
    imgCI.arrayLayers = 1;
    imgCI.samples = vk::SampleCountFlagBits::e1;
    imgCI.tiling = vk::ImageTiling::eOptimal;
    imgCI.usage = vk::ImageUsageFlagBits::eTransferDst |
        vk::ImageUsageFlagBits::eSampled;
    imgCI.sharingMode = vk::SharingMode::eExclusive;
    imgCI.initialLayout = vk::ImageLayout::eUndefined;

    result.image = vk::raii::Image(dev, imgCI);

    auto imgReq = result.image.getMemoryRequirements();

    vk::MemoryAllocateInfo imgAllocInfo;
    imgAllocInfo.allocationSize = imgReq.size;
    imgAllocInfo.memoryTypeIndex = FindMemType(vi, imgReq.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eDeviceLocal);

    result.memory = vk::raii::DeviceMemory(dev, imgAllocInfo);
    result.image.bindMemory(*result.memory, 0);

    // Command buffer
    vk::CommandPoolCreateInfo poolCI;
    poolCI.flags = vk::CommandPoolCreateFlagBits::eTransient;
    poolCI.queueFamilyIndex = vi->GetQueueFamilyIndex();
    vk::raii::CommandPool tmpPool(dev, poolCI);

    vk::CommandBufferAllocateInfo cbAlloc;
    cbAlloc.commandPool = *tmpPool;
    cbAlloc.level = vk::CommandBufferLevel::ePrimary;
    cbAlloc.commandBufferCount = 1;

    auto  cmdBufs = dev.allocateCommandBuffers(cbAlloc);
    auto& cmd = cmdBufs.front();

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd.begin(beginInfo);

    // Undefined → TransferDst
    {
        vk::ImageMemoryBarrier bar;
        bar.oldLayout = vk::ImageLayout::eUndefined;
        bar.newLayout = vk::ImageLayout::eTransferDstOptimal;
        bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bar.image = *result.image;
        bar.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        bar.srcAccessMask = {};
        bar.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, bar);
    }

    // Copy
    {
        vk::BufferImageCopy region;
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = vk::Offset3D{ 0, 0, 0 };
        region.imageExtent = vk::Extent3D{ width, height, 1 };
        cmd.copyBufferToImage(*stagingBuf, *result.image, vk::ImageLayout::eTransferDstOptimal, region);
    }

    // TransferDst → ShaderReadOnlyOptimal
    {
        vk::ImageMemoryBarrier bar;
        bar.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        bar.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        bar.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bar.image = *result.image;
        bar.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        bar.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        bar.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, bar);
    }

    cmd.end();

    vk::SubmitInfo submit;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &*cmd;
    vi->GetGraphicsQueue().submit(submit, nullptr);
    vi->GetGraphicsQueue().waitIdle();

    // Image view
    vk::ImageViewCreateInfo viewCI;
    viewCI.image = *result.image;
    viewCI.viewType = vk::ImageViewType::e2D;
    viewCI.format = vk::Format::eR8G8B8A8Srgb;
    viewCI.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    result.imageView = vk::raii::ImageView(dev, viewCI);

    // Sampler
    vk::SamplerCreateInfo sampCI;
    sampCI.magFilter = vk::Filter::eLinear;
    sampCI.minFilter = vk::Filter::eLinear;
    sampCI.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampCI.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    sampCI.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    sampCI.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    sampCI.borderColor = vk::BorderColor::eFloatTransparentBlack;
    result.sampler = vk::raii::Sampler(dev, sampCI);

    result.descriptor = ImGui_ImplVulkan_AddTexture(
        static_cast<VkSampler>(*result.sampler),
        static_cast<VkImageView>(*result.imageView),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return result;
}

void ContentBrowserPanel::OpenAddDialog()
{
    std::filesystem::path selectedFile;
    if (FileSystem::OpenFileDialog("All Files (*.*)\0*.*\0", "Add Existing Asset", selectedFile))
    {
        std::filesystem::path destPath = m_CurrentDirectory / selectedFile.filename();
        
        try
        {
            std::filesystem::copy_file(selectedFile, destPath, std::filesystem::copy_options::overwrite_existing);
            std::cout << "[ContentBrowser] Added asset: " << destPath << std::endl;
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "[ContentBrowser] Failed to add asset: " << e.what() << std::endl;
        }
    }
}

void ContentBrowserPanel::OpenImportDialog()
{
    std::vector<std::filesystem::path> selectedFiles;
    const char* filter = 
        "All Supported Files\0*.png;*.jpg;*.jpeg;*.glb;*.gltf;*.obj;*.fbx;*.wav;*.mp3;*.ogg\0"
        "Images (*.png;*.jpg;*.jpeg)\0*.png;*.jpg;*.jpeg\0"
        "3D Models (*.glb;*.gltf;*.obj;*.fbx)\0*.glb;*.gltf;*.obj;*.fbx\0"
        "Audio (*.wav;*.mp3;*.ogg)\0*.wav;*.mp3;*.ogg\0"
        "All Files (*.*)\0*.*\0";
        
    if (FileSystem::OpenFilesDialog(filter, "Import Assets", selectedFiles))
    {
        for (const auto& sourceFile : selectedFiles)
        {
            std::filesystem::path destPath = m_CurrentDirectory / sourceFile.filename();
            
            try
            {
                std::filesystem::copy_file(sourceFile, destPath, std::filesystem::copy_options::overwrite_existing);
                std::cout << "[ContentBrowser] Imported: " << destPath << std::endl;
            }
            catch (const std::filesystem::filesystem_error& e)
            {
                std::cerr << "[ContentBrowser] Failed to import: " << e.what() << std::endl;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Advanced Search Implementation
// ─────────────────────────────────────────────────────────────────────────────

static std::string Trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

static bool IsDigit(char c) { return c >= '0' && c <= '9'; }
static bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static bool IsAlphaNum(char c) { return IsAlpha(c) || IsDigit(c) || c == '_'; }

static std::vector<ContentBrowserPanel::SearchToken> TokenizeSearch(const std::string& input)
{
    std::vector<ContentBrowserPanel::SearchToken> tokens;
    std::string s = input;
    size_t pos = 0;

    while (pos < s.size())
    {
        char c = s[pos];

        if (c == ' ' || c == '\t')
        {
            ++pos;
            continue;
        }

        if (c == '"' || c == '\'')
        {
            char quote = c;
            ++pos;
            std::string str;
            while (pos < s.size() && s[pos] != quote)
            {
                if (s[pos] == '\\' && pos + 1 < s.size())
                {
                    ++pos;
                    if (s[pos] == 'n') str += '\n';
                    else if (s[pos] == 't') str += '\t';
                    else str += s[pos];
                }
                else
                {
                    str += s[pos];
                }
                ++pos;
            }
            if (pos < s.size()) ++pos;
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::String;
            tok.value = str;
            tokens.push_back(tok);
            continue;
        }

        if (pos + 1 < s.size() && (
            (s[pos] == '=' && s[pos + 1] == '=') ||
            (s[pos] == '!' && s[pos + 1] == '=') ||
            (s[pos] == '<' && s[pos + 1] == '=') ||
            (s[pos] == '>' && s[pos + 1] == '=') ||
            (s[pos] == '&' && s[pos + 1] == '&') ||
            (s[pos] == '|' && s[pos + 1] == '|')))
        {
            ContentBrowserPanel::SearchToken tok;
            std::string two(1, s[pos]); two += s[pos + 1];
            if (two == "==") tok.type = ContentBrowserPanel::SearchTokenType::OpEqual;
            else if (two == "!=") tok.type = ContentBrowserPanel::SearchTokenType::OpNotEqual;
            else if (two == "<=") tok.type = ContentBrowserPanel::SearchTokenType::OpLessOrEqual;
            else if (two == ">=") tok.type = ContentBrowserPanel::SearchTokenType::OpGreaterOrEqual;
            else if (two == "&&") tok.type = ContentBrowserPanel::SearchTokenType::OpAnd;
            else if (two == "||") tok.type = ContentBrowserPanel::SearchTokenType::OpOr;
            tok.value = two;
            tokens.push_back(tok);
            pos += 2;
            continue;
        }

        if (c == '=' || c == ':')
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::OpEqual;
            tok.value = std::string(1, c);
            tokens.push_back(tok);
            ++pos;
            continue;
        }

        if (c == '!')
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::OpNot;
            tok.value = "!";
            tokens.push_back(tok);
            ++pos;
            continue;
        }

        if (c == '<')
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::OpLess;
            tok.value = "<";
            tokens.push_back(tok);
            ++pos;
            continue;
        }

        if (c == '>')
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::OpGreater;
            tok.value = ">";
            tokens.push_back(tok);
            ++pos;
            continue;
        }

        if ((c == 'A' || c == 'a') && pos + 2 < s.size() &&
            (s[pos + 1] == 'N' || s[pos + 1] == 'n') &&
            (s[pos + 2] == 'D' || s[pos + 2] == 'd'))
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::OpAnd;
            tok.value = "AND";
            tokens.push_back(tok);
            pos += 3;
            continue;
        }

        if ((c == 'O' || c == 'o') && pos + 1 < s.size() &&
            (s[pos + 1] == 'R' || s[pos + 1] == 'r'))
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::OpOr;
            tok.value = "OR";
            tokens.push_back(tok);
            pos += 2;
            continue;
        }

        if (c == '&')
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::OpAnd;
            tok.value = "&";
            tokens.push_back(tok);
            ++pos;
            continue;
        }

        if (c == '|')
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::OpOr;
            tok.value = "|";
            tokens.push_back(tok);
            ++pos;
            continue;
        }

        if (c == '(')
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::LParen;
            tok.value = "(";
            tokens.push_back(tok);
            ++pos;
            continue;
        }

        if (c == ')')
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::RParen;
            tok.value = ")";
            tokens.push_back(tok);
            ++pos;
            continue;
        }

        if (c == '-')
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::OpTextInvert;
            tok.value = "-";
            tokens.push_back(tok);
            ++pos;
            continue;
        }

        if (c == '+')
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::OpTextExact;
            tok.value = "+";
            tokens.push_back(tok);
            ++pos;
            continue;
        }

        if (c == '.' && pos + 2 < s.size() && s[pos + 1] == '.' && s[pos + 2] == '.')
        {
            ContentBrowserPanel::SearchToken tok;
            tok.type = ContentBrowserPanel::SearchTokenType::OpTextAnchorStart;
            tok.value = "...";
            tokens.push_back(tok);
            pos += 3;
            continue;
        }

        std::string word;
        while (pos < s.size() && !strchr(" \t=:<>!()&|-\"+.", s[pos]))
        {
            word += s[pos++];
        }

        if (!word.empty())
        {
            if (IsDigit(word[0]) || (word[0] == '.' && word.size() > 1 && IsDigit(word[1])))
            {
                ContentBrowserPanel::SearchToken tok;
                tok.type = ContentBrowserPanel::SearchTokenType::Number;
                tok.value = word;
                tok.numberValue = std::atof(word.c_str());
                tokens.push_back(tok);
            }
            else
            {
                ContentBrowserPanel::SearchToken tok;
                tok.type = ContentBrowserPanel::SearchTokenType::Identifier;
                tok.value = word;
                tokens.push_back(tok);
            }
        }
    }

    ContentBrowserPanel::SearchToken endTok;
    endTok.type = ContentBrowserPanel::SearchTokenType::End;
    tokens.push_back(endTok);

    return tokens;
}

static bool IsBinaryOp(ContentBrowserPanel::SearchTokenType type)
{
    return type == ContentBrowserPanel::SearchTokenType::OpEqual ||
           type == ContentBrowserPanel::SearchTokenType::OpNotEqual ||
           type == ContentBrowserPanel::SearchTokenType::OpLess ||
           type == ContentBrowserPanel::SearchTokenType::OpLessOrEqual ||
           type == ContentBrowserPanel::SearchTokenType::OpGreater ||
           type == ContentBrowserPanel::SearchTokenType::OpGreaterOrEqual ||
           type == ContentBrowserPanel::SearchTokenType::OpAnd ||
           type == ContentBrowserPanel::SearchTokenType::OpOr;
}

static int GetPrecedence(ContentBrowserPanel::SearchTokenType type)
{
    switch (type)
    {
        case ContentBrowserPanel::SearchTokenType::OpOr: return 1;
        case ContentBrowserPanel::SearchTokenType::OpAnd: return 2;
        case ContentBrowserPanel::SearchTokenType::OpEqual:
        case ContentBrowserPanel::SearchTokenType::OpNotEqual:
        case ContentBrowserPanel::SearchTokenType::OpLess:
        case ContentBrowserPanel::SearchTokenType::OpLessOrEqual:
        case ContentBrowserPanel::SearchTokenType::OpGreater:
        case ContentBrowserPanel::SearchTokenType::OpGreaterOrEqual:
            return 3;
        default: return 0;
    }
}

ContentBrowserPanel::SearchNode ContentBrowserPanel::ParseSearchQuery(const std::string& query)
{
    std::vector<SearchToken> tokens = TokenizeSearch(query);
    size_t idx = 0;

    std::function<SearchNode()> parseExpression;
    std::function<SearchNode()> parseTerm;
    std::function<SearchNode()> parseFactor;

    parseFactor = [&]()
    {
        if (idx >= tokens.size()) return SearchNode{ SearchNode::Type::Identifier };

        SearchToken& tok = tokens[idx];

        if (tok.type == SearchTokenType::LParen)
        {
            ++idx;
            SearchNode node = parseExpression();
            if (idx < tokens.size() && tokens[idx].type == SearchTokenType::RParen)
                ++idx;
            node.type = SearchNode::Type::Group;
            return node;
        }

        if (tok.type == SearchTokenType::OpNot)
        {
            ++idx;
            SearchNode child = parseFactor();
            SearchNode node;
            node.type = SearchNode::Type::UnaryOp;
            node.op = "NOT";
            node.children.push_back(child);
            return node;
        }

        if (tok.type == SearchTokenType::OpTextInvert)
        {
            ++idx;
            SearchNode child = parseFactor();
            SearchNode node;
            node.type = SearchNode::Type::UnaryOp;
            node.op = "TEXT_INVERT";
            node.children.push_back(child);
            return node;
        }

        if (tok.type == SearchTokenType::OpTextExact)
        {
            ++idx;
            SearchNode child = parseFactor();
            SearchNode node;
            node.type = SearchNode::Type::UnaryOp;
            node.op = "TEXT_EXACT";
            node.children.push_back(child);
            return node;
        }

        if (tok.type == SearchTokenType::OpTextAnchorStart)
        {
            ++idx;
            SearchNode node;
            node.type = SearchNode::Type::UnaryOp;
            node.op = "TEXT_ANCHOR_START";
            if (idx < tokens.size() && (tokens[idx].type == SearchTokenType::Identifier || tokens[idx].type == SearchTokenType::String))
            {
                node.stringValue = tokens[idx].value;
                ++idx;
            }
            return node;
        }

        if (tok.type == SearchTokenType::Identifier || tok.type == SearchTokenType::String || tok.type == SearchTokenType::Number)
        {
            SearchNode node;
            if (tok.type == SearchTokenType::Number)
            {
                node.type = SearchNode::Type::Number;
                node.numberValue = tok.numberValue;
            }
            else if (tok.type == SearchTokenType::String)
            {
                node.type = SearchNode::Type::String;
                node.stringValue = tok.value;
            }
            else
            {
                node.type = SearchNode::Type::Identifier;
                node.identifier = tok.value;
            }
            ++idx;
            return node;
        }

        return SearchNode{ SearchNode::Type::Identifier };
    };

    parseTerm = [&]()
    {
        return parseFactor();
    };

    parseExpression = [&]()
    {
        SearchNode left = parseTerm();

        while (idx < tokens.size() && IsBinaryOp(tokens[idx].type))
        {
            SearchToken opTok = tokens[idx];
            ++idx;
            SearchNode right = parseTerm();

            SearchNode node;
            node.type = SearchNode::Type::BinaryOp;
            switch (opTok.type)
            {
                case SearchTokenType::OpEqual: node.op = "=="; break;
                case SearchTokenType::OpNotEqual: node.op = "!="; break;
                case SearchTokenType::OpLess: node.op = "<"; break;
                case SearchTokenType::OpLessOrEqual: node.op = "<="; break;
                case SearchTokenType::OpGreater: node.op = ">"; break;
                case SearchTokenType::OpGreaterOrEqual: node.op = ">="; break;
                case SearchTokenType::OpAnd: node.op = "AND"; break;
                case SearchTokenType::OpOr: node.op = "OR"; break;
                default: node.op = "?"; break;
            }
            node.children.push_back(left);
            node.children.push_back(right);
            left = node;
        }

        return left;
    };

    return parseExpression();
}

static std::string ToLowerStr(const std::string& s)
{
    std::string r;
    r.resize(s.size());
    std::transform(s.begin(), s.end(), r.begin(), ::tolower);
    return r;
}

bool ContentBrowserPanel::EvaluateSearchNode(const SearchNode& node, const std::filesystem::path& entryPath, const std::string& filename)
{
    switch (node.type)
    {
        case SearchNode::Type::Identifier:
        {
            std::string lowerId = ToLowerStr(node.identifier);
            std::string lowerName = ToLowerStr(filename);
            std::string pathStr = entryPath.string();
            std::string lowerPath = ToLowerStr(pathStr);
            return lowerName.find(lowerId) != std::string::npos ||
                   lowerPath.find(lowerId) != std::string::npos;
        }

        case SearchNode::Type::String:
        {
            std::string lowerStr = ToLowerStr(node.stringValue);
            std::string lowerName = ToLowerStr(filename);
            return lowerName.find(lowerStr) != std::string::npos;
        }

        case SearchNode::Type::Number:
            return false;

        case SearchNode::Type::Group:
        {
            if (node.children.empty()) return true;
            return EvaluateSearchNode(node.children[0], entryPath, filename);
        }

        case SearchNode::Type::UnaryOp:
        {
            if (node.op == "NOT")
            {
                if (node.children.empty()) return true;
                return !EvaluateSearchNode(node.children[0], entryPath, filename);
            }
            if (node.op == "TEXT_INVERT")
            {
                if (node.children.empty()) return false;
                std::string strVal;
                const SearchNode& child = node.children[0];
                if (child.type == SearchNode::Type::Identifier) strVal = child.identifier;
                else if (child.type == SearchNode::Type::String) strVal = child.stringValue;
                std::string lowerStr = ToLowerStr(strVal);
                std::string lowerName = ToLowerStr(filename);
                return lowerName.find(lowerStr) == std::string::npos;
            }
            if (node.op == "TEXT_EXACT")
            {
                if (node.children.empty()) return false;
                std::string strVal;
                const SearchNode& child = node.children[0];
                if (child.type == SearchNode::Type::Identifier) strVal = child.identifier;
                else if (child.type == SearchNode::Type::String) strVal = child.stringValue;
                return filename == strVal;
            }
            if (node.op == "TEXT_ANCHOR_START")
            {
                std::string lowerStr = ToLowerStr(node.stringValue);
                std::string lowerName = ToLowerStr(filename);
                size_t len = lowerStr.length();
                return lowerName.size() >= len && lowerName.substr(lowerName.size() - len) == lowerStr;
            }
            return true;
        }

        case SearchNode::Type::BinaryOp:
        {
            if (node.children.size() < 2) return true;

            SearchNode left = node.children[0];
            SearchNode right = node.children[1];

            if (node.op == "AND" || node.op == "&")
            {
                return EvaluateSearchNode(left, entryPath, filename) && EvaluateSearchNode(right, entryPath, filename);
            }
            if (node.op == "OR" || node.op == "|")
            {
                return EvaluateSearchNode(left, entryPath, filename) || EvaluateSearchNode(right, entryPath, filename);
            }

            std::string key;
            std::string valueStr;
            double numValue = 0;
            bool isNumeric = false;

            if (left.type == SearchNode::Type::Identifier)
            {
                key = left.identifier;
                if (right.type == SearchNode::Type::Number)
                {
                    numValue = right.numberValue;
                    isNumeric = true;
                }
                else if (right.type == SearchNode::Type::String)
                {
                    valueStr = right.stringValue;
                }
                else if (right.type == SearchNode::Type::Identifier)
                {
                    valueStr = right.identifier;
                }
            }

            std::string lowerKey = ToLowerStr(key);
            std::string lowerValue = ToLowerStr(valueStr);

            if (lowerKey == "name")
            {
                std::string lowerName = ToLowerStr(filename);
                if (node.op == "==" || node.op == ":")
                    return lowerName == lowerValue || lowerName.find(lowerValue) != std::string::npos;
                if (node.op == "!=") return lowerName != lowerValue;
                return false;
            }
            if (lowerKey == "path")
            {
                std::string relPath = std::filesystem::relative(entryPath, std::filesystem::path()).string();
                std::string lowerPath = ToLowerStr(relPath);
                if (node.op == "==" || node.op == ":")
                    return lowerPath == lowerValue || lowerPath.find(lowerValue) != std::string::npos;
                if (node.op == "!=") return lowerPath != lowerValue;
                return false;
            }
            if (lowerKey == "type" || lowerKey == "class")
            {
                std::string ext = entryPath.extension().string();
                std::string lowerExt = ToLowerStr(ext);
                if (node.op == "==" || node.op == ":")
                    return lowerExt == lowerValue || ext == valueStr;
                if (node.op == "!=") return lowerExt != lowerValue;
                return false;
            }

            if (isNumeric)
            {
                return false;
            }

            bool leftResult = EvaluateSearchNode(left, entryPath, filename);
            bool rightResult = EvaluateSearchNode(right, entryPath, filename);

            if (node.op == "==") return leftResult && rightResult;
            if (node.op == "!=") return !(leftResult && rightResult);
            if (node.op == "<") return false;
            if (node.op == "<=") return false;
            if (node.op == ">") return false;
            if (node.op == ">=") return false;

            return leftResult;
        }
    }

    return true;
}

bool ContentBrowserPanel::MatchesSearchExpression(const std::string& query, const std::filesystem::path& entryPath, const std::string& filename)
{
    if (query.empty()) return true;

    SearchNode ast = ParseSearchQuery(query);
    return EvaluateSearchNode(ast, entryPath, filename);
}
