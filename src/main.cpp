#include "Utils.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>

/*
   This is a simple compute example written in Vulkan.
   All it does is copy contents from a Src buffer to a Dst.
*/

 // ------------------ Globals ------------------
VkInstance				g_Instance;
VkPhysicalDevice		g_PhysicalDevice;
VkDevice				g_Device;

/* Memory to allocate buffers from */
VkDeviceMemory			g_DeviceMemory;

/* Src Buffer parameters */
VkBuffer				g_SrcBuffer;
VkDeviceSize			g_SrcBufferOffset;

/* Dst Buffer parameters */
VkBuffer				g_DstBuffer;
VkDeviceSize			g_DstBufferOffset;

/* Shader stuff */
char*					g_ShaderBinaryData;
size_t					g_ShaderBinarySize; // Bytes
VkShaderModule          g_ComputeShaderModule;
VkDescriptorSetLayout   g_DescriptorSetLayout; // Our shader only has a single descriptor set

/* Pipeline stuff */
VkPipeline				g_ComputeShaderPipeline;
VkPipelineLayout		g_PipelineLayout;

// ------------------ Constants ------------------
const unsigned g_BufferLength = 1024;
const unsigned g_BufferSize = sizeof(unsigned) * g_BufferLength;
const char* g_ShaderBinaryFile = "resources\\SimpleCopy.spv";
const char* g_ShaderEntryPoint = "CopyBuffer";
const char* g_ShaderDescriptorMap = "resources\\SimpleCopy-map.csv";

// 1. This is based on DeviceProperties.txt
//	  This memory's heap is ~ 4 GiB and has both VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT bits set
//	  So we can use vkMapMemory to write data to Src buffer and also read data from Dst buffer
// 2. This is slower though, and the memory to use is type=0 one which has VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT bit set instead and is 
//	  backed by up ~8 GiB of GPU video-memory-heap. But since we cannot use vkMapBuffer as it's not host visible, we'd need a staging buffer 
//    to copy data between host and device though, that's where the memory type=2 comes in.
// 3. The staging buffer memory's heap is sized around 256 MiB and has all 3 flags set: 
//	  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
// Some resources for future:
// https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer
// https://developer.nvidia.com/vulkan-memory-management
// https://software.intel.com/en-us/articles/api-without-secrets-introduction-to-vulkan-part-5
const unsigned memoryTypeIdx = 1;
const bool enableStandardValidationLayer = true;

// ----------------------------
// Create vkInstance
// ----------------------------
void CreateInstance() {

	std::vector<const char*> validationLayers;

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Vulkan Compute";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	// Setup a vkInstanceCreateInfo struct
	VkInstanceCreateInfo instanceInfo = {};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;

	if (enableStandardValidationLayer) {
		validationLayers.push_back("VK_LAYER_LUNARG_core_validation");
		instanceInfo.enabledLayerCount = validationLayers.size();
		instanceInfo.ppEnabledLayerNames = validationLayers.data();
	}

	// No allocater needed for now...
	VkResult result = vkCreateInstance(&instanceInfo, NULL, &g_Instance);
	VK_CHECK_RESULT(result);

	MESSAGE("VkInstance created.");
}

// ----------------------------
// Destroy vkInstance
// ----------------------------
void DestroyInstance() {
	vkDestroyInstance(g_Instance, NULL);
	MESSAGE("VkInstance destroyed.");
}

