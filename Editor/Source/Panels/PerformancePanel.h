// Copyright (c) CreationArtStudios

#pragma once

#include <string>
#include <memory>

class Renderer;
class FrameStats;

class PerformancePanel
{
public:
    PerformancePanel() = default;
    explicit PerformancePanel(Renderer* renderer);
    ~PerformancePanel() = default;

    void OnImGuiRender();

private:
    Renderer* renderer = nullptr;
    
    float smoothedFrameTime = 0.0f;
    float smoothedFPS = 0.0f;
    float smoothedGPUTime = 0.0f;
    float smoothedGameThread = 0.0f;
    float smoothedRenderThread = 0.0f;
    float smoothingFactor = 0.1f;
    
    static constexpr int NUM_SAMPLES = 120;
    float frameTimeHistory[NUM_SAMPLES] = {};
    int historyIndex = 0;
    int sampleCount = 0;
};
