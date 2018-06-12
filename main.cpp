#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>

// -------------- Macros --------------
#define CHECK_RESULT(result) \
  if (VK_SUCCESS != (result)) { printf("Failure at %u %s\n", __LINE__, __FILE__); exit(-1); }

 // -------------- Globals --------------
VkInstance				g_Instance;
VkPhysicalDevice		g_PhysicalDevice;
VkDevice				g_Device;

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

int main() {
	CreateInstance();
	CreateDevice();



	DestroyDevice();
	DestroyInstance();
	return 0;
}