// ----------------------------
// Create VkDevice
// ----------------------------
void CreateDevice() {

	// Enumerate all the physical devices present in the system
	unsigned nPhysicalDevices = 0;
	VkResult result = vkEnumeratePhysicalDevices(g_Instance, &nPhysicalDevices, NULL);
	VK_CHECK_RESULT(result);

	// Get all physical devices
	VkPhysicalDevice* pPhysicalDevices = new VkPhysicalDevice[nPhysicalDevices];
	result = vkEnumeratePhysicalDevices(g_Instance, &nPhysicalDevices, pPhysicalDevices);
	VK_CHECK_RESULT(result);

	// Get each physical device's properties
	for (unsigned i = 0; i < nPhysicalDevices; i++) {
		VkPhysicalDeviceProperties phyDeviceProp;
		vkGetPhysicalDeviceProperties(pPhysicalDevices[i], &phyDeviceProp);

		// Print some important properties of each physical device
		printf("Physical Device: %d, Type: %d, Name: %s\n", i, phyDeviceProp.deviceType, phyDeviceProp.deviceName);

		// Select the physical device with following properties:
		// deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
		// deviceName has "RX 480" in its name
		if (phyDeviceProp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && strstr(phyDeviceProp.deviceName, "RX 480")) {
			g_PhysicalDevice = pPhysicalDevices[i];
		}
	}

	// Query the queues available on the selected physical device
	// Just to make sure that it supports a "compute" only queue
	unsigned nQueueFamilyProperties;
	vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &nQueueFamilyProperties, NULL);

	VkQueueFamilyProperties* pQueueFamilyProperties = new VkQueueFamilyProperties[nQueueFamilyProperties];
	vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &nQueueFamilyProperties, pQueueFamilyProperties);

	unsigned idxQueue = 0;	// Index of queue to be used while creating a VkDeviceQueueCreateInfo
	// Get each queue family properties
	for (unsigned i = 0; i < nQueueFamilyProperties; i++) {
		// Select a compute-only queue family
		// 14 == COMPUTE | TRANSFER | SPARSE
		if (pQueueFamilyProperties[i].queueFlags == 14) {
			idxQueue = i;
		}
		MESSAGE("QueueFamily: %d, QueueFlags: %d, QueueCount: %d.", i, pQueueFamilyProperties[i].queueFlags, pQueueFamilyProperties[i].queueCount);
	}

	// Set- the priority of the single compute queue
	float queuePriority = 1.0;

	// Setup VkDeviceQueueCreateInfo struct before setting up a vkDeviceCreationInfo struct
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = idxQueue;
	queueCreateInfo.queueCount = 1;	// Only a single compute queue for now
	queueCreateInfo.pQueuePriorities = &queuePriority;

	// Setup vkDeviceCreationInfo struct before setting up a device
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1; 
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

	// Select the first physical device to create a device
	result = vkCreateDevice(g_PhysicalDevice, &deviceCreateInfo, NULL, &g_Device);
	VK_CHECK_RESULT(result);

	MESSAGE("VkDevice created.");
}

// ----------------------------
// Destroy VkDevice
// ----------------------------
void DestroyDevice() {
	vkDestroyDevice(g_Device, NULL);
	MESSAGE("VkDevice destroyed.");
}

// --------------------------------------------------------
// Create Src and Dst Buffers
// --------------------------------------------------------
void CreateBuffers() {

	VkResult result = {};

	// First create a vkBufferCreateInfo struct which is used in vkCreateBuffer 
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = g_BufferSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	// Create Src Buffer
	result = vkCreateBuffer(g_Device, &bufferCreateInfo, NULL, &g_SrcBuffer);
	VK_CHECK_RESULT(result);

	// Create Dst Buffer
	result = vkCreateBuffer(g_Device, &bufferCreateInfo, NULL, &g_DstBuffer);
	VK_CHECK_RESULT(result);

	MESSAGE("Buffers created.");
}

// --------------------------------------------------------
// Allocate Src and Dst Buffers
// --------------------------------------------------------
void AllocateBuffers() {
	// Get the memory requirements of the buffers 
	// This is to find a suitable memory-heap for the buffer for its allocation
	VkMemoryRequirements srcMemoryRequirements;
	vkGetBufferMemoryRequirements(g_Device, g_SrcBuffer, &srcMemoryRequirements);

	VkMemoryRequirements dstMemoryRequirements;
	vkGetBufferMemoryRequirements(g_Device, g_DstBuffer, &dstMemoryRequirements);

	// Allocate memory
	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = 2 * g_BufferSize;
	memAllocInfo.memoryTypeIndex = memoryTypeIdx;

	VkResult result = vkAllocateMemory(g_Device, &memAllocInfo, NULL, &g_DeviceMemory);
	VK_CHECK_RESULT(result);

	// Store the buffers next to each other
	g_SrcBufferOffset = 0;
	g_DstBufferOffset = g_BufferSize;

	// Bind memory to buffers
	result = vkBindBufferMemory(g_Device, g_SrcBuffer, g_DeviceMemory, g_SrcBufferOffset);
	VK_CHECK_RESULT(result);

	result = vkBindBufferMemory(g_Device, g_DstBuffer, g_DeviceMemory, g_DstBufferOffset);
	VK_CHECK_RESULT(result);

	MESSAGE("Buffers allocated.");
}

