// Copyright (c) CreationArtStudios, Khairol Anwar

#include "Renderer.h"
#include <chrono>

#include "Runtime/EngineCore/Window.h"
#include "Runtime/EngineCore/FileSystem/FileSystem.h"
#include "Runtime/EngineCore/Shader/Shader.h"
#include "TextureManager.h"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

const std::vector<char const*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// ─────────────────────────────────────────────────────────────────────────────
Renderer::Renderer(Window* InWindow)
    : RendererWindow(InWindow)
    , VulkanInstanceWrapper(std::make_unique<VulkanInstance>())
{}

Renderer::~Renderer() {}

// ─────────────────────────────────────────────────────────────────────────────
void Renderer::CreateDefaultTexture(const std::string& path)
{
    if (std::filesystem::exists(path)) return;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    // Minimal valid 1×1 white PNG
    static const uint8_t whitePng[] = {
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
        0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,
        0xDE,0x00,0x00,0x00,0x0C,0x49,0x44,0x41,
        0x54,0x08,0xD7,0x63,0xF8,0xFF,0xFF,0x3F,
        0x00,0x05,0xFE,0x02,0xFE,0xDC,0xCC,0x59,
        0xE7,0x00,0x00,0x00,0x00,0x49,0x45,0x4E,
        0x44,0xAE,0x42,0x60,0x82 };

    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(whitePng), sizeof(whitePng));
}

