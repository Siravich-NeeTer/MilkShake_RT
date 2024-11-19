#pragma once

#include <filesystem>

#include <vulkan/vulkan.hpp>

namespace MilkShake
{
	namespace Graphics
	{
		class VKRenderer;

		class Texture
		{
			public:
				Texture(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool);
				Texture(const VKRenderer& _vkRenderer, const VkCommandPool& _commandPool, const std::filesystem::path& _texturePath);
				~Texture();

				void LoadTexture(const std::filesystem::path& _path);

				const VkImage& GetImage() const { return m_Image; }
				const VkDeviceMemory& GetDeviceMemory() const { return m_DeviceMemory; }
				const VkImageView& GetImageView() const { return m_ImageView; }
				const VkSampler& GetSampler() const { return m_Sampler; }

			private:
				const VKRenderer& m_VKRendererRef;
				const VkCommandPool& m_CommandPoolRef;

				int m_Width, m_Height;
				int m_Channels;

				VkImage m_Image;
				VkDeviceMemory m_DeviceMemory;
				VkImageView m_ImageView;
				VkSampler m_Sampler;

				void CreateTextureImage(const std::filesystem::path& _path);
				void CreateTextureImageView();
				void CreateTextureSampler();
		};
	}
}