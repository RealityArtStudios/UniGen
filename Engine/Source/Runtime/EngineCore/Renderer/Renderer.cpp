// Copyright (c) CreationArtStudios, Khairol Anwar

#include "Renderer.h"
#include <chrono>

#include "Runtime/EngineCore/Window.h"
#include "Runtime/EngineCore/FileSystem/FileSystem.h"
#include "Runtime/EngineCore/Shader/Shader.h"
#include "TextureManager.h"


//TODO: File system and ecs load objedct and set the filepath
const std::string MODEL_PATH = /*"../Engine/Content/Models/viking_room.obj"*/ "../Engine/Content/Models/viking_room.glb";
const std::string TEXTURE_PATH = "../Engine/Content/Textures/viking_room.png";

//TODO: Will move to vulkan specific RHI types
const std::vector<char const*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

constexpr int      MAX_FRAMES_IN_FLIGHT = 2;

Renderer::Renderer(Window* InWindow)
	: RendererWindow(InWindow), VulkanInstanceWrapper(std::make_unique<VulkanInstance>()), SwapChainWrapper(nullptr), BufferManagerWrapper(nullptr), TextureManagerWrapper(nullptr), PipelineManagerWrapper(nullptr), MeshData(nullptr)
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

    CreateCommandPool();
    CreateDepthResources();
    TextureManagerWrapper = std::make_unique<TextureManager>(VulkanInstanceWrapper.get(), SwapChainWrapper.get(), BufferManagerWrapper.get(), VulkanCommandPool);
    TextureManagerWrapper->Initialize(TEXTURE_PATH);
    
    MeshData = std::make_unique<Mesh>(MODEL_PATH);
    
    BufferManagerWrapper->CreateVertexBuffer(MeshData->GetVertices(), VulkanVertexBuffer, VulkanVertexBufferMemory, VulkanCommandPool, VulkanInstanceWrapper->GetGraphicsQueue());
    BufferManagerWrapper->CreateIndexBuffer(MeshData->GetIndices(), VulkanIndexBuffer, VulkanIndexBufferMemory, VulkanCommandPool, VulkanInstanceWrapper->GetGraphicsQueue());
    BufferManagerWrapper->CreateUniformBuffers(MAX_FRAMES_IN_FLIGHT, VulkanUniformBuffers, VulkanUniformBuffersMemory, VulkanUniformBuffersMapped);
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffers();
    CreateSyncObjects();
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

    VulkanCommandBuffers[frameIndex].reset();
    recordCommandBuffer(imageIndex);

    vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo   submitInfo;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &*VulkanPresentCompleteSemaphores[frameIndex];
    submitInfo.pWaitDstStageMask = &waitDestinationStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*VulkanCommandBuffers[frameIndex];
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

void Renderer::CreateCommandPool()
{
    vk::CommandPoolCreateInfo poolInfo;
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    poolInfo.queueFamilyIndex = VulkanInstanceWrapper->GetQueueFamilyIndex();

    VulkanCommandPool = vk::raii::CommandPool(VulkanInstanceWrapper->GetLogicalDevice(), poolInfo);
}

void Renderer::CreateDepthResources()
{
    vk::Format depthFormat = PipelineManagerWrapper->findDepthFormat();
    CreateImage(SwapChainWrapper->GetExtent().width, SwapChainWrapper->GetExtent().height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage, depthImageMemory);
    depthImageView = SwapChainWrapper->CreateImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
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

void Renderer::CreateDescriptorPool()
{
    std::array poolSize{
     vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
     vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT)
    };
    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSize.size());
    poolInfo.pPoolSizes = poolSize.data();

    VulkanDescriptorPool = vk::raii::DescriptorPool(VulkanInstanceWrapper->GetLogicalDevice(), poolInfo);
}

void Renderer::CreateDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *PipelineManagerWrapper->GetDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = VulkanDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    VulkanDescriptorSets.clear();
    VulkanDescriptorSets = VulkanInstanceWrapper->GetLogicalDevice().allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::DescriptorBufferInfo bufferInfo;
        bufferInfo.buffer = VulkanUniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Mesh::UniformBufferObject);

        vk::DescriptorImageInfo imageInfo;
        imageInfo.sampler = *TextureManagerWrapper->GetTextureSampler();
        imageInfo.imageView = *TextureManagerWrapper->GetTextureImageView();
        imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        vk::WriteDescriptorSet bufferdescriptorWrite;
        bufferdescriptorWrite.dstSet = VulkanDescriptorSets[i];
        bufferdescriptorWrite.dstBinding = 0;
        bufferdescriptorWrite.dstArrayElement = 0;
        bufferdescriptorWrite.descriptorCount = 1;
        bufferdescriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
        bufferdescriptorWrite.pBufferInfo = &bufferInfo;

        vk::WriteDescriptorSet imageinfodescriptorWrite;
        imageinfodescriptorWrite.dstSet = VulkanDescriptorSets[i];
        imageinfodescriptorWrite.dstBinding = 1;
        imageinfodescriptorWrite.dstArrayElement = 0;
        imageinfodescriptorWrite.descriptorCount = 1;
        imageinfodescriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        imageinfodescriptorWrite.pImageInfo = &imageInfo;

        std::array descriptorWrites{ bufferdescriptorWrite , imageinfodescriptorWrite };
        VulkanInstanceWrapper->GetLogicalDevice().updateDescriptorSets(descriptorWrites, {});
    }
}

void Renderer::CreateCommandBuffers()
{
    VulkanCommandBuffers.clear();

    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.commandPool = VulkanCommandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    VulkanCommandBuffers = vk::raii::CommandBuffers(VulkanInstanceWrapper->GetLogicalDevice(), allocInfo);
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

    memcpy(VulkanUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Renderer::recordCommandBuffer(uint32_t imageIndex)
{
    auto& commandBuffer = VulkanCommandBuffers[frameIndex];
    commandBuffer.begin({});
    // Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
    transition_image_layout(
        SwapChainWrapper->GetImages()[imageIndex],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},                                                        // srcAccessMask (no need to wait for previous operations)
        vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // dstStage
        vk::ImageAspectFlagBits::eColor);
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

    vk::RenderingAttachmentInfo colorAttachmentInfo;
    colorAttachmentInfo.setImageView(SwapChainWrapper->GetImageView(imageIndex));
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
    commandBuffer.bindVertexBuffers(0, *VulkanVertexBuffer, { 0 });
    commandBuffer.bindIndexBuffer(*VulkanIndexBuffer, 0, vk::IndexType::eUint32);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *PipelineManagerWrapper->GetPipelineLayout(), 0, *VulkanDescriptorSets[frameIndex], nullptr);
    commandBuffer.drawIndexed(MeshData->GetIndexCount(), 1, 0, 0, 0);
    commandBuffer.endRendering();
    // After rendering, transition the swapchain image to PRESENT_SRC
    transition_image_layout(
        SwapChainWrapper->GetImages()[imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
        {},                                                        // dstAccessMask
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
        vk::PipelineStageFlagBits2::eBottomOfPipe,                 // dstStage
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

    VulkanCommandBuffers[frameIndex].pipelineBarrier2(dependency_info);
}