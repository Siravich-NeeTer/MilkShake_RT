#include "VulkanHelper.h"

#include "Graphics/VulkanRenderer.h"
#include "Graphics/Buffer.h"
#include "Graphics/ImageWrap.h"

namespace MilkShake
{
	namespace Graphics
	{
		namespace Utility
		{
			uint32_t AlignedSize(uint32_t value, uint32_t alignment)
			{
				return (value + alignment - 1) & ~(alignment - 1);
			}
			size_t AlignedSize(size_t value, size_t alignment)
			{
				return (value + alignment - 1) & ~(alignment - 1);
			}

			VkDeviceSize AlignedVkSize(VkDeviceSize value, VkDeviceSize alignment)
			{
				return (value + alignment - 1) & ~(alignment - 1);
			}

			// --------------------------------------------------------------------------------------------------------------------------
			//                                              Vulkan Core Utilities Function
			// --------------------------------------------------------------------------------------------------------------------------
			VkCommandBuffer BeginSingleTimeCommands(const VulkanRenderer& _vkRenderer, const VkCommandPool& _commandPool)
			{
				VkCommandBufferAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				allocInfo.commandPool = _commandPool;
				allocInfo.commandBufferCount = 1;

				VkCommandBuffer commandBuffer;
				vkAllocateCommandBuffers(_vkRenderer.GetDevice(), &allocInfo, &commandBuffer);

				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

				vkBeginCommandBuffer(commandBuffer, &beginInfo);

				return commandBuffer;
			}
			void EndSingleTimeCommands(const VulkanRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkCommandBuffer commandBuffer)
			{
				vkEndCommandBuffer(commandBuffer);

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &commandBuffer;

				vkQueueSubmit(_vkRenderer.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
				vkQueueWaitIdle(_vkRenderer.GetGraphicsQueue());

				vkFreeCommandBuffers(_vkRenderer.GetDevice(), _commandPool, 1, &commandBuffer);
			}
			VkImageView CreateImageView(const VulkanRenderer& _vkRenderer, VkImage image, VkFormat format, VkImageAspectFlags _aspectFlags)
			{
				VkImageViewCreateInfo viewInfo{};
				viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				viewInfo.image = image;
				viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewInfo.format = format;
				viewInfo.subresourceRange.aspectMask = _aspectFlags;
				viewInfo.subresourceRange.baseMipLevel = 0;
				viewInfo.subresourceRange.levelCount = 1;
				viewInfo.subresourceRange.baseArrayLayer = 0;
				viewInfo.subresourceRange.layerCount = 1;

				VkImageView imageView;
				if (vkCreateImageView(_vkRenderer.GetDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
				{
					throw std::runtime_error("Failed to create texture image view!");
				}

				return imageView;
			}

			void CreateBuffer(const VulkanRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkDeviceSize _size, VkBufferUsageFlags _usage, VkMemoryPropertyFlags _properties, VkBuffer& _buffer, VkDeviceMemory& _bufferMemory)
			{
				VkBufferCreateInfo bufferInfo{};
				bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferInfo.size = _size;
				bufferInfo.usage = _usage;
				bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				if (vkCreateBuffer(_vkRenderer.GetDevice(), &bufferInfo, nullptr, &_buffer) != VK_SUCCESS) {
					throw std::runtime_error("failed to create buffer!");
				}

				VkMemoryRequirements memRequirements;
				vkGetBufferMemoryRequirements(_vkRenderer.GetDevice(), _buffer, &memRequirements);

				VkMemoryAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				allocInfo.allocationSize = memRequirements.size;
				allocInfo.memoryTypeIndex = FindMemoryType(_vkRenderer, memRequirements.memoryTypeBits, _properties);

				if (vkAllocateMemory(_vkRenderer.GetDevice(), &allocInfo, nullptr, &_bufferMemory) != VK_SUCCESS) {
					throw std::runtime_error("failed to allocate buffer memory!");
				}

				vkBindBufferMemory(_vkRenderer.GetDevice(), _buffer, _bufferMemory, 0);
			}
			void CreateBuffer(const VulkanRenderer& _vkRenderer, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, Buffer* buffer, VkDeviceSize size, void* data)
			{
				buffer->device = _vkRenderer.GetDevice();

				// Create the buffer handle
				VkBufferCreateInfo bufferCreateInfo{};
				bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferCreateInfo.usage = usageFlags;
				bufferCreateInfo.size = size;
				vkCreateBuffer(_vkRenderer.GetDevice(), &bufferCreateInfo, nullptr, &buffer->buffer);

				// Create the memory backing up the buffer handle
				VkMemoryRequirements memReqs;
				VkMemoryAllocateInfo memAlloc = {};
				memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				vkGetBufferMemoryRequirements(_vkRenderer.GetDevice(), buffer->buffer, &memReqs);
				memAlloc.allocationSize = memReqs.size;
				// Find a memory type index that fits the properties of the buffer
				memAlloc.memoryTypeIndex = Utility::FindMemoryType(_vkRenderer, memReqs.memoryTypeBits, memoryPropertyFlags);

				// If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
				VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
				if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
					allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
					allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
					memAlloc.pNext = &allocFlagsInfo;
				}
				vkAllocateMemory(_vkRenderer.GetDevice(), &memAlloc, nullptr, &buffer->memory);

				buffer->alignment = memReqs.alignment;
				buffer->size = size;
				buffer->usageFlags = usageFlags;
				buffer->memoryPropertyFlags = memoryPropertyFlags;

				// If a pointer to the buffer data has been passed, map the buffer and copy over the data
				if (data != nullptr)
				{
					buffer->Map();
					memcpy(buffer->mapped, data, size);
					if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
						buffer->Flush();

					buffer->UnMap();
				}

				// Initialize a default descriptor that covers the whole buffer size
				buffer->SetupDescriptor();

				// Attach the memory to the buffer object
				buffer->Bind();
			}
			void CopyBuffer(const VulkanRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkBuffer _srcBuffer, VkBuffer _dstBuffer, VkDeviceSize _size)
			{
				VkCommandBuffer commandBuffer = BeginSingleTimeCommands(_vkRenderer, _commandPool);

				VkBufferCopy copyRegion{};
				copyRegion.size = _size;
				vkCmdCopyBuffer(commandBuffer, _srcBuffer, _dstBuffer, 1, &copyRegion);

				EndSingleTimeCommands(_vkRenderer, _commandPool, commandBuffer);
			}
			
			uint32_t FindMemoryType(const VulkanRenderer& _vkRenderer, uint32_t typeFilter, VkMemoryPropertyFlags properties)
			{
				VkPhysicalDeviceMemoryProperties memProperties;
				vkGetPhysicalDeviceMemoryProperties(_vkRenderer.GetPhysicalDevice(), &memProperties);

				for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
				{
					if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
					{
						return i;
					}
				}

				throw std::runtime_error("Failed to find suitable memory type!");
			}

			void CreateImage(const VulkanRenderer& _vkRenderer, uint32_t _width, uint32_t _height, VkFormat _format, VkImageTiling _tiling, VkImageUsageFlags _usage, VkMemoryPropertyFlags _properties, VkImage& _image, VkDeviceMemory& _imageMemory)
			{
				VkImageCreateInfo imageInfo{};
				imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				imageInfo.imageType = VK_IMAGE_TYPE_2D;
				imageInfo.extent.width = _width;
				imageInfo.extent.height = _height;
				imageInfo.extent.depth = 1;
				imageInfo.mipLevels = 1;
				imageInfo.arrayLayers = 1;
				imageInfo.format = _format;
				imageInfo.tiling = _tiling;
				imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageInfo.usage = _usage;
				imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				if (vkCreateImage(_vkRenderer.GetDevice(), &imageInfo, nullptr, &_image) != VK_SUCCESS)
				{
					throw std::runtime_error("Failed to create image!");
				}

				VkMemoryRequirements memRequirements;
				vkGetImageMemoryRequirements(_vkRenderer.GetDevice(), _image, &memRequirements);

				VkMemoryAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				allocInfo.allocationSize = memRequirements.size;
				allocInfo.memoryTypeIndex = FindMemoryType(_vkRenderer, memRequirements.memoryTypeBits, _properties);

				if (vkAllocateMemory(_vkRenderer.GetDevice(), &allocInfo, nullptr, &_imageMemory) != VK_SUCCESS)
				{
					throw std::runtime_error("Failed to allocate image memory!");
				}

				vkBindImageMemory(_vkRenderer.GetDevice(), _image, _imageMemory, 0);
			}
			void CopyBufferToImage(const VulkanRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkBuffer _buffer, VkImage _image, uint32_t _width, uint32_t _height)
			{
				VkCommandBuffer commandBuffer = BeginSingleTimeCommands(_vkRenderer, _commandPool);

				VkBufferImageCopy region{};
				region.bufferOffset = 0;
				region.bufferRowLength = 0;
				region.bufferImageHeight = 0;
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;
				region.imageOffset = { 0, 0, 0 };
				region.imageExtent = {
					_width,
					_height,
					1
				};

				vkCmdCopyBufferToImage(commandBuffer, _buffer, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				EndSingleTimeCommands(_vkRenderer, _commandPool, commandBuffer);
			}
			void TransitionImageLayout(const VulkanRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkImage _image, VkFormat _format, VkImageLayout _oldLayout, VkImageLayout _newLayout)
			{
				VkCommandBuffer commandBuffer = BeginSingleTimeCommands(_vkRenderer, _commandPool);
				TransitionImageLayout(_vkRenderer, commandBuffer, _image, _format, _oldLayout, _newLayout);
				EndSingleTimeCommands(_vkRenderer, _commandPool, commandBuffer);
			}
			void TransitionImageLayout(const VulkanRenderer& _vkRenderer, const VkCommandBuffer& _commandBuffer, VkImage _image, VkFormat _format, VkImageLayout _oldLayout, VkImageLayout _newLayout)
			{
				VkImageMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = _oldLayout;
				barrier.newLayout = _newLayout;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = _image;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;

				VkPipelineStageFlags sourceStage;
				VkPipelineStageFlags destinationStage;

				sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
				destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

				// Source layouts (old)
				// Source access mask controls actions that have to be finished on the old layout
				// before it will be transitioned to the new layout
				switch (_oldLayout)
				{
					case VK_IMAGE_LAYOUT_UNDEFINED:
						// Image layout is undefined (or does not matter)
						// Only valid as initial layout
						// No flags required, listed only for completeness
						barrier.srcAccessMask = 0;
						break;

					case VK_IMAGE_LAYOUT_PREINITIALIZED:
						// Image is preinitialized
						// Only valid as initial layout for linear images, preserves memory contents
						// Make sure host writes have been finished
						barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
						break;

					case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
						// Image is a color attachment
						// Make sure any writes to the color buffer have been finished
						barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
						break;

					case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
						// Image is a depth/stencil attachment
						// Make sure any writes to the depth/stencil buffer have been finished
						barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
						break;

					case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
						// Image is a transfer source
						// Make sure any reads from the image have been finished
						barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						break;

					case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
						// Image is a transfer destination
						// Make sure any writes to the image have been finished
						barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						break;

					case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
						// Image is read by a shader
						// Make sure any shader reads from the image have been finished
						barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
						break;
					default:
						// Other source layouts aren't handled (yet)
						break;
				}

				// Target layouts (new)
				// Destination access mask controls the dependency for the new image layout
				switch (_newLayout)
				{
					case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
						// Image will be used as a transfer destination
						// Make sure any writes to the image have been finished
						barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						break;

					case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
						// Image will be used as a transfer source
						// Make sure any reads from the image have been finished
						barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						break;

					case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
						// Image will be used as a color attachment
						// Make sure any writes to the color buffer have been finished
						barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
						break;

					case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
						// Image layout will be used as a depth/stencil attachment
						// Make sure any writes to depth/stencil buffer have been finished
						barrier.dstAccessMask = barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
						break;

					case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
						// Image will be read in a shader (sampler, input attachment)
						// Make sure any writes to the image have been finished
						if (barrier.srcAccessMask == 0)
						{
							barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
						}
						barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
						break;
					default:
						// Other source layouts aren't handled (yet)
						break;
				}

				vkCmdPipelineBarrier(
					_commandBuffer,
					sourceStage, destinationStage,
					0,
					0, nullptr,
					0, nullptr,
					1, &barrier
				);
			}

			VkFormat FindSupportedFormat(const VulkanRenderer& _vkRenderer, const std::vector<VkFormat>& _formats, VkImageTiling _tiling, VkFormatFeatureFlags _features)
			{
				for (const VkFormat& format : _formats)
				{
					VkFormatProperties props;
					vkGetPhysicalDeviceFormatProperties(_vkRenderer.GetPhysicalDevice(), format, &props);

					if (_tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & _features) == _features)
					{
						return format;
					}
					else if (_tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & _features) == _features)
					{
						return format;
					}
				}

				throw std::runtime_error("Failed to find supported format!");
			}
			VkFormat FindDepthFormat(const VulkanRenderer& _vkRenderer)
			{
				return FindSupportedFormat(_vkRenderer, { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
					VK_IMAGE_TILING_OPTIMAL,
					VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
			}
			bool HasStencilComponent(VkFormat _format)
			{
				return _format == VK_FORMAT_D32_SFLOAT_S8_UINT || _format == VK_FORMAT_D24_UNORM_S8_UINT;
			}

			// --------------------------------------------------------------------------------------------------------------------------- //
			//													Custom Vulkan Structure													   //
			// --------------------------------------------------------------------------------------------------------------------------- //
			VkSampler CreateTextureSampler(const VulkanRenderer& _vkRenderer)
			{
				VkPhysicalDeviceProperties properties{};
				vkGetPhysicalDeviceProperties(_vkRenderer.GetPhysicalDevice(), &properties);

				VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
				samplerInfo.magFilter = VK_FILTER_LINEAR;
				samplerInfo.minFilter = VK_FILTER_LINEAR;
				samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
				samplerInfo.anisotropyEnable = VK_TRUE;
				samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
				samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
				samplerInfo.unnormalizedCoordinates = VK_FALSE;
				samplerInfo.compareEnable = VK_FALSE;
				samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
				samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

				VkSampler textureSampler;
				if (vkCreateSampler(_vkRenderer.GetDevice(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) 
				{
					throw std::runtime_error("failed to create texture sampler!");
				}
				return textureSampler;
			}
			ImageWrap CreateImageWrap(const VulkanRenderer& _vkRenderer,
										uint32_t _width, uint32_t _height,
										VkFormat _format,
										VkImageUsageFlags _usage,
										VkMemoryPropertyFlags _properties, uint32_t _mipLevels)
			{
				ImageWrap myImage;

				VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
				imageInfo.imageType = VK_IMAGE_TYPE_2D;
				imageInfo.extent.width = _width;
				imageInfo.extent.height = _height;
				imageInfo.extent.depth = 1;
				imageInfo.mipLevels = _mipLevels;
				imageInfo.arrayLayers = 1;
				imageInfo.format = _format;
				imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageInfo.usage = _usage;
				imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				vkCreateImage(_vkRenderer.GetDevice(), &imageInfo, nullptr, &myImage.image);

				VkMemoryRequirements memRequirements;
				vkGetImageMemoryRequirements(_vkRenderer.GetDevice(), myImage.image, &memRequirements);

				VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
				allocInfo.allocationSize = memRequirements.size;
				allocInfo.memoryTypeIndex = FindMemoryType(_vkRenderer, memRequirements.memoryTypeBits, _properties);

				// TODO: Error Handler
				VkResult result;
				result = vkAllocateMemory(_vkRenderer.GetDevice(), &allocInfo, nullptr, &myImage.memory);
				result = vkBindImageMemory(_vkRenderer.GetDevice(), myImage.image, myImage.memory, 0);

				myImage.imageView = VK_NULL_HANDLE;
				myImage.sampler = VK_NULL_HANDLE;

				return myImage;
			}
			ImageWrap CreateBufferImage(const VulkanRenderer& _vkRenderer, VkExtent2D& _size)
			{
				// TODO: SHould I handle mipmap things???
				//uint mipLevels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
				uint mipLevels = 1;

				ImageWrap myImage = CreateImageWrap(_vkRenderer,
					_size.width, _size.height, VK_FORMAT_R32G32B32A32_SFLOAT,
					VK_IMAGE_USAGE_TRANSFER_DST_BIT
					| VK_IMAGE_USAGE_SAMPLED_BIT
					| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
					| VK_IMAGE_USAGE_STORAGE_BIT
					| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					mipLevels);

				myImage.imageView = CreateImageView(_vkRenderer, myImage.image, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
				myImage.sampler = CreateTextureSampler(_vkRenderer);
				myImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				return myImage;
			}
		}
	}
}