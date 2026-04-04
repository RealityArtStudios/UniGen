// Copyright (c) CreationArtStudios, Khairol Anwar

#include "Renderer.h"
#include <chrono>

#include "Runtime/EngineCore/Window.h"
#include "Runtime/EngineCore/FileSystem/FileSystem.h"
#include "Runtime/EngineCore/Shader/Shader.h"
#include "TextureManager.h"


//TODO: Will move to vulkan specific RHI types
const std::vector<char const*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

constexpr int      MAX_FRAMES_IN_FLIGHT = 2;

Renderer::Renderer(Window* InWindow)
	: RendererWindow(InWindow), VulkanInstanceWrapper(std::make_unique<VulkanInstance>()), SwapChainWrapper(nullptr), BufferManagerWrapper(nullptr), TextureManagerWrapper(nullptr), PipelineManagerWrapper(nullptr), MeshData(nullptr), DescriptorManagerWrapper(nullptr)
{

}

Renderer::~Renderer()
{
   
}

void Renderer::Initialize()
{
    VulkanInstanceWrapper->CreateInstance(RendererWindow);

    SwapChainWrapper = std::make_unique<SwapChain>(VulkanInstanceWrapper.get(), RendererWindow);
    SwapChainWrapper->Create();

    BufferManagerWrapper = std::make_unique<BufferManager>(VulkanInstanceWrapper.get());

    PipelineManagerWrapper = std::make_unique<PipelineManager>(VulkanInstanceWrapper.get(), SwapChainWrapper.get());
    PipelineManagerWrapper->CreateDescriptorSetLayout();
    PipelineManagerWrapper->CreateGraphicsPipeline();

    CommandPoolWrapper = std::make_unique<CommandPool>(VulkanInstanceWrapper.get());
    CreateDepthResources();
    CreateRenderTarget();
    
    TextureManagerWrapper = std::make_unique<TextureManager>(VulkanInstanceWrapper.get(), SwapChainWrapper.get(), BufferManagerWrapper.get(), CommandPoolWrapper->GetCommandPool());
    
    MeshData = std::make_unique<Mesh>();
    
    DescriptorManagerWrapper = std::make_unique<DescriptorManager>(VulkanInstanceWrapper.get(), MAX_FRAMES_IN_FLIGHT);
    std::vector<vk::raii::Buffer> uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;
    BufferManagerWrapper->CreateUniformBuffers(MAX_FRAMES_IN_FLIGHT, uniformBuffers, uniformBuffersMemory, uniformBuffersMapped);
    DescriptorManagerWrapper->SetUniformBuffers(uniformBuffers, uniformBuffersMemory, uniformBuffersMapped);
    DescriptorManagerWrapper->CreateDescriptorPool();
    CommandBufferManagerWrapper = std::make_unique<CommandBufferManager>(VulkanInstanceWrapper.get(), CommandPoolWrapper.get(), MAX_FRAMES_IN_FLIGHT);
    CreateSyncObjects();
    
    ImGuiSystemWrapper = std::make_unique<ImGuiSystem>();
    ImGuiSystemWrapper->Initialize(this, RendererWindow);
    
    VkDescriptorSet renderTargetDescriptor = ImGui_ImplVulkan_AddTexture(
        static_cast<VkSampler>(*renderTargetSampler),
        static_cast<VkImageView>(*renderTargetImageView),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    ImGuiSystemWrapper->SetupViewportDescriptor(renderTargetDescriptor);
}

void Renderer::ReloadSceneData()
{
    VulkanInstanceWrapper->GetLogicalDevice().waitIdle();
    
    Scene& scene = SceneWrapper.GetInternalScene();
    auto view = scene.CreateView<MeshComponent, MaterialComponent>();
    
    int loadedCount = 0;
    for (EntityID entity : view)
    {
        MeshComponent* meshComp = scene.GetComponent<MeshComponent>(entity);
        MaterialComponent* materialComp = scene.GetComponent<MaterialComponent>(entity);
        
        if (!meshComp || meshComp->ModelPath.empty())
            continue;
        
        std::cout << "[Renderer] Loading model: " << meshComp->ModelPath << std::endl;
        
        TextureManagerWrapper->Initialize(materialComp ? materialComp->TexturePath : "");
        
        MeshData = std::make_unique<Mesh>(meshComp->ModelPath);
        
        BufferManagerWrapper->CreateVertexBuffer(MeshData->GetVertices(), VulkanVertexBuffer, VulkanVertexBufferMemory, CommandPoolWrapper->GetCommandPool(), VulkanInstanceWrapper->GetGraphicsQueue());
        BufferManagerWrapper->CreateIndexBuffer(MeshData->GetIndices(), VulkanIndexBuffer, VulkanIndexBufferMemory, CommandPoolWrapper->GetCommandPool(), VulkanInstanceWrapper->GetGraphicsQueue());
        
        DescriptorManagerWrapper->CreateDescriptorSets(PipelineManagerWrapper.get(), TextureManagerWrapper.get(), MeshData.get());
        
        loadedCount++;
        std::cout << "[Renderer] Successfully loaded model for entity: " << entity << std::endl;
        break;
    }
    
    if (loadedCount == 0)
    {
        std::cout << "[Renderer] No entities with mesh components found" << std::endl;
        MeshData.reset();
    }
}

void Renderer::Render()
{
    // Note: inFlightFences, presentCompleteSemaphores, and commandBuffers are indexed by frameIndex,
            //       while renderFinishedSemaphores is indexed by imageIndex
    auto fenceResult = VulkanInstanceWrapper->GetLogicalDevice().waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
    if (fenceResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to wait for fence!");
    }

    auto [result, imageIndex] = SwapChainWrapper->GetSwapChain().acquireNextImage(UINT64_MAX, *VulkanPresentCompleteSemaphores[frameIndex], nullptr);

    // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
    // here and does not need to be caught by an exception.
    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        SwapChainWrapper->Recreate();
        CreateDepthResources();
        CreateRenderTarget();
        DescriptorManagerWrapper->CreateDescriptorSets(PipelineManagerWrapper.get(), TextureManagerWrapper.get(), MeshData.get());
        if (ImGuiSystemWrapper) {
            ImGuiSystemWrapper->CleanupViewport();
            VkDescriptorSet renderTargetDescriptor = ImGui_ImplVulkan_AddTexture(
                static_cast<VkSampler>(*renderTargetSampler),
                static_cast<VkImageView>(*renderTargetImageView),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            ImGuiSystemWrapper->SetupViewportDescriptor(renderTargetDescriptor);
        }
        return;
    }
    // On other success codes than eSuccess and eSuboptimalKHR we just throw an exception.
    // On any error code, aquireNextImage already threw an exception.
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    UpdateUniformBuffer(frameIndex);

    // Only reset the fence if we are submitting work
    VulkanInstanceWrapper->GetLogicalDevice().resetFences(*inFlightFences[frameIndex]);

    CommandBufferManagerWrapper->GetCurrentCommandBuffer(frameIndex).reset();
    recordCommandBuffer(imageIndex);

    vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo   submitInfo;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &*VulkanPresentCompleteSemaphores[frameIndex];
    submitInfo.pWaitDstStageMask = &waitDestinationStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*CommandBufferManagerWrapper->GetCurrentCommandBuffer(frameIndex);
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &*VulkanRenderFinishedSemaphores[imageIndex];

    VulkanInstanceWrapper->GetGraphicsQueue().submit(submitInfo, *inFlightFences[frameIndex]);

    vk::PresentInfoKHR presentInfoKHR;
    presentInfoKHR.waitSemaphoreCount = 1;
    presentInfoKHR.pWaitSemaphores = &*VulkanRenderFinishedSemaphores[imageIndex];
    presentInfoKHR.swapchainCount = 1;
    presentInfoKHR.pSwapchains = &*SwapChainWrapper->GetSwapChain();
    presentInfoKHR.pImageIndices = &imageIndex;

    result = VulkanInstanceWrapper->GetGraphicsQueue().presentKHR(presentInfoKHR);
    // Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
    // here and does not need to be caught by an exception.
    if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || RendererWindow->IsResized())
    {
        RendererWindow->SetResizedFalse();
        SwapChainWrapper->Recreate();
        CreateDepthResources();
        CreateRenderTarget();
    if (MeshData && MeshData->GetIndexCount() > 0)
        {
            DescriptorManagerWrapper->CreateDescriptorSets(PipelineManagerWrapper.get(), TextureManagerWrapper.get(), MeshData.get());
        }
        
        // Recreate the descriptor for ImGui viewport
        if (ImGuiSystemWrapper) {
            ImGuiSystemWrapper->CleanupViewport();
            
            VkDescriptorSet renderTargetDescriptor = ImGui_ImplVulkan_AddTexture(
                static_cast<VkSampler>(*renderTargetSampler),
                static_cast<VkImageView>(*renderTargetImageView),
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            ImGuiSystemWrapper->SetupViewportDescriptor(renderTargetDescriptor);
        }
    }
    else
    {
        // There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
        assert(result == vk::Result::eSuccess);
    }
    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::Shutdown()
{
}

void Renderer::CreateDepthResources()
{
    vk::Format depthFormat = PipelineManagerWrapper->findDepthFormat();
    CreateImage(SwapChainWrapper->GetExtent().width, SwapChainWrapper->GetExtent().height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageImageMemory);
    depthImageView = SwapChainWrapper->CreateImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
}

void Renderer::CreateRenderTarget()
{
    // Clean up old resources
    renderTargetImageView = nullptr;
    renderTargetImage = nullptr;
    renderTargetImageMemory = nullptr;
    renderTargetSampler = nullptr;
    
    auto format = SwapChainWrapper->GetFormat().format;
    auto extent = SwapChainWrapper->GetExtent();
    
    // Create render target image
    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = format;
    imageInfo.extent = vk::Extent3D{ extent.width, extent.height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    
    renderTargetImage = vk::raii::Image(VulkanInstanceWrapper->GetLogicalDevice(), imageInfo);
    
    vk::MemoryRequirements memRequirements = renderTargetImage.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = BufferManagerWrapper->FindMemoryType(memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    
    renderTargetImageMemory = vk::raii::DeviceMemory(VulkanInstanceWrapper->GetLogicalDevice(), allocInfo);
    renderTargetImage.bindMemory(renderTargetImageMemory, 0);
    
    // Create image view
    vk::ImageViewCreateInfo viewInfo;
    viewInfo.image = *renderTargetImage;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
    
    renderTargetImageView = vk::raii::ImageView(VulkanInstanceWrapper->GetLogicalDevice(), viewInfo);
    
    // Create sampler
    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.borderColor = vk::BorderColor::eFloatTransparentBlack;
    
    renderTargetSampler = vk::raii::Sampler(VulkanInstanceWrapper->GetLogicalDevice(), samplerInfo);
}

void Renderer::CreateImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::raii::Image& image, vk::raii::DeviceMemory& imageMemory)
{
    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = format,
        imageInfo.extent = vk::Extent3D{ width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1,
        imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = tiling;
    imageInfo.usage = usage;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;

    image = vk::raii::Image(VulkanInstanceWrapper->GetLogicalDevice(), imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = BufferManagerWrapper->FindMemoryType(memRequirements.memoryTypeBits, properties);

    imageMemory = vk::raii::DeviceMemory(VulkanInstanceWrapper->GetLogicalDevice(), allocInfo);
    image.bindMemory(imageMemory, 0);
}

void Renderer::CreateSyncObjects()
{
    assert(VulkanPresentCompleteSemaphores.empty() && VulkanRenderFinishedSemaphores.empty() && inFlightFences.empty());

    for (size_t i = 0; i < SwapChainWrapper->GetImages().size(); i++)
    {
        VulkanRenderFinishedSemaphores.emplace_back(VulkanInstanceWrapper->GetLogicalDevice(), vk::SemaphoreCreateInfo());
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::FenceCreateInfo FenceCreateInfo;
        FenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;

        VulkanPresentCompleteSemaphores.emplace_back(VulkanInstanceWrapper->GetLogicalDevice(), vk::SemaphoreCreateInfo());
        inFlightFences.emplace_back(VulkanInstanceWrapper->GetLogicalDevice(), FenceCreateInfo);
    }
};

void Renderer::UpdateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto  currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(currentTime - startTime).count();

    Mesh::UniformBufferObject ubo{};
    ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(SwapChainWrapper->GetExtent().width) / static_cast<float>(SwapChainWrapper->GetExtent().height), 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    DescriptorManagerWrapper->UpdateUniformBuffer(currentImage, ubo);
}

void Renderer::recordCommandBuffer(uint32_t imageIndex)
{
    auto& commandBuffer = CommandBufferManagerWrapper->GetCurrentCommandBuffer(frameIndex);
    commandBuffer.begin({});
    
    // Transition render target to color attachment for rendering
    if (renderTargetImage != nullptr) {
        transition_image_layout(
            *renderTargetImage,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            {},
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::PipelineStageFlagBits2::eTopOfPipe,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::ImageAspectFlagBits::eColor);
    }
    
    // Transition depth image to depth attachment optimal layout
    transition_image_layout(
        *depthImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::ImageAspectFlagBits::eDepth);

    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

    // Render to render target for ImGui viewport instead of swapchain
    vk::RenderingAttachmentInfo colorAttachmentInfo;
    colorAttachmentInfo.setImageView(renderTargetImageView);
    colorAttachmentInfo.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachmentInfo.setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachmentInfo.setClearValue(clearColor);

    vk::RenderingAttachmentInfo depthAttachmentInfo;
    depthAttachmentInfo.setImageView(depthImageView);
    depthAttachmentInfo.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);
    depthAttachmentInfo.setLoadOp(vk::AttachmentLoadOp::eClear);
    depthAttachmentInfo.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthAttachmentInfo.setClearValue(clearDepth);

    vk::RenderingInfo renderingInfo;

    vk::Rect2D rect2d;
    rect2d.offset = vk::Offset2D{ 0, 0 };
    rect2d.extent = SwapChainWrapper->GetExtent();

    renderingInfo.renderArea = rect2d;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;

    commandBuffer.beginRendering(renderingInfo);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *PipelineManagerWrapper->GetGraphicsPipeline());
    commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(SwapChainWrapper->GetExtent().width), static_cast<float>(SwapChainWrapper->GetExtent().height), 0.0f, 1.0f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), SwapChainWrapper->GetExtent()));
    
    if (MeshData && MeshData->GetIndexCount() > 0)
    {
        commandBuffer.bindVertexBuffers(0, *VulkanVertexBuffer, { 0 });
        commandBuffer.bindIndexBuffer(*VulkanIndexBuffer, 0, vk::IndexType::eUint32);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *PipelineManagerWrapper->GetPipelineLayout(), 0, *DescriptorManagerWrapper->GetCurrentDescriptorSet(frameIndex), nullptr);
        commandBuffer.drawIndexed(MeshData->GetIndexCount(), 1, 0, 0, 0);
    }
    
    commandBuffer.endRendering();
    
    // Transition render target to shader read for ImGui viewport
    if (renderTargetImage != nullptr) {
        transition_image_layout(
            *renderTargetImage,
            vk::ImageLayout::eColorAttachmentOptimal,
            vk::ImageLayout::eShaderReadOnlyOptimal,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            {},
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::PipelineStageFlagBits2::eFragmentShader,
            vk::ImageAspectFlagBits::eColor);
    }
    
    // Render ImGui UI to swapchain
    if (ImGuiSystemWrapper) {
        ImGuiSystemWrapper->NewFrame();
    }
    
    // Transition swapchain to color attachment for ImGui rendering
    transition_image_layout(
        SwapChainWrapper->GetImages()[imageIndex],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::ImageAspectFlagBits::eColor);
    
    // Create ImGui rendering attachment
    vk::RenderingAttachmentInfo imguiColorAttachment;
    imguiColorAttachment.setImageView(SwapChainWrapper->GetImageView(imageIndex));
    imguiColorAttachment.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    imguiColorAttachment.setLoadOp(vk::AttachmentLoadOp::eLoad);
    imguiColorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    
    vk::RenderingInfo imguiRenderingInfo;
    imguiRenderingInfo.renderArea = vk::Rect2D{ vk::Offset2D{0, 0}, SwapChainWrapper->GetExtent() };
    imguiRenderingInfo.layerCount = 1;
    imguiRenderingInfo.colorAttachmentCount = 1;
    imguiRenderingInfo.pColorAttachments = &imguiColorAttachment;
    
    commandBuffer.beginRendering(imguiRenderingInfo);
    
    if (ImGuiSystemWrapper) {
        ImGuiSystemWrapper->Render(commandBuffer, frameIndex);
    }
    
    commandBuffer.endRendering();
    
    // Transition swapchain to present
    transition_image_layout(
        SwapChainWrapper->GetImages()[imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::ImageAspectFlagBits::eColor);
    
    commandBuffer.end();
}

void Renderer::transition_image_layout(
    vk::Image               image,
    vk::ImageLayout         old_layout,
    vk::ImageLayout         new_layout,
    vk::AccessFlags2        src_access_mask,
    vk::AccessFlags2        dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask,
    vk::ImageAspectFlags    image_aspect_flags)
{
    vk::ImageMemoryBarrier2 barrier;
    barrier.srcStageMask = src_stage_mask;
    barrier.srcAccessMask = src_access_mask;
    barrier.dstStageMask = dst_stage_mask;
    barrier.dstAccessMask = dst_access_mask;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {
                image_aspect_flags,
                 0,
                1,
                0,
                1 };

    vk::DependencyInfo dependency_info;
    dependency_info.dependencyFlags = {};
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &barrier;

    CommandBufferManagerWrapper->GetCurrentCommandBuffer(frameIndex).pipelineBarrier2(dependency_info);
}