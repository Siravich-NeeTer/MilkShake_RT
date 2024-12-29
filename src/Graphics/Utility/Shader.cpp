#include "Shader.h"

#include "Graphics/VulkanRenderer.h"

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

			VkPipelineShaderStageCreateInfo LoadShader(VulkanRenderer& _vkRenderer, const std::filesystem::path& shaderPath)
			{
				std::vector<uint32_t> code = Shader::ReadFile(shaderPath);

				VkShaderModuleCreateInfo shaderModuleCreateInfo{};
				shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
				shaderModuleCreateInfo.codeSize = code.size() * sizeof(uint32_t);
				shaderModuleCreateInfo.pCode = code.data();

				VkShaderModule shaderModule;
				if (vkCreateShaderModule(_vkRenderer.GetDevice(), &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
				{
					throw std::runtime_error("failed to create shader module!");
				}
				_vkRenderer.AddShaderModule(shaderModule);

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
	}
}