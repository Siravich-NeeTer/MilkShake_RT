#include "VKUtilities.h"

#include "Renderer/VKRenderer.h"
#include "Renderer/Buffer.h"

namespace MilkShake
{
	namespace Graphics
	{
		namespace Shader
		{
			shaderc::Compiler compiler;
			shaderc::CompileOptions options;

			shaderc_include_result* IncludeCallback::GetInclude(
				const char* requested_source,
				shaderc_include_type type,
				const char* requesting_source,
				size_t requesting_source_length)
			{
				// Try to find the included file in the specified directories
				for (const auto& dir : includeDirs_) 
				{
					std::string fullPath = dir + requested_source;
					std::ifstream includeFile(fullPath);

					if (includeFile.is_open()) 
					{
						std::ostringstream ss;
						ss << includeFile.rdbuf();
						std::string fileContent = ss.str();

						// Allocate a result struct to return the file content
						shaderc_include_result* result = new shaderc_include_result;
						result->content = _strdup(fileContent.c_str());  // Copy the content of the include file
						result->content_length = fileContent.size();
						result->source_name = _strdup(requested_source);  // Set the source name
						result->source_name_length = strlen(requested_source);

						return result;
					}
				}

				// If the file couldn't be found, return nullptr
				return nullptr;
			}        
			void IncludeCallback::ReleaseInclude(shaderc_include_result* data) 
			{
				if (data) 
				{
					delete data->content;
					delete data->source_name;
					delete data;
				}
			}

			VkPipelineShaderStageCreateInfo LoadShader(VkDevice device, const std::filesystem::path& shaderPath)
			{
				std::vector<uint32_t> code = Shader::ReadFile(shaderPath);

				VkShaderModuleCreateInfo shaderModuleCreateInfo{};
				shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				shaderModuleCreateInfo.codeSize = code.size() * sizeof(uint32_t);
				shaderModuleCreateInfo.pCode = code.data();

				VkShaderModule shaderModule;
				if (vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
				{
					throw std::runtime_error("failed to create shader module!");
				}

				VkPipelineShaderStageCreateInfo shaderStageCreateInfo{};
				shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
				shaderStageCreateInfo.stage = GetShaderStageFlag(shaderPath);
				shaderStageCreateInfo.module = shaderModule;
				shaderStageCreateInfo.pName = "main";

				return shaderStageCreateInfo;
			}
			std::vector<uint32_t> ReadFile(const std::filesystem::path& shaderPath)
			{
				std::string fileExtension = shaderPath.extension().string();
				// If file is not compile (.vert, .frag, ...)
				if (fileExtension != ".spv")
				{
					return CompileGLSLToSpirV(shaderPath);
				}

				std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
				// TODO: Better Error Handler
				if (!file.is_open())
				{
					throw std::runtime_error("Failed to open " + shaderPath.string());
				}
				size_t fileSize = (size_t)file.tellg();
				std::vector<char> buffer(fileSize);

				file.seekg(0);
				file.read(buffer.data(), fileSize);
				file.close();

				// Cast the data
				const uint32_t* uint32Ptr = reinterpret_cast<const uint32_t*>(buffer.data());

				// Create the uint32_t vector using the casted data
				std::vector<uint32_t> uint32Vec(uint32Ptr, uint32Ptr + (buffer.size() / sizeof(uint32_t)));

				return uint32Vec;
			}
			shaderc_shader_kind GetShaderKind(const std::filesystem::path& shaderPath)
			{
				std::string fileExtension = shaderPath.extension().string();

				if (fileExtension == ".vert")
					return shaderc_vertex_shader;
				if (fileExtension == ".frag")
					return shaderc_fragment_shader;
				if (fileExtension == ".geom")
					return shaderc_geometry_shader;
				if (fileExtension == ".comp")
					return shaderc_compute_shader;
				if (fileExtension == ".rgen")
					return shaderc_raygen_shader;
				if (fileExtension == ".rmiss")
					return shaderc_miss_shader;
				if (fileExtension == ".rchit")
					return shaderc_closesthit_shader;
				if (fileExtension == ".rahit")
					return shaderc_anyhit_shader;
			}
			VkShaderStageFlagBits GetShaderStageFlag(const std::filesystem::path& shaderPath)
			{
				std::string fileExtension = shaderPath.extension().string();
				if (fileExtension == ".spv")
				{
					std::string newPath = shaderPath.string();
					newPath.erase(newPath.find(".spv"), 4);
					fileExtension = std::filesystem::path(newPath).extension().string();
				}

				if (fileExtension == ".vert")
					return VK_SHADER_STAGE_VERTEX_BIT;
				if (fileExtension == ".frag")
					return VK_SHADER_STAGE_FRAGMENT_BIT;
				if (fileExtension == ".geom")
					return VK_SHADER_STAGE_GEOMETRY_BIT;
				if (fileExtension == ".comp")
					return VK_SHADER_STAGE_COMPUTE_BIT;
				if (fileExtension == ".rgen")
					return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
				if (fileExtension == ".rmiss")
					return VK_SHADER_STAGE_MISS_BIT_KHR;
				if (fileExtension == ".rchit")
					return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
				if (fileExtension == ".rahit")
					return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

				return VK_SHADER_STAGE_ALL;
			}
			std::vector<uint32_t> CompileGLSLToSpirV(const std::filesystem::path& shaderPath)
			{
				std::ifstream file(shaderPath);
				// TODO: Better Error Handler
				if (!file.is_open())
				{
					throw std::runtime_error("Failed to open " + shaderPath.string());
				}
				std::ostringstream ss;
				ss << file.rdbuf();
				const std::string& s = ss.str();
				std::vector<char> shaderCode(s.begin(), s.end());

				// Set Compiler SPIR_V version to 1.4
				options.SetTargetSpirv(shaderc_spirv_version_1_4);

				// Define the directories to search for included files
				std::vector<std::string> includeDirs = { "assets/shaders/" };

				// Create the includer and set it in the compile options
				auto includer = std::make_unique<IncludeCallback>(includeDirs);
				options.SetIncluder(std::move(includer));

				shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
					shaderCode.data(),
					shaderCode.size(),
					GetShaderKind(shaderPath),
					shaderPath.filename().string().c_str(),
					"main",
					options
				);

				// Check if there were any compilation errors
				// TODO: Better Error Handler
				if (result.GetCompilationStatus() != shaderc_compilation_status_success)
				{
					throw std::runtime_error(result.GetErrorMessage());
				}

				// Return the compiled SPIR-V binary as a vector of uint32_t
				return std::vector<uint32_t>(result.begin(), result.end()); ;
			}
		}
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
			VkCommandBuffer BeginSingleTimeCommands(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool)
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
			void EndSingleTimeCommands(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkCommandBuffer commandBuffer)
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
			VkImageView CreateImageView(const VKRenderer& _vkRenderer, VkImage image, VkFormat format, VkImageAspectFlags _aspectFlags)
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

