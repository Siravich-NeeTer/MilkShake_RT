#include "Texture.h"

#include <stb_image.h>

#include "Renderer/VKRenderer.h"
#include "Utilities/VKUtilities.h"

namespace MilkShake
{
	namespace Graphics
	{
		Texture::Texture(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool)
			: m_VKRendererRef(_vkRenderer), m_CommandPoolRef(_commandPool)
		{
		}
		Texture::Texture(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, const std::filesystem::path& _texturePath)
			: m_VKRendererRef(_vkRenderer), m_CommandPoolRef(_commandPool)
		{
			CreateTextureImage(_texturePath);
			CreateTextureImageView();
			CreateTextureSampler();
		}
		Texture::~Texture()
		{
			vkDestroySampler(m_VKRendererRef.GetDevice(), m_Sampler, nullptr);
			vkDestroyImageView(m_VKRendererRef.GetDevice(), m_ImageView, nullptr);

			vkDestroyImage(m_VKRendererRef.GetDevice(), m_Image, nullptr);
			vkFreeMemory(m_VKRendererRef.GetDevice(), m_DeviceMemory, nullptr);
		}

		void Texture::LoadTexture(const std::filesystem::path& _path)
		{
			CreateTextureImage(_path);
			CreateTextureImageView();
			CreateTextureSampler();
		}

		void Texture::CreateTextureImage(const std::filesystem::path& _path)
		{
			stbi_uc* pixels = stbi_load(_path.string().c_str(), &m_Width, &m_Height, &m_Channels, STBI_rgb_alpha);
			VkDeviceSize imageSize = m_Width * m_Height * 4;

			if (!pixels)
			{
				throw std::runtime_error("Failed to load texture image! (" + _path.string() + ")");
			}


			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			Utility::CreateBuffer(m_VKRendererRef, m_CommandPoolRef, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

			void* data;
			vkMapMemory(m_VKRendererRef.GetDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
			memcpy(data, pixels, static_cast<size_t>(imageSize));
			vkUnmapMemory(m_VKRendererRef.GetDevice(), stagingBufferMemory);

			stbi_image_free(pixels);

			Utility::CreateImage(m_VKRendererRef, m_Width, m_Height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Image, m_DeviceMemory);

			Utility::TransitionImageLayout(m_VKRendererRef, m_CommandPoolRef, m_Image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			Utility::CopyBufferToImage(m_VKRendererRef, m_CommandPoolRef, stagingBuffer, m_Image, static_cast<uint32_t>(m_Width), static_cast<uint32_t>(m_Height));
			Utility::TransitionImageLayout(m_VKRendererRef, m_CommandPoolRef, m_Image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			vkDestroyBuffer(m_VKRendererRef.GetDevice(), stagingBuffer, nullptr);
			vkFreeMemory(m_VKRendererRef.GetDevice(), stagingBufferMemory, nullptr);
		}
		void Texture::CreateTextureImageView()
		{
			m_ImageView = Utility::CreateImageView(m_VKRendererRef, m_Image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
		}
		void Texture::CreateTextureSampler()
		{
			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerInfo.anisotropyEnable = VK_TRUE;

			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(m_VKRendererRef.GetPhysicalDevice(), &properties);
			samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.mipLodBias = 0.0f;
			samplerInfo.minLod = 0.0f;
			samplerInfo.maxLod = 0.0f;

			if (vkCreateSampler(m_VKRendererRef.GetDevice(), &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create texture sampler!");
			}
		}
	}
}