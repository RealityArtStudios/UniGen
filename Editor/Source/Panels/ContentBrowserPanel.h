#pragma once

#include <filesystem>
#include <string>
#include <functional>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#   include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <vulkan/vulkan.h>

class Renderer;

// ─────────────────────────────────────────────────────────────────────────────
//  PanelTexture
// ─────────────────────────────────────────────────────────────────────────────
struct PanelTexture
{
    vk::raii::Image        image      = nullptr;
    vk::raii::DeviceMemory memory     = nullptr;
    vk::raii::ImageView    imageView  = nullptr;
    vk::raii::Sampler      sampler    = nullptr;
    VkDescriptorSet        descriptor = VK_NULL_HANDLE;

    bool IsValid() const { return descriptor != VK_NULL_HANDLE; }
};

// ─────────────────────────────────────────────────────────────────────────────
//  ContentBrowserPanel
// ─────────────────────────────────────────────────────────────────────────────
class ContentBrowserPanel
{
public:
    ContentBrowserPanel() = default;
    explicit ContentBrowserPanel(Renderer* renderer);
    ~ContentBrowserPanel() = default;

    void SetBaseDirectory(const std::filesystem::path& path);
    void OnRender();
    void RefreshIcons();

    void OpenAddDialog();
    void OpenImportDialog();

    std::function<void(std::filesystem::path)> OnLoadScene;
    std::function<void(std::filesystem::path)> OnSaveScene;

    // ─────────────────────────────────────────────────────────────────────────────
    //  Advanced Search System
    // ─────────────────────────────────────────────────────────────────────────────
    enum class SearchTokenType
    {
        Identifier,
        String,
        Number,
        OpEqual,
        OpNotEqual,
        OpLess,
        OpLessOrEqual,
        OpGreater,
        OpGreaterOrEqual,
        OpAnd,
        OpOr,
        OpNot,
        OpTextInvert,
        OpTextExact,
        OpTextAnchorStart,
        OpTextAnchorEnd,
        LParen,
        RParen,
        End
    };

    struct SearchToken
    {
        SearchTokenType type = SearchTokenType::End;
        std::string value;
        double numberValue = 0.0;
    };

    struct SearchNode
    {
        enum class Type
        {
            Identifier,
            String,
            Number,
            BinaryOp,
            UnaryOp,
            Group
        } type;

        std::string identifier;
        std::string stringValue;
        double numberValue = 0.0;
        std::string op;
        std::vector<SearchNode> children;
    };

    SearchNode ParseSearchQuery(const std::string& query);
    bool EvaluateSearchNode(const SearchNode& node, const std::filesystem::path& entryPath, const std::string& filename);
    bool MatchesSearchExpression(const std::string& query, const std::filesystem::path& entryPath, const std::string& filename);

private:
    PanelTexture LoadTextureFromFile(const std::string& filepath);
    PanelTexture MakeSolidColourTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    PanelTexture UploadToGPU(const uint8_t* pixels, uint32_t width, uint32_t height);

    Renderer*             m_Renderer      = nullptr;
    std::filesystem::path m_BaseDirectory;
    std::filesystem::path m_CurrentDirectory;
    std::string           m_SearchQuery;
    char                  m_SearchBuffer[256];

    PanelTexture          m_FolderIcon;
    PanelTexture          m_FileIcon;
    PanelTexture          m_MenuIcon;

    float                 m_ThumbnailSize = 96.0f;
    float                 m_Padding       = 16.0f;
    bool                  m_ShowSettings  = false;

    std::vector<std::filesystem::path> m_NavigationHistory;
    int                               m_HistoryIndex = -1;

    bool                              m_ShowSourcesPanel = true;
    bool                              m_SourcesPanelExpanded = true;

    enum class ViewMode { Tiles, List, Columns };
    ViewMode                          m_ViewMode = ViewMode::Tiles;

    enum class ThumbnailSize { Tiny = 48, Small = 64, Medium = 96, Large = 128, Huge = 192 };
    ThumbnailSize                     m_ThumbnailSizePreset = ThumbnailSize::Medium;
    bool                              m_RealTimeThumbnails = true;

    bool                              m_ShowFolders = true;
    bool                              m_ShowEmptyFolders = true;
    bool                              m_FilterRecursively = false;
    bool                              m_OrganizeFolders = false;
    bool                              m_LockContentBrowser = false;

    bool                              m_ShowCPlusPlusClasses = false;
    bool                              m_ShowDevelopersContent = false;
    bool                              m_ShowEngineContent = false;
    bool                              m_ShowPluginContent = false;
    bool                              m_ShowLocalizedContent = false;

    bool                              m_SearchAssetNames = true;
    bool                              m_SearchAssetPaths = true;
    bool                              m_SearchCollections = true;

    std::string                       m_CurrentPathDisplay;

    struct Filter
    {
        std::string name;
        std::string category;
        std::string extension;
        bool enabled = false;
    };

    struct Collection
    {
        enum class Type { Local, Private, Shared };
        std::string name;
        Type type = Type::Local;
        std::vector<std::filesystem::path> assets;
    };

    std::vector<Filter>               m_AvailableFilters;
    std::vector<Filter>               m_ActiveFilters;
    std::vector<Collection>            m_Collections;
    Collection*                       m_SelectedCollection = nullptr;
    std::filesystem::path             m_SelectedAsset;

    std::vector<std::filesystem::path> m_RecentAssets;
    static constexpr int               MAX_RECENT_ASSETS = 20;
};