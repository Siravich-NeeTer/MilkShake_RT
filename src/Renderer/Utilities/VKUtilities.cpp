#include "VKUtilities.h"

namespace MilkShake
{
	namespace Graphics
	{
		namespace Shader
		{
			shaderc::Compiler compiler;
			shaderc::CompileOptions options;

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
	}
}