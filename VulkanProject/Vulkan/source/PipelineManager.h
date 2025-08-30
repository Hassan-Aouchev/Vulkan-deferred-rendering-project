#pragma once
#include <vulkan/vulkan.h>
#include <fstream>
#include <vector>
#include <unordered_map>

struct TonemappingPushConstants {
	float averageLuminance;
	int exposureMode;
};
struct PipelineResource {
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	std::vector<std::string> descriptorSetLayoutNames;
};

struct DescriptorSetLayoutBindingConfig {
	VkDescriptorType descriptorType;
	uint32_t descriptorCount;
	VkShaderStageFlags stageFlags;
	uint32_t bindingFlags;
};
struct DescriptorSetLayoutConfig {
	std::string name;
	VkDescriptorSetLayoutCreateFlags layoutFlags;
	std::vector<std::string> bindingNames;
};

static std::vector<uint32_t> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("failed to open file: " + filename);
	}

	size_t fileSize = (size_t)file.tellg();
	// Ensure the size is a multiple of 4 (uint32_t size)
	if (fileSize % 4 != 0) {
		throw std::runtime_error("SPIR-V file size is not a multiple of 4: " + filename);
	}

	// Create a vector of uint32_t (which is what VkShaderModule expects)
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	file.seekg(0);
	file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

	return buffer;
}
class ResourceManager;
class SwapChain;
class Device;
class PipelineManager
{
public:
	PipelineManager(Device* device,ResourceManager* resourceManager, SwapChain* swapChain);
	~PipelineManager();

	VkDescriptorSetLayout GetUniversalDescriptorSetLayout() const { return m_UniversalDescriptorSetLayout; }
	VkDescriptorSetLayout GetGBufferDescriptorSetLayout() const { return m_GBufferDescriptorSetLayout; }
	VkDescriptorSetLayout GetDepthPrepassDescriptorSetLayout() const { return m_DepthPrepassDescriptorSetLayout; }

	void CreateDepthPrepassPipeline();
	VkPipeline GetDepthPrepassPipeline() const { return m_DepthPrepassPipeline; }
	VkPipelineLayout GetDepthPrepassPipelineLayout() const { return m_DepthPrepassPipelineLayout; }

	void CreateGBufferPipeline();
	VkPipeline GetGBufferPipeline() const { return m_GBufferPipeline; }
	VkPipelineLayout GetGBufferPipelineLayout() const { return m_GBufferPipelineLayout; }

	void CreateLightingPipeline();
	VkDescriptorSetLayout& GetLightingDescriptorSetLayout() { return m_LightingDescriptorSetLayout; }
	VkPipeline GetLightingPipeline() const { return m_LightingPipeline; }
	VkPipelineLayout GetLightingPipelineLayout() const { return m_LightingPipelineLayout; }

	void CreateToneMappingPipeline();
	VkDescriptorSetLayout& GetToneMappingDescriptorSetLayout() { return m_ToneMappingDescriptorSetLayout; }
	VkPipeline GetToneMappingPipeline() const { return m_ToneMappingPipeline; }
	VkPipelineLayout GetToneMappingPipelineLayout() const { return m_ToneMappingPipelineLayout; }

	VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code);

	const std::unordered_map<std::string, VkDescriptorSetLayout>& GetDescriptorSetLayouts() const{ return m_DescriptorSetLayouts; }
	const std::unordered_map<std::string, DescriptorSetLayoutBindingConfig>& GetDescriptorSetLayoutBindings() const { return m_DescriptorSetLayoutBindings; }

	void AddDescriptorSetLayout(const std::string& name, VkDescriptorSetLayout layout);
	void AddDescriptorSetLayoutBinding(const std::string& name, const DescriptorSetLayoutBindingConfig& config);

	const VkDescriptorSetLayout GetDescriptorSetLayout(const std::string& name) const {
		auto it = m_DescriptorSetLayouts.find(name);
		return (it != m_DescriptorSetLayouts.end()) ? it->second : VkDescriptorSetLayout{};
	}

	const DescriptorSetLayoutBindingConfig GetDescriptorSetLayoutBinding(const std::string& name) const {
		auto it = m_DescriptorSetLayoutBindings.find(name);
		return (it != m_DescriptorSetLayoutBindings.end()) ? it->second : DescriptorSetLayoutBindingConfig{};
	}
private:
	//(set 0 in both pipelines)
	void CreateUniversalDescriptorSetLayout();
	//(set 1 in main pipeline)  
	void CreateGBufferDescriptorSetLayout();
	//(set 1 in depth pipeline)
	void CreateDepthPrepassDescriptorSetLayout();

	void CreateToneMappingDescriptorSetLayout();

	void CreateLightingDescriptorSetLayout();
	void LoadPipelineCache();
	void SavePipelineCache();


	std::unordered_map<std::string, PipelineResource> m_Pipelines;
	std::unordered_map<std::string, VkDescriptorSetLayout> m_DescriptorSetLayouts;
	std::unordered_map<std::string, DescriptorSetLayoutBindingConfig> m_DescriptorSetLayoutBindings;

	VkPipelineCache m_PipelineCache;
	VkDescriptorSetLayout m_UniversalDescriptorSetLayout;
	VkDescriptorSetLayout m_GBufferDescriptorSetLayout;
	VkDescriptorSetLayout m_DepthPrepassDescriptorSetLayout;
	VkPipeline m_GBufferPipeline;
	VkPipelineLayout m_GBufferPipelineLayout;

	VkDescriptorSetLayout m_LightingDescriptorSetLayout;
	VkPipeline m_LightingPipeline;
	VkPipelineLayout m_LightingPipelineLayout;

	VkDescriptorSetLayout m_ToneMappingDescriptorSetLayout;
	VkPipeline m_ToneMappingPipeline;
	VkPipelineLayout m_ToneMappingPipelineLayout;

	VkPipeline m_DepthPrepassPipeline;
	VkPipelineLayout m_DepthPrepassPipelineLayout;

	std::vector<VkDescriptorSetLayoutBinding> m_Bindings{};
	std::vector<VkDescriptorBindingFlags> m_BindingFlags{};

	ResourceManager* m_ResourceManager;
	SwapChain* m_SwapChain;
	Device* m_Device;


};

class RenderPassBuilder
{
public:
	RenderPassBuilder() {};

	RenderPassBuilder& AddRenderPass(const std::string& name, const std::string& vertexFileName, const std::string fragmentFileName);
	RenderPassBuilder& AddDescriptorSetLayoutBinding(const std::string& name, DescriptorSetLayoutBindingConfig config);
	RenderPassBuilder& AddDescriptorSetLayout(DescriptorSetLayoutConfig config);
	RenderPassBuilder& AddBindlessDescriptorSetLayout(const std::string& setLayoutName, const std::string& bindingName, DescriptorSetLayoutBindingConfig bindingConfig);
	void BuildRenderPass(Device* device, ResourceManager* resourceManager, SwapChain* swapChain, PipelineManager* pipelineManager);

private:
	
	std::string m_Name;

	PipelineResource m_Pipeline;

	std::vector<DescriptorSetLayoutConfig> m_DescriptorSetLayoutsConfig;

	std::unordered_map<std::string, DescriptorSetLayoutBindingConfig> m_DescriptorSetLayoutBindings;

};