// --------------------------------------------------------
// DeAllocate Src and Dst Buffers
// --------------------------------------------------------
void DeAllocateBuffers() {
	vkFreeMemory(g_Device, g_DeviceMemory, NULL);
}

// --------------------------------------------------------
// Destroy Src and Dst Buffers
// --------------------------------------------------------
void DestroyBuffers() {
	vkDestroyBuffer(g_Device, g_SrcBuffer, NULL);
	vkDestroyBuffer(g_Device, g_DstBuffer, NULL);
}

// --------------------------------------------------------
// Initialize Src Buffer
// --------------------------------------------------------
void InitializeSrcBuffer() {

	unsigned* pData = nullptr;
	// Map the Src Buffer to CPU Memory
	VkResult result = vkMapMemory(g_Device, g_DeviceMemory, g_SrcBufferOffset, g_BufferSize, 0, (void**)&pData);
	VK_CHECK_RESULT(result);

	if (pData == nullptr) {
		MESSAGE("Memory unmapping failed.");
		exit(-1);
	}

	// Fill the buffer with some data
	for (unsigned i = 0; i < g_BufferLength; i++) {
		pData[i] = i;
	}

	vkUnmapMemory(g_Device, g_DeviceMemory);
	VK_CHECK_RESULT(result);

	MESSAGE("Src Buffer initialized.");
}

// --------------------------------------------------------
// Create a Compute Shader
// --------------------------------------------------------
void CreateComputeShaderModule() {

	// Load Compute-Shader SPIR-V binary
	std::ifstream fileStream;
	fileStream.open(g_ShaderBinaryFile, std::ios::binary);
	if (!fileStream.is_open()) {
		MESSAGE("Unable to load Shader binary: %s", g_ShaderBinaryFile);
		exit(-1);
	}

	// Get binary size first
	std::streampos begin = fileStream.tellg();
	fileStream.seekg(0, std::ios::end);
	std::streampos end = fileStream.tellg();
	g_ShaderBinarySize = size_t(end - begin); // In Bytes

	// Load file into memory
	g_ShaderBinaryData = new char[g_ShaderBinarySize];

	// Seek back to the start of the file
	fileStream.seekg(0, std::ios::beg);

	// Read binary file contents
	fileStream.read(g_ShaderBinaryData, g_ShaderBinarySize);
	fileStream.close();

	// Initialize a VkShaderModuleCreateInfo struct
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = g_ShaderBinarySize;
	createInfo.pCode = (const uint32_t*)g_ShaderBinaryData;

	// Create shader module
	VkResult result = vkCreateShaderModule(g_Device, &createInfo, NULL, &g_ComputeShaderModule);
	VK_CHECK_RESULT(result);

	MESSAGE("vkShaderModule created.")
}

// Contents of a single line of a descriptor map generated by clspv
struct DescriptorMapLine {
	std::string kernelName;
	std::string argName;
	unsigned argIdx;
	unsigned set;
	unsigned binding;
	unsigned offset;
	std::string argKind;
};