// ─────────────────────────────────────────────────────────────────────────────
void Renderer::Initialize()
{
    VulkanInstanceWrapper->CreateInstance(RendererWindow);

    SwapChainWrapper = std::make_unique<SwapChain>(VulkanInstanceWrapper.get(), RendererWindow);
    SwapChainWrapper->Create();

    BufferManagerWrapper  = std::make_unique<BufferManager>(VulkanInstanceWrapper.get());
    PipelineManagerWrapper = std::make_unique<PipelineManager>(VulkanInstanceWrapper.get(), SwapChainWrapper.get());
    PipelineManagerWrapper->CreateDescriptorSetLayout();
    PipelineManagerWrapper->CreateGraphicsPipeline();

    CommandPoolWrapper = std::make_unique<CommandPool>(VulkanInstanceWrapper.get());
    CreateDepthResources();
    CreateRenderTarget();

    CreateDefaultTexture("../Engine/Content/Textures/default.png");
    TextureManagerWrapper = std::make_unique<TextureManager>(
        VulkanInstanceWrapper.get(), SwapChainWrapper.get(),
        BufferManagerWrapper.get(), CommandPoolWrapper->GetCommandPool());

    MeshData = std::make_unique<Mesh>();

    DescriptorManagerWrapper = std::make_unique<DescriptorManager>(VulkanInstanceWrapper.get(), MAX_FRAMES_IN_FLIGHT);
    std::vector<vk::raii::Buffer>       uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void*>                  uniformBuffersMapped;
    BufferManagerWrapper->CreateUniformBuffers(
        MAX_FRAMES_IN_FLIGHT, uniformBuffers, uniformBuffersMemory, uniformBuffersMapped);
    DescriptorManagerWrapper->SetUniformBuffers(uniformBuffers, uniformBuffersMemory, uniformBuffersMapped);
    DescriptorManagerWrapper->CreateDescriptorPool();

    CommandBufferManagerWrapper = std::make_unique<CommandBufferManager>(
        VulkanInstanceWrapper.get(), CommandPoolWrapper.get(), MAX_FRAMES_IN_FLIGHT);
    CreateSyncObjects();
    CreateTimestampQueryPool();

    ImGuiSystemWrapper = std::make_unique<ImGuiSystem>();
    ImGuiSystemWrapper->Initialize(this, RendererWindow);

    VkDescriptorSet rtDesc = ImGui_ImplVulkan_AddTexture(
        static_cast<VkSampler>   (*renderTargetSampler),
        static_cast<VkImageView>  (*renderTargetImageView),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    ImGuiSystemWrapper->SetupViewportDescriptor(rtDesc);
}

// ─────────────────────────────────────────────────────────────────────────────
//  BuildMaterialGpuData
//  Creates one TextureManager + MAX_FRAMES_IN_FLIGHT descriptor sets for a
//  single material's base-color texture.
// ─────────────────────────────────────────────────────────────────────────────
Renderer::PerMaterialGpuData Renderer::BuildMaterialGpuData(const std::string& texturePath)
{
    PerMaterialGpuData data;

    // ── Texture ───────────────────────────────────────────────────────────────
    data.TextureMgr = std::make_unique<TextureManager>(
        VulkanInstanceWrapper.get(), SwapChainWrapper.get(),
        BufferManagerWrapper.get(), CommandPoolWrapper->GetCommandPool());
    try
    {
        data.TextureMgr->Initialize(texturePath);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Renderer] Texture load failed (" << texturePath
                  << "): " << e.what() << " — using default" << std::endl;
        data.TextureMgr->Initialize("../Engine/Content/Textures/default.png");
    }

    // ── Descriptor pool ───────────────────────────────────────────────────────
    std::array poolSizes = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer,         MAX_FRAMES_IN_FLIGHT),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler,  MAX_FRAMES_IN_FLIGHT)
    };
    vk::DescriptorPoolCreateInfo poolCI;
    poolCI.flags          = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolCI.maxSets        = MAX_FRAMES_IN_FLIGHT;
    poolCI.poolSizeCount  = static_cast<uint32_t>(poolSizes.size());
    poolCI.pPoolSizes     = poolSizes.data();
    data.DescPool = vk::raii::DescriptorPool(VulkanInstanceWrapper->GetLogicalDevice(), poolCI);

    // ── Descriptor sets ───────────────────────────────────────────────────────
    std::vector<vk::DescriptorSetLayout> layouts(
        MAX_FRAMES_IN_FLIGHT, *PipelineManagerWrapper->GetDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool      = *data.DescPool;
    allocInfo.descriptorSetCount  = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts         = layouts.data();
    data.DescSets = VulkanInstanceWrapper->GetLogicalDevice().allocateDescriptorSets(allocInfo);

    // ── Update (bind UBO + texture) ───────────────────────────────────────────
    for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f)
    {
        vk::DescriptorBufferInfo bufInfo;
        bufInfo.buffer = DescriptorManagerWrapper->GetUniformBuffer(f);
        bufInfo.offset = 0;
        bufInfo.range  = sizeof(Mesh::UniformBufferObject);

        vk::DescriptorImageInfo imgInfo;
        imgInfo.sampler     = *data.TextureMgr->GetTextureSampler();
        imgInfo.imageView   = *data.TextureMgr->GetTextureImageView();
        imgInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        std::array writes = {
            vk::WriteDescriptorSet(*data.DescSets[f], 0, 0, 1,
                vk::DescriptorType::eUniformBuffer, nullptr, &bufInfo),
            vk::WriteDescriptorSet(*data.DescSets[f], 1, 0, 1,
                vk::DescriptorType::eCombinedImageSampler, &imgInfo)
        };
        VulkanInstanceWrapper->GetLogicalDevice().updateDescriptorSets(writes, {});
    }

    return data;
}

