// Copyright (c) CreationArtStudios, Khairol Anwar

#include "PipelineManager.h"
#include "VulkanInstance/VulkanInstance.h"
#include "SwapChain.h"
#include "Mesh.h"
#include "../Shader/Shader.h"
#include "../FileSystem/FileSystem.h"

PipelineManager::PipelineManager(VulkanInstance* vulkanInstance, SwapChain* swapChain)
	: VulkanInstanceWrapper(vulkanInstance), SwapChainWrapper(swapChain)
{
}

PipelineManager::~PipelineManager()
{
}

void PipelineManager::CreateDescriptorSetLayout()
{
	std::array bindings = {
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
		vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
	};

	vk::DescriptorSetLayoutCreateInfo layoutInfo({}, static_cast<uint32_t>(bindings.size()), bindings.data());

	VulkanDescriptorSetLayout = vk::raii::DescriptorSetLayout(VulkanInstanceWrapper->GetLogicalDevice(), layoutInfo);
}

void PipelineManager::CreateGraphicsPipeline()
{
	Shader* shaderSystem = new Shader();
	FileSystem* fileSystem = new FileSystem();
	vk::raii::ShaderModule VertexShaderModule = shaderSystem->CreateShaderModule(fileSystem->ReadFile("../Engine/Binaries/Shaders/ShaderType_Vertex.spv"), VulkanInstanceWrapper->GetLogicalDevice());
	vk::raii::ShaderModule FragmentShaderModule = shaderSystem->CreateShaderModule(fileSystem->ReadFile("../Engine/Binaries/Shaders/ShaderTypes_Fragment.spv"), VulkanInstanceWrapper->GetLogicalDevice());

	vk::PipelineShaderStageCreateInfo VertShaderStageInfo;
	VertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	VertShaderStageInfo.module = VertexShaderModule;
	VertShaderStageInfo.pName = "main";

	vk::PipelineShaderStageCreateInfo FragShaderStageInfo{};
	FragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	FragShaderStageInfo.module = FragmentShaderModule;
	FragShaderStageInfo.pName = "main";

	vk::PipelineShaderStageCreateInfo ShaderStages[] = { VertShaderStageInfo, FragShaderStageInfo };

	auto bindingDescription = Mesh::Vertex::GetBindingDescription();
	auto attributeDescriptions = Mesh::Vertex::GetAttributeDescriptions();

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	inputAssembly.primitiveRestartEnable = vk::False;

	vk::PipelineViewportStateCreateInfo viewportState;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	vk::PipelineRasterizationStateCreateInfo rasterizer;
	rasterizer.depthClampEnable = vk::False;
	rasterizer.rasterizerDiscardEnable = vk::False;
	rasterizer.polygonMode = vk::PolygonMode::eFill;
	rasterizer.cullMode = vk::CullModeFlagBits::eBack;
	rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
	rasterizer.depthBiasEnable = vk::False;
	rasterizer.lineWidth = 1.0f;

	vk::PipelineMultisampleStateCreateInfo multisampling;
	multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
	multisampling.sampleShadingEnable = vk::False;

	vk::PipelineDepthStencilStateCreateInfo depthStencil;
	depthStencil.depthTestEnable = vk::True;
	depthStencil.depthWriteEnable = vk::True;
	depthStencil.depthCompareOp = vk::CompareOp::eLess;
	depthStencil.depthBoundsTestEnable = vk::False;
	depthStencil.stencilTestEnable = vk::False;

	vk::PipelineColorBlendAttachmentState colorBlendAttachment;
	colorBlendAttachment.blendEnable = vk::False;
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

	vk::PipelineColorBlendStateCreateInfo colorBlending;
	colorBlending.logicOpEnable = vk::False;
	colorBlending.logicOp = vk::LogicOp::eCopy;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	std::vector dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo dynamicState;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &*VulkanDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;

	VulkanPipelineLayout = vk::raii::PipelineLayout(VulkanInstanceWrapper->GetLogicalDevice(), pipelineLayoutInfo);

	depthFormat = findDepthFormat();

	vk::Format swapChainFormat = SwapChainWrapper->GetFormat().format;

	vk::PipelineRenderingCreateInfo PipelineRenderingCreateInfo;
	PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChainFormat;
	PipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;

	vk::GraphicsPipelineCreateInfo GraphicPipelineCreateInfo;
	GraphicPipelineCreateInfo.stageCount = 2;
	GraphicPipelineCreateInfo.pStages = ShaderStages;
	GraphicPipelineCreateInfo.pVertexInputState = &vertexInputInfo;
	GraphicPipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	GraphicPipelineCreateInfo.pViewportState = &viewportState;
	GraphicPipelineCreateInfo.pRasterizationState = &rasterizer;
	GraphicPipelineCreateInfo.pMultisampleState = &multisampling;
	GraphicPipelineCreateInfo.pColorBlendState = &colorBlending;
	GraphicPipelineCreateInfo.pDynamicState = &dynamicState;
	GraphicPipelineCreateInfo.layout = VulkanPipelineLayout;
	GraphicPipelineCreateInfo.renderPass = nullptr;
	GraphicPipelineCreateInfo.pNext = &PipelineRenderingCreateInfo;
	GraphicPipelineCreateInfo.pDepthStencilState = &depthStencil;

	VulkanGraphicsPipeline = vk::raii::Pipeline(VulkanInstanceWrapper->GetLogicalDevice(), nullptr, GraphicPipelineCreateInfo);
}

vk::Format PipelineManager::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
	for (const auto format : candidates) {
		vk::FormatProperties props = VulkanInstanceWrapper->GetPhysicalDevice().getFormatProperties(format);

		if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}

vk::Format PipelineManager::findDepthFormat()
{
	return findSupportedFormat(
		{ vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
		vk::ImageTiling::eOptimal,
		vk::FormatFeatureFlagBits::eDepthStencilAttachment
	);
}

bool PipelineManager::hasStencilComponent(vk::Format format)
{
	return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}