void ReadDescriptorMap(std::vector<DescriptorMapLine>& descriptorMaps) {

	// Read the descriptor-map file of our kernel: CopyBuffer
	std::ifstream fileStream(g_ShaderDescriptorMap);
	if (!fileStream.is_open()) {
		MESSAGE("Unable to load shader descriptor map: %s", g_ShaderDescriptorMap);
		exit(-1);
	}

	// Read file line by line
	std::string line;
	for (unsigned i = 0; std::getline(fileStream, line); i++)
	{
		std::istringstream iss(line);
		std::string token;

		DescriptorMapLine descriptorMapLine;

		// Get all tokens separated by ,
		for (unsigned j = 0; std::getline(iss, token, ','); j++) {
			switch (j) {
			case 1:
				descriptorMapLine.kernelName = token;
				break;
			case 3:
				descriptorMapLine.argName = token;
				break;
			case 5:
				descriptorMapLine.argIdx = std::stoi(token);
				break;
			case 7:
				descriptorMapLine.set = std::stoi(token);
				break;
			case 9:
				descriptorMapLine.binding = std::stoi(token);
				break;
			case 11:
				descriptorMapLine.offset = std::stoi(token);
				break;
			case 13:
				descriptorMapLine.argKind = token;
				break;
			default:
				break;
			}
		}
		descriptorMaps.push_back(descriptorMapLine);
	}
}

// --------------------------------------------------------
// Create a Descriptor Set Layout
// --------------------------------------------------------
void CreateDescriptorSetLayout() {
	
	std::vector<DescriptorMapLine> descriptorMaps;
	ReadDescriptorMap(descriptorMaps);

	unsigned nBindings = descriptorMaps.size();

	// Create an array of descriptor-set bindings
	VkDescriptorSetLayoutBinding* bindings = new VkDescriptorSetLayoutBinding[nBindings];
	
	// Create a binding for each DescriptorMap as they're laid out in the same way
	for (unsigned b = 0; b < nBindings; b++) {

		VkDescriptorSetLayoutBinding binding = {};

		binding.binding = descriptorMaps[b].binding;
		if (descriptorMaps[b].argKind == "buffer" || descriptorMaps[b].argKind == "pod") {
			binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		}
		binding.descriptorCount = 1;
		binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

		bindings[b] = binding;
	}

	// Create a descriptor set
	VkDescriptorSetLayoutCreateInfo setInfo = {};		// Per Set (Single set for now)
	setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setInfo.bindingCount = nBindings;
	setInfo.pBindings = &bindings[0];

	VkResult result = vkCreateDescriptorSetLayout(g_Device, &setInfo, NULL, &g_DescriptorSetLayout);
	VK_CHECK_RESULT(result);

	MESSAGE("vkDescriptorSetLayout created.")
}

// --------------------------------------------------------
// Create a Compute Pipeline
// --------------------------------------------------------
void CreateComputePipeline() {

	// Create a VkPipelineShaderStageCreateInfo struct needed by VkComputePipelineCreateInfo
	VkPipelineShaderStageCreateInfo csCreateInfo = {};
	csCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	csCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	csCreateInfo.module = g_ComputeShaderModule;
	csCreateInfo.pName = g_ShaderEntryPoint;

	// Create a pipeline layout info struct for a pipeline layout
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &g_DescriptorSetLayout;

	// Create a pipeline layout
	VkResult result = vkCreatePipelineLayout(g_Device, &pipelineLayoutInfo, NULL, &g_PipelineLayout);
	VK_CHECK_RESULT(result);

	// First create a VkComputePipelineCreateInfo needed by vkCreateComputePipelines()
	VkComputePipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stage = csCreateInfo;
	pipelineCreateInfo.layout = g_PipelineLayout;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	// Create a compute pipeline
	result = vkCreateComputePipelines(g_Device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &g_ComputeShaderPipeline);
	VK_CHECK_RESULT(result);
	
	MESSAGE("VkPipeline created.");
}

int main() {

	/* Device */
	CreateInstance();
	CreateDevice();

	/* Memory */
	CreateBuffers();
	AllocateBuffers();
	InitializeSrcBuffer();

	/* Compute Pipeline and Shader creation */
	CreateComputeShaderModule();
	CreateDescriptorSetLayout();
	CreateComputePipeline();

	/* Free stuff */
	DestroyBuffers();
	DeAllocateBuffers();
	DestroyDevice();
	DestroyInstance();

	return 0;
}


