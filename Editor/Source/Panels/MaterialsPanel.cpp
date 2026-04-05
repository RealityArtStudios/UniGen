// Copyright (c) CreationArtStudios
#include "MaterialsPanel.h"

#include "Runtime/EngineCore/Renderer/Renderer.h"
#include "Runtime/EngineCore/Renderer/Material.h"

#include <imgui.h>
#include <filesystem>
#include <string>

MaterialsPanel::MaterialsPanel(Renderer* renderer)
    : m_Renderer(renderer)
{}

// ─────────────────────────────────────────────────────────────────────────────
static void ShowTextureLine(const char* label, const std::string& path)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);

    if (path.empty())
    {
        ImGui::TextDisabled("—");
        return;
    }

    std::string fname = std::filesystem::path(path).filename().string();
    ImGui::TextUnformatted(fname.c_str());
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", path.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
void MaterialsPanel::OnImGuiRender()
{
    if (!m_Renderer)
        return;

    const std::vector<GltfMaterial>* mats = m_Renderer->GetLoadedMaterials();

    ImGui::Begin("Materials");

    if (!mats || mats->empty())
    {
        ImGui::TextDisabled("No materials loaded.");
        ImGui::End();
        return;
    }

    ImGui::Text("%d material(s)", static_cast<int>(mats->size()));
    ImGui::Separator();

    // ── Left pane: material list ──────────────────────────────────────────────
    const float listWidth = 200.0f;
    ImGui::BeginChild("##MatList", ImVec2(listWidth, 0.0f), true);

    for (int i = 0; i < static_cast<int>(mats->size()); ++i)
    {
        const auto& mat = (*mats)[i];

        // Colour-code by alpha mode
        ImVec4 tint = ImGui::GetStyle().Colors[ImGuiCol_Text];
        if      (mat.AlphaMode == GltfMaterial::AlphaMode::Mask)  tint = ImVec4(1.0f, 0.85f, 0.4f, 1.0f);
        else if (mat.AlphaMode == GltfMaterial::AlphaMode::Blend) tint = ImVec4(0.4f, 0.8f,  1.0f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Text, tint);
        bool selected = (m_Selected == i);
        if (ImGui::Selectable(mat.Name.c_str(), selected, 0, ImVec2(0.0f, 0.0f)))
            m_Selected = i;
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // ── Right pane: properties ────────────────────────────────────────────────
    ImGui::BeginChild("##MatProps", ImVec2(0.0f, 0.0f), true);

    if (m_Selected >= 0 && m_Selected < static_cast<int>(mats->size()))
    {
        const GltfMaterial& mat = (*mats)[m_Selected];

        // Header
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 1.0f, 1.0f));
        ImGui::Text("%s", mat.Name.c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Surface flags
        const char* alphaModeStr[] = { "Opaque", "Mask", "Blend" };
        ImGui::Text("Alpha Mode:   %s", alphaModeStr[static_cast<int>(mat.AlphaMode)]);
        if (mat.AlphaMode == GltfMaterial::AlphaMode::Mask)
            ImGui::Text("Alpha Cutoff: %.3f", mat.AlphaCutoff);
        ImGui::Text("Double Sided: %s", mat.DoubleSided ? "Yes" : "No");

        // PBR factors
        ImGui::Spacing();
        ImGui::TextDisabled("PBR Factors");
        ImGui::Separator();

        float col4[4] = {
            mat.BaseColorFactor.r, mat.BaseColorFactor.g,
            mat.BaseColorFactor.b, mat.BaseColorFactor.a };
        ImGui::ColorButton("##bcf", ImVec4(col4[0], col4[1], col4[2], col4[3]),
            ImGuiColorEditFlags_AlphaPreview, ImVec2(16, 16));
        ImGui::SameLine();
        ImGui::Text("Base Color  (%.2f, %.2f, %.2f, %.2f)",
            col4[0], col4[1], col4[2], col4[3]);

        ImGui::Text("Metallic    %.3f", mat.MetallicFactor);
        ImGui::Text("Roughness   %.3f", mat.RoughnessFactor);
        ImGui::Text("Emissive    (%.2f, %.2f, %.2f)",
            mat.EmissiveFactor.r, mat.EmissiveFactor.g, mat.EmissiveFactor.b);
        ImGui::Text("Occlusion   strength %.2f", mat.OcclusionStrength);
        ImGui::Text("Normal      scale    %.2f", mat.NormalScale);

        // Texture paths
        ImGui::Spacing();
        ImGui::TextDisabled("Textures");
        ImGui::Separator();

        if (ImGui::BeginTable("##TexTable", 2,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("File",    ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            ShowTextureLine("Base Color",          mat.BaseColorTexturePath);
            ShowTextureLine("Metallic/Roughness",  mat.MetallicRoughnessTexturePath);
            ShowTextureLine("Normal Map",           mat.NormalMapPath);
            ShowTextureLine("Emissive",             mat.EmissiveTexturePath);
            ShowTextureLine("Occlusion",            mat.OcclusionTexturePath);

            ImGui::EndTable();
        }
    }

    ImGui::EndChild();
    ImGui::End();
}
