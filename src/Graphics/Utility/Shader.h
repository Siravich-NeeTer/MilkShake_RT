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
		class VulkanRenderer;

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

			VkPipelineShaderStageCreateInfo LoadShader(VulkanRenderer& _vkRenderer, const std::filesystem::path& shaderPath);
			std::vector<uint32_t> ReadFile(const std::filesystem::path& shaderPath);
			shaderc_shader_kind GetShaderKind(const std::filesystem::path& shaderPath);
			VkShaderStageFlagBits GetShaderStageFlag(const std::filesystem::path& shaderPath);
			std::vector<uint32_t> CompileGLSLToSpirV(const std::filesystem::path& shaderPath);
		}
	}
}