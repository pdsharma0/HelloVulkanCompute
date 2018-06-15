#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <iostream>

/*
   This is a simple compute example written in Vulkan.
   All it does is copy contents from a Src buffer to a Dst.
*/

// -------------- Macros --------------
#define CHECK_RESULT(result) \
  if (VK_SUCCESS != (result)) { printf("Failure at %u %s\n", __LINE__, __FILE__); exit(-1); }

 // -------------- Globals --------------
VkInstance				g_Instance;
VkPhysicalDevice		g_PhysicalDevice;
VkDevice				g_Device;
VkBuffer				g_deviceSrcBuffer;
VkBuffer				g_deviceDstBuffer;
VkDeviceMemory			g_DeviceMemory;

// -------------- Constants --------------
const unsigned bufferLength = 1024;

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

// ----------------------------
// Create vkInstance
// ----------------------------
void CreateInstance() {

	// Setup a vkInstanceCreateInfo struct
	VkInstanceCreateInfo instanceInfo;
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pNext = NULL;
	instanceInfo.flags = 0;
	instanceInfo.pApplicationInfo = NULL;
	instanceInfo.enabledLayerCount = 0;
	instanceInfo.ppEnabledLayerNames = NULL;
	instanceInfo.enabledExtensionCount = 0;
	instanceInfo.ppEnabledExtensionNames = NULL;

	// No allocater needed for now...
	VkResult result = vkCreateInstance(&instanceInfo, NULL, &g_Instance);
	CHECK_RESULT(result);

	printf("VkInstance sucessfully created!\n");
}

// ----------------------------
// Destroy vkInstance
// ----------------------------
void DestroyInstance() {
	vkDestroyInstance(g_Instance, NULL);
	printf("VkInstance sucessfully destroyed!\n");
}

// ----------------------------
// Create VkDevice
// ----------------------------
void CreateDevice() {

	// Enumerate all the physical devices present in the system
	unsigned nPhysicalDevices = 0;
	VkResult result = vkEnumeratePhysicalDevices(g_Instance, &nPhysicalDevices, NULL);
	CHECK_RESULT(result);

	// Get all physical devices
	VkPhysicalDevice* pPhysicalDevices = new VkPhysicalDevice[nPhysicalDevices];
	result = vkEnumeratePhysicalDevices(g_Instance, &nPhysicalDevices, pPhysicalDevices);
	CHECK_RESULT(result);

	// Get each physical device's properties
	for (unsigned i = 0; i < nPhysicalDevices; i++) {
		VkPhysicalDeviceProperties phyDeviceProp;
		vkGetPhysicalDeviceProperties(pPhysicalDevices[i], &phyDeviceProp);

		// Print some important properties of each physical device
		printf("Physical Device: %d, Type: %d, Name: %s\n", i, phyDeviceProp.deviceType, phyDeviceProp.deviceName);

		// Select the physical device with following properties:
		// deviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
		// deviceName has "RX 480" in its name
		if (phyDeviceProp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
			strstr(phyDeviceProp.deviceName, "RX 480")) {
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
		printf("QueueFamily: %d, QueueFlags: %d, QueueCount: %d\n", i, pQueueFamilyProperties[i].queueFlags, pQueueFamilyProperties[i].queueCount);
	}

	// Set- the priority of the single compute queue
	float queuePriority = 1.0;

	// Setup VkDeviceQueueCreateInfo struct before setting up a vkDeviceCreationInfo struct
	VkDeviceQueueCreateInfo queueCreateInfo;
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.pNext = NULL;
	queueCreateInfo.flags = 0;
	queueCreateInfo.queueFamilyIndex = idxQueue;
	queueCreateInfo.queueCount = 1;	// Only a single compute queue for now
	queueCreateInfo.pQueuePriorities = &queuePriority;

	// Setup vkDeviceCreationInfo struct before setting up a device
	VkDeviceCreateInfo deviceCreateInfo;
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = NULL;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = 1; 
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = NULL;
	deviceCreateInfo.enabledExtensionCount = 0;
	deviceCreateInfo.ppEnabledExtensionNames = NULL;
	deviceCreateInfo.pEnabledFeatures = NULL;

	// Select the first physical device to create a device
	result = vkCreateDevice(g_PhysicalDevice, &deviceCreateInfo, NULL, &g_Device);
	CHECK_RESULT(result);

	printf("VkDevice sucessfully created!\n");
}

// ----------------------------
// Destroy VkDevice
// ----------------------------
void DestroyDevice() {
	vkDestroyDevice(g_Device, NULL);
	printf("VkDevice sucessfully destroyed!\n");
}

// --------------------------------------------------------
// Create Src and Dst Buffers
// --------------------------------------------------------
void CreateBuffers() {

	VkResult result = {};

	// First create a vkBufferCreateInfo struct which is used in vkCreateBuffer 
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = sizeof(unsigned) * bufferLength;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	// Create Src Buffer
	result = vkCreateBuffer(g_Device, &bufferCreateInfo, NULL, &g_deviceSrcBuffer);
	CHECK_RESULT(result);

	// Create Dst Buffer
	result = vkCreateBuffer(g_Device, &bufferCreateInfo, NULL, &g_deviceDstBuffer);
	CHECK_RESULT(result);

	printf("Buffers sucessfully created!\n");
}

// --------------------------------------------------------
// Allocate Src and Dst Buffers
// --------------------------------------------------------
void AllocateBuffers() {
	// Get the memory requirements of the buffers 
	// This is to find a suitable memory-heap for the buffer for its allocation
	VkMemoryRequirements srcMemoryRequirements;
	vkGetBufferMemoryRequirements(g_Device, g_deviceSrcBuffer, &srcMemoryRequirements);

	VkMemoryRequirements dstMemoryRequirements;
	vkGetBufferMemoryRequirements(g_Device, g_deviceDstBuffer, &dstMemoryRequirements);

	// Allocate memory
	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = srcMemoryRequirements.size + dstMemoryRequirements.size;
	memAllocInfo.memoryTypeIndex = memoryTypeIdx;

	VkResult result = vkAllocateMemory(g_Device, &memAllocInfo, NULL, &g_DeviceMemory);
	CHECK_RESULT(result);

	// Bind memory to buffers
	result = vkBindBufferMemory(g_Device, g_deviceSrcBuffer, g_DeviceMemory, 0);
	CHECK_RESULT(result);

	result = vkBindBufferMemory(g_Device, g_deviceSrcBuffer, g_DeviceMemory, srcMemoryRequirements.size);
	CHECK_RESULT(result);

	printf("Buffers sucessfully allocated!\n");
}

// --------------------------------------------------------
// Allocate Src Buffer
// --------------------------------------------------------
void InitializeSrcBuffer() {

}

int main() {
	CreateInstance();
	CreateDevice();

	CreateBuffers();
	AllocateBuffers();

	DestroyDevice();
	DestroyInstance();
	return 0;
}