// ─────────────────────────────────────────────────────────────────────────────
void Renderer::ReloadSceneData()
{
    VulkanInstanceWrapper->GetLogicalDevice().waitIdle();

    // Release previous per-material GPU resources
    m_MaterialGpuData.clear();

    Scene& scene = SceneWrapper.GetInternalScene();
    auto   view  = scene.CreateView<MeshComponent, MaterialComponent>();

    int loadedCount = 0;
    for (EntityID entity : view)
    {
        MeshComponent*    meshComp    = scene.GetComponent<MeshComponent>(entity);
        MaterialComponent* materialComp = scene.GetComponent<MaterialComponent>(entity);

        if (!meshComp || meshComp->ModelPath.empty())
            continue;

        std::cout << "[Renderer] Loading model: " << meshComp->ModelPath << std::endl;

        // ── 1. Load geometry (extracts materials + submeshes) ─────────────────
        MeshData = std::make_unique<Mesh>(meshComp->ModelPath);

        // ── 2. Upload vertex / index buffers ──────────────────────────────────
        BufferManagerWrapper->CreateVertexBuffer(
            MeshData->GetVertices(), VulkanVertexBuffer, VulkanVertexBufferMemory,
            CommandPoolWrapper->GetCommandPool(), VulkanInstanceWrapper->GetGraphicsQueue());
        BufferManagerWrapper->CreateIndexBuffer(
            MeshData->GetIndices(), VulkanIndexBuffer, VulkanIndexBufferMemory,
            CommandPoolWrapper->GetCommandPool(), VulkanInstanceWrapper->GetGraphicsQueue());

        // ── 3. Build per-material GPU data ────────────────────────────────────
        const auto& gltfMats = MeshData->GetMaterials();

        if (!gltfMats.empty())
        {
            m_MaterialGpuData.reserve(gltfMats.size());
            for (size_t i = 0; i < gltfMats.size(); ++i)
            {
                const GltfMaterial& gMat = gltfMats[i];

                // Priority: scene-file override > GLTF base-color > default
                std::string texPath = "../Engine/Content/Textures/default.png";

                if (materialComp && !materialComp->TexturePath.empty())
                    texPath = materialComp->TexturePath;
                else if (!gMat.BaseColorTexturePath.empty()
                         && std::filesystem::exists(gMat.BaseColorTexturePath))
                    texPath = gMat.BaseColorTexturePath;

                std::cout << "[Renderer] Mat[" << i << "] \"" << gMat.Name
                          << "\" -> " << std::filesystem::path(texPath).filename().string()
                          << std::endl;

                m_MaterialGpuData.push_back(BuildMaterialGpuData(texPath));
            }
        }
        else
        {
            // No GLTF materials — create one fallback entry
            std::string texPath = "../Engine/Content/Textures/default.png";
            if (materialComp && !materialComp->TexturePath.empty())
                texPath = materialComp->TexturePath;

            m_MaterialGpuData.push_back(BuildMaterialGpuData(texPath));

            // Keep legacy DescriptorManager sets in sync for the fallback path
            DescriptorManagerWrapper->CreateDescriptorSets(
                PipelineManagerWrapper.get(), m_MaterialGpuData[0].TextureMgr.get(), MeshData.get());
        }

        ++loadedCount;
        std::cout << "[Renderer] Loaded entity " << entity
                  << " — " << m_MaterialGpuData.size() << " material(s)" << std::endl;
        break; // one entity per scene for now
    }

    if (loadedCount == 0)
    {
        std::cout << "[Renderer] No entities with mesh components found" << std::endl;
        MeshData.reset();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void Renderer::Render()
{
    static auto frameStartTime = std::chrono::high_resolution_clock::now();
    auto currentTime  = std::chrono::high_resolution_clock::now();
    auto duration     = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - frameStartTime);
    float frameTimeMs = static_cast<float>(duration.count()) / 1000.0f;
    frameStartTime    = currentTime;

    float fps = (frameTimeMs > 0.0f) ? 1000.0f / frameTimeMs : 0.0f;
    frameStats.frameTimeMs.store(frameTimeMs);
    frameStats.fps.store(fps);

    auto waitStart   = std::chrono::high_resolution_clock::now();
    auto fenceResult = VulkanInstanceWrapper->GetLogicalDevice().waitForFences(
        *inFlightFences[frameIndex], vk::True, UINT64_MAX);
    auto waitEnd     = std::chrono::high_resolution_clock::now();
    frameStats.cpuWaitGpuMs.store(
        static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(waitEnd - waitStart).count()) / 1000.0f);

    if (fenceResult != vk::Result::eSuccess)
        throw std::runtime_error("failed to wait for fence!");

    auto acquireStart = std::chrono::high_resolution_clock::now();
    auto [result, imageIndex] = SwapChainWrapper->GetSwapChain().acquireNextImage(
        UINT64_MAX, *VulkanPresentCompleteSemaphores[frameIndex], nullptr);
    auto acquireEnd = std::chrono::high_resolution_clock::now();
    frameStats.cpuAcquireMs.store(
        static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(acquireEnd - acquireStart).count()) / 1000.0f);

    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        SwapChainWrapper->Recreate();
        CreateDepthResources();
        CreateRenderTarget();
        // Rebuild descriptor sets for all materials
        for (auto& matData : m_MaterialGpuData)
        {
            // Descriptor sets remain valid across swapchain recreation;
            // only the render-target descriptor for the viewport needs refresh.
        }
        if (ImGuiSystemWrapper)
        {
            ImGuiSystemWrapper->CleanupViewport();
            VkDescriptorSet rtDesc = ImGui_ImplVulkan_AddTexture(
                static_cast<VkSampler>  (*renderTargetSampler),
                static_cast<VkImageView>(*renderTargetImageView),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            ImGuiSystemWrapper->SetupViewportDescriptor(rtDesc);
        }
        return;
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    UpdateUniformBuffer(frameIndex);
    VulkanInstanceWrapper->GetLogicalDevice().resetFences(*inFlightFences[frameIndex]);
    CommandBufferManagerWrapper->GetCurrentCommandBuffer(frameIndex).reset();

    auto recordStart = std::chrono::high_resolution_clock::now();
    recordCommandBuffer(imageIndex);
    auto recordEnd = std::chrono::high_resolution_clock::now();
    frameStats.cpuRecordMs.store(
        static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(recordEnd - recordStart).count()) / 1000.0f);

    vk::PipelineStageFlags waitMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submitInfo;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &*VulkanPresentCompleteSemaphores[frameIndex];
    submitInfo.pWaitDstStageMask    = &waitMask;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &*CommandBufferManagerWrapper->GetCurrentCommandBuffer(frameIndex);
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &*VulkanRenderFinishedSemaphores[imageIndex];

    auto submitStart = std::chrono::high_resolution_clock::now();
    VulkanInstanceWrapper->GetGraphicsQueue().submit(submitInfo, *inFlightFences[frameIndex]);
    auto submitEnd = std::chrono::high_resolution_clock::now();
    frameStats.cpuSubmitMs.store(
        static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(submitEnd - submitStart).count()) / 1000.0f);

    vk::PresentInfoKHR presentInfo;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &*VulkanRenderFinishedSemaphores[imageIndex];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &*SwapChainWrapper->GetSwapChain();
    presentInfo.pImageIndices      = &imageIndex;

    auto presentStart = std::chrono::high_resolution_clock::now();
    result = VulkanInstanceWrapper->GetGraphicsQueue().presentKHR(presentInfo);
    auto presentEnd = std::chrono::high_resolution_clock::now();
    frameStats.cpuPresentMs.store(
        static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(presentEnd - presentStart).count()) / 1000.0f);

    if (result == vk::Result::eSuboptimalKHR ||
        result == vk::Result::eErrorOutOfDateKHR ||
        RendererWindow->IsResized())
    {
        RendererWindow->SetResizedFalse();
        SwapChainWrapper->Recreate();
        CreateDepthResources();
        CreateRenderTarget();

        if (ImGuiSystemWrapper)
        {
            ImGuiSystemWrapper->CleanupViewport();
            VkDescriptorSet rtDesc = ImGui_ImplVulkan_AddTexture(
                static_cast<VkSampler>  (*renderTargetSampler),
                static_cast<VkImageView>(*renderTargetImageView),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            ImGuiSystemWrapper->SetupViewportDescriptor(rtDesc);
        }
    }
    else
    {
        assert(result == vk::Result::eSuccess);
    }

    CalculateFrameStats();
    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

// ─────────────────────────────────────────────────────────────────────────────
void Renderer::Shutdown() {}

// ─────────────────────────────────────────────────────────────────────────────
void Renderer::UpdateUniformBuffer(uint32_t currentImage)
{
    // ── Camera input (only when ImGui isn't consuming it) ────────────────────
    const bool imguiWantsMouse = ImGuiSystemWrapper && ImGuiSystemWrapper->WantCaptureMouse();

    if (!imguiWantsMouse)
    {
        // Scroll wheel → zoom
        double scroll = RendererWindow->GetScrollDelta();
        if (scroll != 0.0)
        {
            m_CamDistance -= static_cast<float>(scroll) * 0.8f;
            m_CamDistance = glm::clamp(m_CamDistance, 0.5f, 500.0f);
            RendererWindow->ConsumeScroll();
        }

        // Right-mouse drag → orbit
        if (RendererWindow->isMouseButtonHeld(GLFW_MOUSE_BUTTON_RIGHT))
        {
            m_CamYaw -= RendererWindow->deltaMouseX * 0.25f;
            m_CamPitch += RendererWindow->deltaMouseY * 0.25f;
            m_CamPitch = glm::clamp(m_CamPitch, -89.0f, 89.0f);
        }

        // Middle-mouse drag → pan target
        if (RendererWindow->isMouseButtonHeld(GLFW_MOUSE_BUTTON_MIDDLE))
        {
            // Build right / up vectors for current orientation
            float yawRad = glm::radians(m_CamYaw);
            float pitchRad = glm::radians(m_CamPitch);
            glm::vec3 forward = glm::normalize(glm::vec3(
                cos(pitchRad) * sin(yawRad),
                cos(pitchRad) * cos(yawRad),
                sin(pitchRad)));
            glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 0, 1)));
            glm::vec3 up = glm::cross(right, forward);

            float panSpeed = m_CamDistance * 0.001f;
            m_CamTarget -= right * static_cast<float>(RendererWindow->deltaMouseX) * panSpeed;
            m_CamTarget += up * static_cast<float>(RendererWindow->deltaMouseY) * panSpeed;
        }
    }

    // ── Build view matrix from spherical coordinates (Z-up) ─────────────────
    float yawRad = glm::radians(m_CamYaw);
    float pitchRad = glm::radians(m_CamPitch);

    glm::vec3 camPos = m_CamTarget + glm::vec3(
        m_CamDistance * cos(pitchRad) * sin(yawRad),
        m_CamDistance * cos(pitchRad) * cos(yawRad),
        m_CamDistance * sin(pitchRad));

    Mesh::UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = glm::lookAt(camPos, m_CamTarget, glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(
        glm::radians(60.0f),
        static_cast<float>(SwapChainWrapper->GetExtent().width) /
        static_cast<float>(SwapChainWrapper->GetExtent().height),
        0.05f, 1000.0f);
    ubo.proj[1][1] *= -1;

    DescriptorManagerWrapper->UpdateUniformBuffer(currentImage, ubo);
}
// ─────────────────────────────────────────────────────────────────────────────
void Renderer::recordCommandBuffer(uint32_t imageIndex)
{
    auto& cmd = CommandBufferManagerWrapper->GetCurrentCommandBuffer(frameIndex);
    cmd.begin({});

    cmd.resetQueryPool(*renderTimestampQueryPool, 0, 2);
    cmd.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *renderTimestampQueryPool, 0);

    // Render target: Undefined → ColorAttachment
    if (renderTargetImage != nullptr)
    {
        transition_image_layout(*renderTargetImage,
            vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
            {}, vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::ImageAspectFlagBits::eColor);
    }

    // Depth: Undefined → DepthAttachment
    transition_image_layout(*depthImage,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::ImageAspectFlagBits::eDepth);

    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderingAttachmentInfo colorAttach;
    colorAttach.setImageView(renderTargetImageView);
    colorAttach.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttach.setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttach.setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttach.setClearValue(clearColor);

    vk::RenderingAttachmentInfo depthAttach;
    depthAttach.setImageView(depthImageView);
    depthAttach.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);
    depthAttach.setLoadOp(vk::AttachmentLoadOp::eClear);
    depthAttach.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthAttach.setClearValue(clearDepth);

    vk::RenderingInfo ri;
    ri.renderArea            = vk::Rect2D{ {0,0}, SwapChainWrapper->GetExtent() };
    ri.layerCount            = 1;
    ri.colorAttachmentCount  = 1;
    ri.pColorAttachments     = &colorAttach;
    ri.pDepthAttachment      = &depthAttach;

    cmd.beginRendering(ri);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *PipelineManagerWrapper->GetGraphicsPipeline());
    cmd.setViewport(0, vk::Viewport(0, 0,
        static_cast<float>(SwapChainWrapper->GetExtent().width),
        static_cast<float>(SwapChainWrapper->GetExtent().height), 0, 1));
    cmd.setScissor(0, vk::Rect2D({0,0}, SwapChainWrapper->GetExtent()));

    if (MeshData && MeshData->GetIndexCount() > 0)
    {
        cmd.bindVertexBuffers(0, *VulkanVertexBuffer, {0});
        cmd.bindIndexBuffer(*VulkanIndexBuffer, 0, vk::IndexType::eUint32);

        const auto& submeshes = MeshData->GetSubMeshes();

        if (!submeshes.empty() && !m_MaterialGpuData.empty())
        {
            // ── Per-submesh draw with correct material descriptor set ──────────
            for (const auto& sm : submeshes)
            {
                // Clamp material index to valid range; -1 (unassigned) uses 0
                int matIdx = sm.MaterialIndex;
                if (matIdx < 0 || matIdx >= static_cast<int>(m_MaterialGpuData.size()))
                    matIdx = 0;

                cmd.bindDescriptorSets(
                    vk::PipelineBindPoint::eGraphics,
                    *PipelineManagerWrapper->GetPipelineLayout(),
                    0,
                    *m_MaterialGpuData[matIdx].DescSets[frameIndex],
                    nullptr);
                cmd.drawIndexed(sm.IndexCount, 1, sm.FirstIndex, 0, 0);
            }
        }
        else
        {
            // ── Legacy path: no submesh info — single draw ────────────────────
            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                *PipelineManagerWrapper->GetPipelineLayout(),
                0,
                *DescriptorManagerWrapper->GetCurrentDescriptorSet(frameIndex),
                nullptr);
            cmd.drawIndexed(MeshData->GetIndexCount(), 1, 0, 0, 0);
        }
    }

    cmd.endRendering();

    // Render target: ColorAttachment → ShaderRead (for ImGui viewport)
    if (renderTargetImage != nullptr)
    {
        transition_image_layout(*renderTargetImage,
            vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits2::eColorAttachmentWrite, {},
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::ImageAspectFlagBits::eColor);
    }

    if (ImGuiSystemWrapper) ImGuiSystemWrapper->NewFrame();

    // Swapchain: Undefined → ColorAttachment (for ImGui)
    transition_image_layout(SwapChainWrapper->GetImages()[imageIndex],
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
        {}, vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor);

    vk::RenderingAttachmentInfo imguiColor;
    imguiColor.setImageView(SwapChainWrapper->GetImageView(imageIndex));
    imguiColor.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    imguiColor.setLoadOp(vk::AttachmentLoadOp::eLoad);
    imguiColor.setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfo imguiRI;
    imguiRI.renderArea           = vk::Rect2D{ {0,0}, SwapChainWrapper->GetExtent() };
    imguiRI.layerCount           = 1;
    imguiRI.colorAttachmentCount = 1;
    imguiRI.pColorAttachments    = &imguiColor;

    cmd.beginRendering(imguiRI);
    if (ImGuiSystemWrapper) ImGuiSystemWrapper->Render(cmd, frameIndex);
    cmd.endRendering();

    // Swapchain: ColorAttachment → Present
    transition_image_layout(SwapChainWrapper->GetImages()[imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite, {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::ImageAspectFlagBits::eColor);

    cmd.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *renderTimestampQueryPool, 1);
    cmd.end();
}

