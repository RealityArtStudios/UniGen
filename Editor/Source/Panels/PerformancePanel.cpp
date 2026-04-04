// Copyright (c) CreationArtStudios

#include "PerformancePanel.h"

#include "Runtime/EngineCore/Renderer/Renderer.h"
#include "imgui.h"

PerformancePanel::PerformancePanel(Renderer* renderer)
    : renderer(renderer)
{
}

void PerformancePanel::OnImGuiRender()
{
    if (!renderer)
        return;

    FrameStats& stats = renderer->GetFrameStats();
    
    float currentFrameTime = stats.GetFrameTime();
    float currentFPS = stats.GetFPS();
    float currentGPUTime = stats.GetGPUTime();
    
    smoothedFrameTime = smoothedFrameTime * (1.0f - smoothingFactor) + currentFrameTime * smoothingFactor;
    smoothedFPS = smoothedFPS * (1.0f - smoothingFactor) + currentFPS * smoothingFactor;
    smoothedGPUTime = smoothedGPUTime * (1.0f - smoothingFactor) + currentGPUTime * smoothingFactor;
    
    frameTimeHistory[historyIndex] = currentFrameTime;
    historyIndex = (historyIndex + 1) % NUM_SAMPLES;
    if (sampleCount < NUM_SAMPLES)
        sampleCount++;
    
    smoothedGPUTime = smoothedGPUTime * (1.0f - smoothingFactor) + currentGPUTime * smoothingFactor;
    smoothedGameThread = smoothedGameThread * (1.0f - smoothingFactor) + stats.GetGameThread() * smoothingFactor;
    smoothedRenderThread = smoothedRenderThread * (1.0f - smoothingFactor) + stats.GetRenderThread() * smoothingFactor;
    
    ImGui::Begin("Performance", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    
    ImGui::Text("Renderer Performance");
    ImGui::Separator();
    
    ImGui::Text("Frametime:  %.2f ms", smoothedFrameTime);
    ImGui::SameLine();
    if (smoothedFrameTime < 10.0f) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Text("[Good]");
    } else if (smoothedFrameTime < 16.0f) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Text("[Fair]");
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::Text("[Poor]");
    }
    ImGui::PopStyleColor();
    
    ImGui::Text("FPS:        %.1f", smoothedFPS);
    ImGui::SameLine();
    if (smoothedFPS >= 60.0f) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Text("[Good]");
    } else if (smoothedFPS >= 30.0f) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        ImGui::Text("[Fair]");
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::Text("[Poor]");
    }
    ImGui::PopStyleColor();
    
    ImGui::Text("GPU Time:   %.2f ms", smoothedGPUTime);
    
    ImGui::Separator();
    ImGui::Text("CPU Pipeline Timing");
    ImGui::Separator();
    
    ImGui::Text("Wait GPU: %.2f ms", stats.GetCPUWaitGpu());
    ImGui::Text("Acquire:  %.2f ms", stats.GetCPUAcquire());
    ImGui::Text("Record:   %.2f ms", stats.GetCPURecord());
    ImGui::Text("Submit:   %.2f ms", stats.GetCPUSubmit());
    ImGui::Text("Present:  %.2f ms", stats.GetCPUPresent());
    
    float totalCpu = stats.GetCPUWaitGpu() + stats.GetCPUAcquire() + stats.GetCPURecord() + stats.GetCPUSubmit() + stats.GetCPUPresent();
    ImGui::Separator();
    ImGui::Text("CPU Total: %.2f ms", totalCpu);
    
    ImGui::Separator();
    ImGui::Text("Thread Timing");
    ImGui::Separator();
    ImGui::Text("Game:    %.2f ms", stats.GetGameThread());
    ImGui::Text("Draw:    %.2f ms", stats.GetRenderThread());
    
    ImGui::Separator();
    ImGui::Text("Memory");
    ImGui::Separator();
    
    uint64_t gpuMemUsed = stats.GetGPUMemoryUsed();
    uint64_t gpuMemBudget = stats.GetGPUMemoryBudget();
    if (gpuMemBudget > 0) {
        float gpuUsedMb = static_cast<float>(gpuMemUsed) / (1024.0f * 1024.0f);
        float gpuBudgetMb = static_cast<float>(gpuMemBudget) / (1024.0f * 1024.0f);
        ImGui::Text("GPU:     %.1f / %.1f MB", gpuUsedMb, gpuBudgetMb);
    } else {
        ImGui::Text("GPU:     N/A");
    }
    
    uint64_t sysMemUsed = stats.GetSystemMemoryUsed();
    if (sysMemUsed > 0) {
        float sysUsedMb = static_cast<float>(sysMemUsed) / (1024.0f * 1024.0f);
        ImGui::Text("System:  %.1f MB", sysUsedMb);
    } else {
        ImGui::Text("System:  N/A");
    }
    
    ImGui::Separator();
    
    ImGui::Text("Frame Count: %llu", static_cast<unsigned long long>(stats.GetFrameCount()));
    
    if (sampleCount > 1) {
        ImGui::Separator();
        ImGui::Text("Frametime Graph:");
        
        float graphWidth = 250.0f;
        float graphHeight = 60.0f;
        
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        ImGui::BeginChild("FrameTimeGraph", ImVec2(graphWidth, graphHeight), false, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 graphMin = ImGui::GetWindowPos();
        ImVec2 graphMax = ImVec2(graphMin.x + graphWidth, graphMin.y + graphHeight);
        
        drawList->AddRectFilled(graphMin, graphMax, IM_COL32(30, 30, 30, 255));
        
        float maxTime = 33.3f;
        
        for (int i = 0; i < sampleCount - 1; ++i) {
            int idx = (historyIndex - sampleCount + i + NUM_SAMPLES) % NUM_SAMPLES;
            int nextIdx = (idx + 1) % NUM_SAMPLES;
            
            float x1 = graphMin.x + (static_cast<float>(i) / static_cast<float>(NUM_SAMPLES - 1)) * graphWidth;
            float x2 = graphMin.x + (static_cast<float>(i + 1) / static_cast<float>(NUM_SAMPLES - 1)) * graphWidth;
            
            float y1 = graphMax.y - (frameTimeHistory[idx] / maxTime) * graphHeight;
            float y2 = graphMax.y - (frameTimeHistory[nextIdx] / maxTime) * graphHeight;
            
            y1 = std::max(graphMin.y + 1.0f, std::min(graphMax.y - 1.0f, y1));
            y2 = std::max(graphMin.y + 1.0f, std::min(graphMax.y - 1.0f, y2));
            
            ImU32 color = IM_COL32(0, 255, 128, 255);
            if (frameTimeHistory[idx] > 16.6f) color = IM_COL32(255, 255, 0, 255);
            if (frameTimeHistory[idx] > 33.3f) color = IM_COL32(255, 0, 0, 255);
            
            drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, 2.0f);
        }
        
        drawList->AddLine(ImVec2(graphMin.x, graphMax.y - (16.6f / maxTime) * graphHeight), ImVec2(graphMax.x, graphMax.y - (16.6f / maxTime) * graphHeight), IM_COL32(100, 100, 100, 100), 1.0f);
        
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    
    ImGui::End();
}
