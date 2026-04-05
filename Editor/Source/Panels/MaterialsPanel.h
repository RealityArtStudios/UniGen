// Copyright (c) CreationArtStudios
#pragma once

#include <memory>

class Renderer;

// ─────────────────────────────────────────────────────────────────────────────
//  MaterialsPanel
//  Editor panel that displays all GltfMaterial entries extracted from the
//  currently loaded mesh.  Left pane = material list; right pane = properties.
// ─────────────────────────────────────────────────────────────────────────────
class MaterialsPanel
{
public:
    MaterialsPanel() = default;
    explicit MaterialsPanel(Renderer* renderer);
    ~MaterialsPanel() = default;

    void OnImGuiRender();

private:
    Renderer* m_Renderer = nullptr;
    int       m_Selected = 0;
};