// ── Remaining functions are unchanged from your original ─────────────────────

void Renderer::CreateDepthResources()
{
    vk::Format depthFormat = PipelineManagerWrapper->findDepthFormat();
    CreateImage(SwapChainWrapper->GetExtent().width, SwapChainWrapper->GetExtent().height,
        depthFormat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eDepthStencilAttachment,
        vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageImageMemory);
    depthImageView = SwapChainWrapper->CreateImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
}

void Renderer::CreateRenderTarget()
{
    renderTargetImageView = nullptr;
    renderTargetImage     = nullptr;
    renderTargetImageMemory = nullptr;
    renderTargetSampler   = nullptr;

    auto format = SwapChainWrapper->GetFormat().format;
    auto extent = SwapChainWrapper->GetExtent();

    vk::ImageCreateInfo imgCI;
    imgCI.imageType   = vk::ImageType::e2D;
    imgCI.format      = format;
    imgCI.extent      = vk::Extent3D{ extent.width, extent.height, 1 };
    imgCI.mipLevels   = 1;
    imgCI.arrayLayers = 1;
    imgCI.samples     = vk::SampleCountFlagBits::e1;
    imgCI.tiling      = vk::ImageTiling::eOptimal;
    imgCI.usage       = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    imgCI.sharingMode = vk::SharingMode::eExclusive;
    imgCI.initialLayout = vk::ImageLayout::eUndefined;
    renderTargetImage = vk::raii::Image(VulkanInstanceWrapper->GetLogicalDevice(), imgCI);

    auto memReq = renderTargetImage.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = BufferManagerWrapper->FindMemoryType(
        memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    renderTargetImageMemory = vk::raii::DeviceMemory(VulkanInstanceWrapper->GetLogicalDevice(), allocInfo);
    renderTargetImage.bindMemory(renderTargetImageMemory, 0);

    vk::ImageViewCreateInfo viewCI;
    viewCI.image            = *renderTargetImage;
    viewCI.viewType         = vk::ImageViewType::e2D;
    viewCI.format           = format;
    viewCI.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    renderTargetImageView = vk::raii::ImageView(VulkanInstanceWrapper->GetLogicalDevice(), viewCI);

    vk::SamplerCreateInfo sampCI;
    sampCI.magFilter    = vk::Filter::eLinear;
    sampCI.minFilter    = vk::Filter::eLinear;
    sampCI.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    sampCI.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    sampCI.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    sampCI.borderColor  = vk::BorderColor::eFloatTransparentBlack;
    renderTargetSampler = vk::raii::Sampler(VulkanInstanceWrapper->GetLogicalDevice(), sampCI);
}

void Renderer::CreateImage(uint32_t width, uint32_t height, vk::Format format,
    vk::ImageTiling tiling, vk::ImageUsageFlags usage,
    vk::MemoryPropertyFlags properties,
    vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory)
{
    vk::ImageCreateInfo imgCI;
    imgCI.imageType     = vk::ImageType::e2D;
    imgCI.format        = format;
    imgCI.extent        = vk::Extent3D{ width, height, 1 };
    imgCI.mipLevels     = 1;
    imgCI.arrayLayers   = 1;
    imgCI.samples       = vk::SampleCountFlagBits::e1;
    imgCI.tiling        = tiling;
    imgCI.usage         = usage;
    imgCI.sharingMode   = vk::SharingMode::eExclusive;
    imgCI.initialLayout = vk::ImageLayout::eUndefined;
    image = vk::raii::Image(VulkanInstanceWrapper->GetLogicalDevice(), imgCI);

    auto memReq = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = BufferManagerWrapper->FindMemoryType(memReq.memoryTypeBits, properties);
    imageMemory = vk::raii::DeviceMemory(VulkanInstanceWrapper->GetLogicalDevice(), allocInfo);
    image.bindMemory(imageMemory, 0);
}

void Renderer::CreateSyncObjects()
{
    assert(VulkanPresentCompleteSemaphores.empty() &&
           VulkanRenderFinishedSemaphores.empty()  &&
           inFlightFences.empty());

    for (size_t i = 0; i < SwapChainWrapper->GetImages().size(); ++i)
        VulkanRenderFinishedSemaphores.emplace_back(VulkanInstanceWrapper->GetLogicalDevice(), vk::SemaphoreCreateInfo());

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vk::FenceCreateInfo fci;
        fci.flags = vk::FenceCreateFlagBits::eSignaled;
        VulkanPresentCompleteSemaphores.emplace_back(VulkanInstanceWrapper->GetLogicalDevice(), vk::SemaphoreCreateInfo());
        inFlightFences.emplace_back(VulkanInstanceWrapper->GetLogicalDevice(), fci);
    }
}