			void CreateBuffer(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkDeviceSize _size, VkBufferUsageFlags _usage, VkMemoryPropertyFlags _properties, VkBuffer& _buffer, VkDeviceMemory& _bufferMemory)
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
			void CreateBuffer(const VKRenderer& _vkRenderer, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, Buffer* buffer, VkDeviceSize size, void* data)
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
			void CopyBuffer(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkBuffer _srcBuffer, VkBuffer _dstBuffer, VkDeviceSize _size)
			{
				VkCommandBuffer commandBuffer = BeginSingleTimeCommands(_vkRenderer, _commandPool);

				VkBufferCopy copyRegion{};
				copyRegion.size = _size;
				vkCmdCopyBuffer(commandBuffer, _srcBuffer, _dstBuffer, 1, &copyRegion);

				EndSingleTimeCommands(_vkRenderer, _commandPool, commandBuffer);
			}
			
			uint32_t FindMemoryType(const VKRenderer& _vkRenderer, uint32_t typeFilter, VkMemoryPropertyFlags properties)
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

			void CreateImage(const VKRenderer& _vkRenderer, uint32_t _width, uint32_t _height, VkFormat _format, VkImageTiling _tiling, VkImageUsageFlags _usage, VkMemoryPropertyFlags _properties, VkImage& _image, VkDeviceMemory& _imageMemory)
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
			void CopyBufferToImage(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkBuffer _buffer, VkImage _image, uint32_t _width, uint32_t _height)
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
			void TransitionImageLayout(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkImage _image, VkFormat _format, VkImageLayout _oldLayout, VkImageLayout _newLayout)
			{
				VkCommandBuffer commandBuffer = BeginSingleTimeCommands(_vkRenderer, _commandPool);

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

				if (_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && _newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
				{
					barrier.srcAccessMask = 0;
					barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

					sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
					destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				}
				else if (_oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && _newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				{
					barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

					sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
					destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				}
				// TODO: For Ray-Tracing not sure that handle correctly or not
				else if (_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && _newLayout == VK_IMAGE_LAYOUT_GENERAL)
				{
					barrier.srcAccessMask = 0;

					sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
					destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
				}
				else
				{
					throw std::invalid_argument("unsupported layout transition!");
				}

				vkCmdPipelineBarrier(
					commandBuffer,
					sourceStage, destinationStage,
					0,
					0, nullptr,
					0, nullptr,
					1, &barrier
				);

				EndSingleTimeCommands(_vkRenderer, _commandPool, commandBuffer);
			}

			VkFormat FindSupportedFormat(const VKRenderer& _vkRenderer, const std::vector<VkFormat>& _formats, VkImageTiling _tiling, VkFormatFeatureFlags _features)
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
			VkFormat FindDepthFormat(const VKRenderer& _vkRenderer)
			{
				return FindSupportedFormat(_vkRenderer, { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
					VK_IMAGE_TILING_OPTIMAL,
					VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
			}
			bool HasStencilComponent(VkFormat _format)
			{
				return _format == VK_FORMAT_D32_SFLOAT_S8_UINT || _format == VK_FORMAT_D24_UNORM_S8_UINT;
			}
		}
	}
}