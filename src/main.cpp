#include "Utils.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>
#include <vector>

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
VkShaderModule          g_ComputeShader;

// ------------------ Constants ------------------
const unsigned bufferLength = 1024;
const unsigned bufferSize = sizeof(unsigned) * bufferLength;

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

	// Setup a vkInstanceCreateInfo struct
	VkInstanceCreateInfo instanceInfo = {};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	if (enableStandardValidationLayer) {
		validationLayers.push_back("VK_LAYER_LUNARG_standard_validation");
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
	bufferCreateInfo.size = bufferSize;
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
	memAllocInfo.allocationSize = 2 * bufferSize;
	memAllocInfo.memoryTypeIndex = memoryTypeIdx;

	VkResult result = vkAllocateMemory(g_Device, &memAllocInfo, NULL, &g_DeviceMemory);
	VK_CHECK_RESULT(result);

	// Store the buffers next to each other
	g_SrcBufferOffset = 0;
	g_DstBufferOffset = bufferSize;

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
	VkResult result = vkMapMemory(g_Device, g_DeviceMemory, g_SrcBufferOffset, bufferSize, 0, (void**)&pData);
	VK_CHECK_RESULT(result);

	if (pData == nullptr) {
		MESSAGE("Memory unmapping failed.");
		exit(-1);
	}

	// Fill the buffer with some data
	for (unsigned i = 0; i < bufferLength; i++) {
		pData[i] = i;
	}

	vkUnmapMemory(g_Device, g_DeviceMemory);
	VK_CHECK_RESULT(result);

	MESSAGE("Src Buffer initialized.");
}

// --------------------------------------------------------
// Create a Compute Shader
// --------------------------------------------------------
void CreateComputeShader() {

}

// --------------------------------------------------------
// Create a Compute Pipeline
// --------------------------------------------------------
void CreateComputePipeline() {

	// Create a VkShaderModule needed by VkPipelineShaderStageCreateInfo


	// Create a VkPipelineShaderStageCreateInfo struct needed by VkComputePipelineCreateInfo
	VkPipelineShaderStageCreateInfo csCreateInfo = {};
	csCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	csCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;


	// First create a VkComputePipelineCreateInfo needed by vkCreateComputePipelines()
	VkComputePipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	//pipelineCreateInfo.
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



	/* Free stuff */
	DestroyBuffers();
	DeAllocateBuffers();
	DestroyDevice();
	DestroyInstance();
	return 0;
}