void Renderer::transition_image_layout(vk::Image image,
    vk::ImageLayout old_layout, vk::ImageLayout new_layout,
    vk::AccessFlags2 src_access, vk::AccessFlags2 dst_access,
    vk::PipelineStageFlags2 src_stage, vk::PipelineStageFlags2 dst_stage,
    vk::ImageAspectFlags aspect)
{
    vk::ImageMemoryBarrier2 barrier;
    barrier.srcStageMask        = src_stage;
    barrier.srcAccessMask       = src_access;
    barrier.dstStageMask        = dst_stage;
    barrier.dstAccessMask       = dst_access;
    barrier.oldLayout           = old_layout;
    barrier.newLayout           = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = { aspect, 0, 1, 0, 1 };

    vk::DependencyInfo di;
    di.imageMemoryBarrierCount = 1;
    di.pImageMemoryBarriers    = &barrier;
    CommandBufferManagerWrapper->GetCurrentCommandBuffer(frameIndex).pipelineBarrier2(di);
}

void Renderer::CreateTimestampQueryPool()
{
    vk::QueryPoolCreateInfo qi;
    qi.queryType  = vk::QueryType::eTimestamp;
    qi.queryCount = 2;
    renderTimestampQueryPool = vk::raii::QueryPool(VulkanInstanceWrapper->GetLogicalDevice(), qi);
}

void Renderer::CalculateFrameStats()
{
    float tsPeriod = VulkanInstanceWrapper->GetPhysicalDevice().getProperties().limits.timestampPeriod;
    vkGetQueryPoolResults(*VulkanInstanceWrapper->GetLogicalDevice(),
        *renderTimestampQueryPool, 0, 2,
        sizeof(uint64_t) * 2, timestampResults.data(), sizeof(uint64_t),
        VK_QUERY_RESULT_WAIT_BIT);
    frameStats.gpuTimeMs.store(
        static_cast<float>(timestampResults[1] - timestampResults[0]) * tsPeriod / 1'000'000.0f);
    frameStats.frameCount.fetch_add(1);
}

void Renderer::UpdateMemoryStats()
{
    auto props = VulkanInstanceWrapper->GetPhysicalDevice().getProperties();
    frameStats.gpuMemoryBudget.store(props.limits.maxMemoryAllocationCount * 1024ULL);
    frameStats.gpuMemoryUsed.store(0);
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms))
        frameStats.systemMemoryUsed.store(ms.ullTotalPhys - ms.ullAvailPhys);
#endif
}
