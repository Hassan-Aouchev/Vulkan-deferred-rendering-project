#pragma once
#include <vulkan/vulkan.h>
#include <fstream>
#include <vector>
#include <unordered_map>

struct TonemappingPushConstants {
	float averageLuminance;
	int exposureMode;
};

struct PushConstantConfig {
	VkShaderStageFlags stageFlags = 0;
	uint32_t offset = 0;
	uint32_t size = 0;
};

struct PipelineResource {
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct PipelineLayoutConfig {
	std::string name;

	std::string vertShaderName;
	std::string fragShaderName;
	enum class CullMode {
		None = 0,
		Front = 1,
		Back = 2,
		FrontAndBack = 3,
	} cullMode;
	
	bool depthTestEnable = false;

	bool depthWriteEnable = false;
	bool clockwise = false;

	enum class DepthCompareOp {
		Never = 0,
		Less = 1,
		Equal = 2,
		LessOrEqual = 3,
		Greater = 4,
		NotEqual = 5,
		GreaterOrEqual = 6,
		Always = 7
	} depthCompareOp;

	enum class Formats {
		R8G8B8A8_UNORM,
		R32G32B32A32_SFLOAT,
		R8G8B8A8_SRGB
	};

	struct Output
	{
		std::string name;
		Formats format;
	};

	std::vector <Output> Outputs;

	std::vector<std::string> PushConstants;

	std::vector<std::string> descriptorSetLayoutNames;
};

struct DescriptorSetLayoutBindingConfig {
	VkDescriptorType descriptorType;
	uint32_t descriptorCount;
	VkShaderStageFlags stageFlags;
	uint32_t bindingFlags = 0;
};

struct DescriptorSetLayoutConfig {
	std::string name;
	VkDescriptorSetLayoutCreateFlags layoutFlags = 0;
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
	void AddPipelineResource(const std::string& name, VkPipeline pipeline, VkPipelineLayout pipelineLayout);

	const VkDescriptorSetLayout GetDescriptorSetLayout(const std::string& name) const {
		auto it = m_DescriptorSetLayouts.find(name);
		return (it != m_DescriptorSetLayouts.end()) ? it->second : VkDescriptorSetLayout{};
	}

	const DescriptorSetLayoutBindingConfig GetDescriptorSetLayoutBinding(const std::string& name) const {
		auto it = m_DescriptorSetLayoutBindings.find(name);
		return (it != m_DescriptorSetLayoutBindings.end()) ? it->second : DescriptorSetLayoutBindingConfig{};
	}

	const PipelineResource GetPipeline(const std::string& name) const {
		auto it = m_Pipelines.find(name);
		return (it != m_Pipelines.end()) ? it->second : PipelineResource{};
	}

	const PushConstantConfig& GetPushConstantConfig()const { return m_PushConstantConfig; }

	VkPipelineCache m_PipelineCache;
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

	std::unordered_map<std::string, PipelineLayoutConfig> m_PipelineConfigs;

	PushConstantConfig m_PushConstantConfig;

	std::unordered_map<std::string, VkDescriptorSetLayout> m_DescriptorSetLayouts;
	std::unordered_map<std::string, DescriptorSetLayoutBindingConfig> m_DescriptorSetLayoutBindings;

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

class PipelineBuilder
{
public:
	PipelineBuilder() {};

	PipelineBuilder& AddPipeline(PipelineLayoutConfig& config);
	PipelineBuilder& AddDescriptorSetLayoutBinding(const std::string& name, DescriptorSetLayoutBindingConfig config);
	PipelineBuilder& AddDescriptorSetLayout(DescriptorSetLayoutConfig config);
	void BuildPipeline(Device* device, ResourceManager* resourceManager, SwapChain* swapChain, PipelineManager* pipelineManager);

private:
	std::vector<DescriptorSetLayoutConfig> m_DescriptorSetLayoutsConfig;

	std::vector<PipelineLayoutConfig> m_PipelineLayoutConfig;

	std::unordered_map<std::string, DescriptorSetLayoutBindingConfig> m_DescriptorSetLayoutBindings;

	VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code, Device* device);
};