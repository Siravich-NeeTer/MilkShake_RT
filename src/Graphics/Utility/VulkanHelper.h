#pragma once

#include <vulkan/vulkan.hpp>

#include <iostream>

namespace MilkShake
{
	namespace Graphics
	{
		class VulkanRenderer;
		class Buffer;

		struct ImageWrap;

		namespace Utility
		{
			uint32_t AlignedSize(uint32_t value, uint32_t alignment);
			size_t AlignedSize(size_t value, size_t alignment);
			VkDeviceSize AlignedVkSize(VkDeviceSize value, VkDeviceSize alignment);

			VkCommandBuffer BeginSingleTimeCommands(const VulkanRenderer& _vkRenderer, const VkCommandPool& _commandPool);
			void EndSingleTimeCommands(const VulkanRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkCommandBuffer commandBuffer);
			VkImageView CreateImageView(const VulkanRenderer& _vkRenderer, VkImage image, VkFormat format, VkImageAspectFlags _aspectFlags);

			void CreateBuffer(const VulkanRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkDeviceSize _size, VkBufferUsageFlags _usage, VkMemoryPropertyFlags _properties, VkBuffer& _buffer, VkDeviceMemory& _bufferMemory);
			void CreateBuffer(const VulkanRenderer& _vkRenderer, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, Buffer* buffer, VkDeviceSize size, void* data = nullptr);
			void CopyBuffer(const VulkanRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkBuffer _srcBuffer, VkBuffer _dstBuffer, VkDeviceSize _size);

			uint32_t FindMemoryType(const VulkanRenderer& _vkRenderer, uint32_t typeFilter, VkMemoryPropertyFlags properties);

			void CreateImage(const VulkanRenderer& _vkRenderer, uint32_t _width, uint32_t _height, VkFormat _format, VkImageTiling _tiling, VkImageUsageFlags _usage, VkMemoryPropertyFlags _properties, VkImage& _image, VkDeviceMemory& _imageMemory);
			void CopyBufferToImage(const VulkanRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkBuffer _buffer, VkImage _image, uint32_t _width, uint32_t _height);
			void TransitionImageLayout(const VulkanRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkImage _image, VkFormat _format, VkImageLayout _oldLayout, VkImageLayout _newLayout);
			void TransitionImageLayout(const VulkanRenderer& _vkRenderer, const VkCommandBuffer& _commandBuffer, VkImage _image, VkFormat _format, VkImageLayout _oldLayout, VkImageLayout _newLayout);

			VkFormat FindSupportedFormat(const VulkanRenderer& _vkRenderer, const std::vector<VkFormat>& _formats, VkImageTiling _tiling, VkFormatFeatureFlags _features);
			VkFormat FindDepthFormat(const VulkanRenderer& _vkRenderer);
			bool HasStencilComponent(VkFormat _format);

			// --------------------------------------------------------------------------------------------------------------------------- //
			//													Custom Vulkan Structure													   //
			// --------------------------------------------------------------------------------------------------------------------------- //
			VkSampler CreateTextureSampler(const VulkanRenderer& _vkRenderer);
			ImageWrap CreateImageWrap(const VulkanRenderer& _vkRenderer,
										uint32_t _width, uint32_t _height,
										VkFormat _format,
										VkImageUsageFlags _usage,
										VkMemoryPropertyFlags _properties, uint32_t _mipLevels);
			ImageWrap CreateBufferImage(const VulkanRenderer& _vkRenderer, VkExtent2D& _size);
		}
	}
}