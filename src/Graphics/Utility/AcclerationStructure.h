#pragma once

#include <vulkan/vulkan.hpp>

#include "Graphics/Buffer.h"

namespace MilkShake
{
	namespace Graphics
	{
		// Holds data for a ray tracing scratch buffer that is used as a temporary storage
		struct ScratchBuffer
		{
			uint64_t deviceAddress = 0;
			VkBuffer handle = VK_NULL_HANDLE;
			VkDeviceMemory memory = VK_NULL_HANDLE;
		};

		// Ray tracing acceleration structure
		struct AccelerationStructure 
		{
			VkAccelerationStructureKHR handle;
			uint64_t deviceAddress = 0;
			VkDeviceMemory memory;
			VkBuffer buffer;
		};

		// Holds information for a storage image that the ray tracing shaders output to
		struct StorageImage 
		{
			VkDeviceMemory memory = VK_NULL_HANDLE;
			VkImage image = VK_NULL_HANDLE;
			VkImageView view = VK_NULL_HANDLE;
			VkFormat format;
		};

		class ShaderBindingTable : public Buffer
		{
			public:
				VkStridedDeviceAddressRegionKHR stridedDeviceAddressRegion{};
		};
	}
}