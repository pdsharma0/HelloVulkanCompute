// 1. Parse SPIR-V 
// 2. Get bindings to create VkDescriptorSetLayoutBinding objects for all bindings in the kernel => Reflection
// 3. Create VkDescriptorSetLayoutCreateInfo grouping bindings into sets => Reflection

#include "spirv_cross.hpp"
#include <vector>
#include <assert.h>
#include <iostream>
#include "DescriptorSets.h"

// ----------- Globals -----------
spirv_cross::Compiler* g_SPIRVCompiler = nullptr;

// Parses SPIR-V Binary to Compiler
void ParseSPIRVBinary(char* spirvBinaryData, size_t spirvBinarySize) {

	assert(spirvBinaryData != 0);
	assert(spirvBinarySize % 4 == 0);

	uint32_t* data = (uint32_t*)spirvBinaryData;
	unsigned nWords = spirvBinarySize / 4;

	g_SPIRVCompiler = new spirv_cross::Compiler(data, nWords);
}

// Get Kernel Resources
void GetKernelResources() {

	assert(g_SPIRVCompiler != 0);
	spirv_cross::ShaderResources res = g_SPIRVCompiler->get_shader_resources();
	
	// Get all storage buffers
	// These are __global objects in OpenCL kernel
	for (auto sb : res.storage_buffers) {
		unsigned set = g_SPIRVCompiler->get_decoration(sb.id, spv::DecorationDescriptorSet);
		unsigned binding = g_SPIRVCompiler->get_decoration(sb.id, spv::DecorationBinding);
		std::cout << " Set:" << set << " Binding:" << binding << " StorageBuffer:" << sb.name << '\n';
	}
}

 // const uint32_t *, size_t interface is also available.