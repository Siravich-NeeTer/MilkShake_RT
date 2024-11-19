// Reference: Copyright (C) 2016 by Sascha Willems - www.saschawillems.de

#pragma once

#include <iostream>
#include <cassert>

#include <vulkan/vulkan.h>

namespace MilkShake
{
	namespace Graphics
	{
		struct Buffer
		{
			VkDevice device;
			VkBuffer buffer = VK_NULL_HANDLE;
			VkDeviceMemory memory = VK_NULL_HANDLE;
			VkDescriptorBufferInfo descriptor;
			VkDeviceSize size = 0;
			VkDeviceSize alignment = 0;

			void* mapped = nullptr;

			VkBufferUsageFlags usageFlags;
			VkMemoryPropertyFlags memoryPropertyFlags;

			VkResult Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
			void UnMap();
			VkResult Bind(VkDeviceSize offset = 0);
			void SetupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
			void CopyTo(void* data, VkDeviceSize size);
			VkResult Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
			VkResult Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
			void Destroy();
		};
	}
}