#pragma once

#include <vulkan/vulkan.hpp>
#include <shaderc/shaderc.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace MilkShake
{
	namespace Graphics
	{
		class VKRenderer;
		class Buffer;

		namespace Shader
		{
			extern shaderc::Compiler compiler;
			extern shaderc::CompileOptions options;

			// Callback class to handle #include directives
			class IncludeCallback : public shaderc::CompileOptions::IncluderInterface
			{
				public:
					IncludeCallback(const std::vector<std::string>& includeDirs) : includeDirs_(includeDirs) {}

					// This method is called when an #include is found in the shader code
					shaderc_include_result* GetInclude(
						const char* requested_source,
						shaderc_include_type type,
						const char* requesting_source,
						size_t requesting_source_length) override;

					// This method is called to release memory used for the include file
					void ReleaseInclude(shaderc_include_result* data) override;

				private:
					std::vector<std::string> includeDirs_;  // List of directories to search for include files
			};

			VkPipelineShaderStageCreateInfo LoadShader(VkDevice device, const std::filesystem::path& shaderPath);
			std::vector<uint32_t> ReadFile(const std::filesystem::path& shaderPath);
			shaderc_shader_kind GetShaderKind(const std::filesystem::path& shaderPath);
			VkShaderStageFlagBits GetShaderStageFlag(const std::filesystem::path& shaderPath);
			std::vector<uint32_t> CompileGLSLToSpirV(const std::filesystem::path& shaderPath);


		}
		namespace Utility
		{
			uint32_t AlignedSize(uint32_t value, uint32_t alignment);
			size_t AlignedSize(size_t value, size_t alignment);

			VkDeviceSize AlignedVkSize(VkDeviceSize value, VkDeviceSize alignment);


			VkCommandBuffer BeginSingleTimeCommands(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool);
			void EndSingleTimeCommands(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkCommandBuffer commandBuffer);
			VkImageView CreateImageView(const VKRenderer& _vkRenderer, VkImage image, VkFormat format, VkImageAspectFlags _aspectFlags);

			void CreateBuffer(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkDeviceSize _size, VkBufferUsageFlags _usage, VkMemoryPropertyFlags _properties, VkBuffer& _buffer, VkDeviceMemory& _bufferMemory);
			void CreateBuffer(const VKRenderer& _vkRenderer, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, Buffer* buffer, VkDeviceSize size, void* data = nullptr);
			void CopyBuffer(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkBuffer _srcBuffer, VkBuffer _dstBuffer, VkDeviceSize _size);

			uint32_t FindMemoryType(const VKRenderer& _vkRenderer, uint32_t typeFilter, VkMemoryPropertyFlags properties);

			void CreateImage(const VKRenderer& _vkRenderer, uint32_t _width, uint32_t _height, VkFormat _format, VkImageTiling _tiling, VkImageUsageFlags _usage, VkMemoryPropertyFlags _properties, VkImage& _image, VkDeviceMemory& _imageMemory);
			void CopyBufferToImage(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkBuffer _buffer, VkImage _image, uint32_t _width, uint32_t _height);
			void TransitionImageLayout(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, VkImage _image, VkFormat _format, VkImageLayout _oldLayout, VkImageLayout _newLayout);

			VkFormat FindSupportedFormat(const VKRenderer& _vkRenderer, const std::vector<VkFormat>& _formats, VkImageTiling _tiling, VkFormatFeatureFlags _features);
			VkFormat FindDepthFormat(const VKRenderer& _vkRenderer);
			bool HasStencilComponent(VkFormat _format);
		}
	}
}