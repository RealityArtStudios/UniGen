// Copyright (c) CreationArtStudios

#define NOMINMAX
#include "ProjectLauncherPanel.h"

#include <imgui.h>
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>   // SHBrowseForFolder / SHGetPathFromIDList
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
//  Construction
// ─────────────────────────────────────────────────────────────────────────────
ProjectLauncherPanel::ProjectLauncherPanel()
{
    // Built-in project templates (mirrors the Game/ folder structure)
    m_Templates = {
        {
            "Blank",
            "An empty project with no content.\n"
            "A default scene and the standard directory layout are created.\n"
            "Best for starting from scratch."
        },
    };
}

// ─────────────────────────────────────────────────────────────────────────────
//  OnRender – called every ImGui frame while the launcher is active
// ─────────────────────────────────────────────────────────────────────────────
void ProjectLauncherPanel::OnRender()
{
    ImGuiIO& io      = ImGui::GetIO();
    ImVec2   display = io.DisplaySize;

    // ── Full-screen dimmed background ─────────────────────────────────────────
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.08f, 0.92f));
    ImGui::Begin("##LauncherBg", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoInputs     |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::End();
    ImGui::PopStyleColor();

    // ── Centred launcher window ───────────────────────────────────────────────
    constexpr float WIN_W = 820.0f;
    constexpr float WIN_H = 520.0f;

    ImGui::SetNextWindowPos(
        ImVec2((display.x - WIN_W) * 0.5f, (display.y - WIN_H) * 0.5f),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(WIN_W, WIN_H), ImGuiCond_Always);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,  8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,    ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,      ImVec4(0.30f, 0.30f, 0.40f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,     ImVec4(0.10f, 0.10f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.14f, 0.14f, 0.18f, 1.0f));

    ImGui::Begin("UniGen Project Manager", nullptr,
        ImGuiWindowFlags_NoResize    |
        ImGuiWindowFlags_NoMove      |
        ImGuiWindowFlags_NoCollapse  |
        ImGuiWindowFlags_NoSavedSettings);

    ImGui::PopStyleVar(3);

    // ── Header strip ─────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.12f, 1.0f));
    ImGui::BeginChild("##Header", ImVec2(0.0f, 56.0f), false, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(18.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.95f, 1.0f));
    ImGui::SetWindowFontScale(1.4f);
    ImGui::Text("UniGen Project Manager");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();

    ImGui::SetCursorPos(ImVec2(18.0f, 36.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.65f, 1.0f));
    ImGui::Text("Open an existing project or create one from a template.");
    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg

    ImGui::Separator();

    // ── Tab bar ───────────────────────────────────────────────────────────────
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(14.0f, 6.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(4.0f,  4.0f));
    ImGui::PushStyleColor(ImGuiCol_Tab,              ImVec4(0.14f, 0.14f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,       ImVec4(0.24f, 0.24f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabSelected,      ImVec4(0.20f, 0.40f, 0.70f, 1.0f));

    ImGui::SetCursorPosX(16.0f);
    if (ImGui::BeginTabBar("##LauncherTabs"))
    {
        if (ImGui::BeginTabItem("  Open Existing  "))
        {
            m_ActiveTab = 0;
            RenderOpenTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("  Create New  "))
        {
            m_ActiveTab = 1;
            RenderCreateTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    ImGui::End();
    ImGui::PopStyleColor(4);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Open-existing tab
// ─────────────────────────────────────────────────────────────────────────────
void ProjectLauncherPanel::RenderOpenTab()
{
    ImGui::SetCursorPos(ImVec2(16.0f, ImGui::GetCursorPosY() + 24.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.65f, 0.75f, 1.0f));
    ImGui::TextWrapped(
        "Browse to a .ungproject file to open it in the editor.\n"
        "The project's asset directory will be set as the content browser root.");
    ImGui::PopStyleColor();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 28.0f);
    ImGui::SetCursorPosX(16.0f);

    // ── Browse button ─────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.40f, 0.70f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.50f, 0.80f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.16f, 0.34f, 0.62f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    if (ImGui::Button("Browse for Project...", ImVec2(200.0f, 36.0f)))
    {
        std::filesystem::path picked;
        if (BrowseForProjectFile(picked))
        {
            if (OnOpenProject)
                OnOpenProject(picked);
        }
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::SetCursorPosX(16.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.40f, 0.50f, 1.0f));
    ImGui::Text("Supported format: .ungproject");
    ImGui::PopStyleColor();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Create-new tab
// ─────────────────────────────────────────────────────────────────────────────
void ProjectLauncherPanel::RenderCreateTab()
{
    const float COL_LEFT  = 16.0f;
    const float LABEL_W   = 110.0f;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 14.0f);

    // ── Template list (left column) ───────────────────────────────────────────
    ImGui::SetCursorPosX(COL_LEFT);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.80f, 0.90f, 1.0f));
    ImGui::Text("TEMPLATES");
    ImGui::PopStyleColor();

    ImGui::SetCursorPosX(COL_LEFT);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.13f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::BeginChild("##TemplateList", ImVec2(180.0f, 200.0f), true);

    for (int i = 0; i < static_cast<int>(m_Templates.size()); ++i)
    {
        bool selected = (m_SelectedTemplate == i);
        ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.20f, 0.40f, 0.70f, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.24f, 0.46f, 0.76f, 0.80f));
        if (ImGui::Selectable(m_Templates[i].name.c_str(), selected,
                              ImGuiSelectableFlags_None, ImVec2(0.0f, 32.0f)))
        {
            m_SelectedTemplate = i;
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // ── Template description (right of list) ──────────────────────────────────
    float listBottom = ImGui::GetCursorPosY();
    ImGui::SameLine();
    float descX = ImGui::GetCursorPosX();

    ImGui::SetCursorPos(ImVec2(descX, ImGui::GetCursorPosY() - 200.0f - ImGui::GetStyle().ItemSpacing.y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.13f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::BeginChild("##TemplateDesc", ImVec2(0.0f, 200.0f), true);

    if (m_SelectedTemplate < static_cast<int>(m_Templates.size()))
    {
        const auto& tmpl = m_Templates[m_SelectedTemplate];
        ImGui::SetCursorPos(ImVec2(10.0f, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.95f, 1.0f));
        ImGui::Text("%s", tmpl.name.c_str());
        ImGui::PopStyleColor();
        ImGui::SetCursorPosX(10.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.70f, 1.0f));
        ImGui::TextWrapped("%s", tmpl.description.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // ── Project Name ──────────────────────────────────────────────────────────
    ImGui::SetCursorPosY(listBottom + 14.0f);
    ImGui::SetCursorPosX(COL_LEFT);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.70f, 0.80f, 1.0f));
    ImGui::Text("Project Name");
    ImGui::PopStyleColor();
    ImGui::SameLine(COL_LEFT + LABEL_W);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.14f, 0.14f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  ImVec4(0.18f, 0.18f, 0.23f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 16.0f);
    ImGui::InputText("##ProjName", m_ProjectName, sizeof(m_ProjectName));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    // ── Location ──────────────────────────────────────────────────────────────
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
    ImGui::SetCursorPosX(COL_LEFT);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.70f, 0.80f, 1.0f));
    ImGui::Text("Location");
    ImGui::PopStyleColor();
    ImGui::SameLine(COL_LEFT + LABEL_W);

    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.14f, 0.14f, 0.18f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    float browseW  = 90.0f;
    float inputW   = ImGui::GetContentRegionAvail().x - browseW - 8.0f - 16.0f;
    ImGui::SetNextItemWidth(inputW);
    ImGui::InputText("##ProjLoc",
        const_cast<char*>(m_LocationDisplay.c_str()),
        m_LocationDisplay.size() + 1,
        ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.22f, 0.28f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.38f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    if (ImGui::Button("Browse...", ImVec2(browseW, 0.0f)))
    {
        std::filesystem::path folder;
        if (BrowseForFolder(folder))
        {
            m_ProjectLocation = folder;
            m_LocationDisplay = folder.string();
        }
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    // ── Error message ─────────────────────────────────────────────────────────
    if (!m_ErrorMessage.empty())
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
        ImGui::SetCursorPosX(COL_LEFT);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
        ImGui::TextWrapped("%s", m_ErrorMessage.c_str());
        ImGui::PopStyleColor();
    }

    // ── Create button (bottom-right) ──────────────────────────────────────────
    float btnW = 130.0f, btnH = 36.0f;
    float btnX = ImGui::GetWindowWidth() - btnW - 16.0f;
    float btnY = ImGui::GetWindowHeight() - btnH - 14.0f;
    ImGui::SetCursorPos(ImVec2(btnX, btnY));

    bool canCreate = (m_ProjectName[0] != '\0') && !m_ProjectLocation.empty();

    if (!canCreate)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.45f);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.20f, 0.55f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.64f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.16f, 0.46f, 0.20f, 1.0f));
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

    if (ImGui::Button("Create Project", ImVec2(btnW, btnH)) && canCreate)
    {
        std::string name = m_ProjectName;

        // Basic validation
        if (name.find_first_of("/\\:*?\"<>|") != std::string::npos)
        {
            m_ErrorMessage = "Project name contains invalid characters.";
        }
        else
        {
            std::filesystem::path target = m_ProjectLocation / name;
            if (std::filesystem::exists(target))
            {
                m_ErrorMessage = "A folder named '" + name + "' already exists in that location.";
            }
            else
            {
                m_ErrorMessage.clear();
                if (OnCreateProject)
                    OnCreateProject(name, m_ProjectLocation);
            }
        }
    }

    ImGui::PopStyleVar(); // FrameRounding

    if (!canCreate)
    {
        ImGui::PopStyleVar(); // Alpha
    }
    else
    {
        ImGui::PopStyleColor(3);
    }

    // Hint under the create button
    if (!canCreate)
    {
        ImGui::SetCursorPos(ImVec2(btnX - 80.0f, btnY + btnH + 2.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.55f, 1.0f));
        ImGui::Text("Enter a name and choose a location first.");
        ImGui::PopStyleColor();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Native file-dialog helpers
// ─────────────────────────────────────────────────────────────────────────────
bool ProjectLauncherPanel::BrowseForProjectFile(std::filesystem::path& outPath)
{
    OPENFILENAMEA ofn = {};
    CHAR szFile[260]  = {};

    ofn.lStructSize  = sizeof(OPENFILENAME);
    ofn.lpstrFile    = szFile;
    ofn.nMaxFile     = sizeof(szFile);
    ofn.lpstrFilter  = "UniGen Project (*.ungproject)\0*.ungproject\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle   = "Open UniGen Project";
    ofn.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn))
    {
        outPath = szFile;
        return true;
    }
    return false;
}

bool ProjectLauncherPanel::BrowseForFolder(std::filesystem::path& outPath)
{
    BROWSEINFOA bi   = {};
    bi.lpszTitle     = "Select project parent folder";
    bi.ulFlags       = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl)
    {
        CHAR szPath[MAX_PATH] = {};
        if (SHGetPathFromIDListA(pidl, szPath))
        {
            outPath = szPath;
            CoTaskMemFree(pidl);
            return true;
        }
        CoTaskMemFree(pidl);
    }
    return false;
}
