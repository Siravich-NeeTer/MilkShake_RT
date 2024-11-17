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
		namespace Shader
		{
			extern shaderc::Compiler compiler;
			extern shaderc::CompileOptions options;

			std::vector<uint32_t> ReadFile(const std::filesystem::path& shaderPath);
			shaderc_shader_kind GetShaderKind(const std::filesystem::path& shaderPath);
			VkShaderStageFlagBits GetShaderStageFlag(const std::filesystem::path& shaderPath);
			std::vector<uint32_t> CompileGLSLToSpirV(const std::filesystem::path& shaderPath);
		}
	}
}