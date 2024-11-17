#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stb_image.h>

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>

#include "Utilities/VKUtilities.h"
#include "Utilities/VKValidation.h"

namespace MilkShake
{
    namespace Graphics
    {
        const uint32_t WIDTH = 800;
        const uint32_t HEIGHT = 600;

        const int MAX_FRAMES_IN_FLIGHT = 2;

        const std::vector<const char*> validationLayers =
        {
            "VK_LAYER_KHRONOS_validation"
        };

        const std::vector<const char*> deviceExtensions =
        {
            // Window Swapchain
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,

            // For Ray-Tracing Stuff
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,      // Validation tell me to add this! (Related: VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,    // Validation tell me to add this! (Related: VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)

            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,                // Validation tell me to add this! (Related: VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
            VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,    // Validation tell me to add this! (Related: VK_KHR_SPIRV_1_4_EXTENSION_NAME)
            // - Required by Ray-Tracing Pipeline
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
        };

        #ifdef NDEBUG
                const bool enableValidationLayers = false;
        #else
                const bool enableValidationLayers = true;
        #endif

        struct QueueFamilyIndices
        {
            std::optional<uint32_t> graphicsFamily;
            std::optional<uint32_t> presentFamily;

            bool isComplete()
            {
                return graphicsFamily.has_value() && presentFamily.has_value();
            }
        };

        struct SwapChainSupportDetails
        {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 color;
            glm::vec2 texCoord;

            static VkVertexInputBindingDescription getBindingDescription()
            {
                VkVertexInputBindingDescription bindingDescription{};
                bindingDescription.binding = 0;
                bindingDescription.stride = sizeof(Vertex);
                bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

                return bindingDescription;
            }

            static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
            {
                std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

                attributeDescriptions[0].binding = 0;
                attributeDescriptions[0].location = 0;
                attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
                attributeDescriptions[0].offset = offsetof(Vertex, pos);

                attributeDescriptions[1].binding = 0;
                attributeDescriptions[1].location = 1;
                attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
                attributeDescriptions[1].offset = offsetof(Vertex, color);

                attributeDescriptions[2].binding = 0;
                attributeDescriptions[2].location = 2;
                attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
                attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

                return attributeDescriptions;
            }
        };

        struct UniformBufferObject
        {
            alignas(16) glm::mat4 model;
            alignas(16) glm::mat4 view;
            alignas(16) glm::mat4 proj;
        };

        const std::vector<Vertex> vertices =
        {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
            {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

            {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
        };

        const std::vector<uint16_t> indices =
        {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4
        };

        class VKRenderer
        {
            public:
                void Run()
                {
                    InitWindow();
                    InitVulkan();
                    MainLoop();
                    Cleanup();
                }

            private:
                GLFWwindow* m_Window;

                VkInstance m_Instance;
                VkDebugUtilsMessengerEXT m_DebugMessenger;
                VkSurfaceKHR m_Surface;

                VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
                VkDevice m_Device;

                VkQueue m_GraphicsQueue;
                VkQueue m_PresentQueue;

                VkSwapchainKHR m_SwapChain;
                std::vector<VkImage> m_SwapChainImages;
                VkFormat m_SwapChainImageFormat;
                VkExtent2D m_SwapChainExtent;
                std::vector<VkImageView> m_SwapChainImageViews;
                std::vector<VkFramebuffer> m_SwapChainFramebuffers;

                VkRenderPass m_RenderPass;
                VkDescriptorSetLayout m_DescriptorSetLayout;
                VkPipelineLayout m_PipelineLayout;
                VkPipeline m_GraphicsPipeline;

                VkCommandPool m_CommandPool;

                VkImage m_DepthImage;
                VkDeviceMemory m_DepthImageMemory;
                VkImageView m_DepthImageView;

                VkImage m_TextureImage;
                VkDeviceMemory m_TextureImageMemory;
                VkImageView m_TextureImageView;
                VkSampler m_TextureSampler;

                VkBuffer m_VertexBuffer;
                VkDeviceMemory m_VertexBufferMemory;
                VkBuffer m_IndexBuffer;
                VkDeviceMemory m_IndexBufferMemory;

                std::vector<VkBuffer> m_UniformBuffers;
                std::vector<VkDeviceMemory> m_UniformBuffersMemory;
                std::vector<void*> m_UniformBuffersMapped;

                VkDescriptorPool m_DescriptorPool;
                std::vector<VkDescriptorSet> m_DescriptorSets;

                std::vector<VkCommandBuffer> m_CommandBuffers;

                std::vector<VkSemaphore> m_ImageAvailableSemaphores;
                std::vector<VkSemaphore> m_RenderFinishedSemaphores;
                std::vector<VkFence> m_InFlightFences;
                uint32_t m_CurrentFrame = 0;

                bool m_FramebufferResized = false;

                // - Core Functions
                void InitWindow();
                void InitVulkan();
                void MainLoop();
                void Cleanup();
                // - Cleanup Functions
                void CleanupSwapChain();
                void RecreateSwapChain();
                // - Create Functions
                void CreateInstance();
                void SetupValidationLayer();
                void CreateSurface();
                void PickPhysicalDevice();
                void CreateLogicalDevice();
                void CreateSwapChain();
                void CreateImageViews();
                void CreateRenderPass();
                void CreateDescriptorSetLayout();
                void CreateGraphicsPipeline();
                void CreateFramebuffers();
                void CreateCommandPool();
                void CreateDepthResources();

                void CreateTextureImage();
                void CreateTextureImageView();
                void CreateTextureSampler();
                VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
                void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

                void CreateVertexBuffer();
                void CreateIndexBuffer();
                void CreateUniformBuffers();
                void CreateDescriptorPool();
                void CreateDescriptorSets();
                void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

                void CreateCommandBuffers();
                void CreateSyncObjects();
                // -- Ray Tracing Create Stuff
                void InitRayTracing();

                // - Helper Functions
                // -- Texture/Depth Functions
                VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
                VkFormat FindDepthFormat();
                bool HasStencilComponent(VkFormat format);
                // -- Image Buffer Functions
                void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
                void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
                // -- Command Functions
                VkCommandBuffer BeginSingleTimeCommands();
                void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
                // -- Buffer Functions
                void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
                uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
                // -- !!Renderer Loop Functions!!
                void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
                void UpdateUniformBuffer(uint32_t currentImage);
                void DrawFrame();
                // -- Shader Functions
                VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code);
                // -- Window Swapchain Functions
                VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
                VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
                VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
                SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
                // -- Core Checking Functions
                bool IsDeviceSuitable(VkPhysicalDevice device);
                bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
                QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
                std::vector<const char*> GetRequiredExtensions();
                bool CheckValidationLayerSupport();

                // - Callback Functions
                static void FramebufferResizeCallback(GLFWwindow* window, int width, int height)
                {
                    auto app = reinterpret_cast<VKRenderer*>(glfwGetWindowUserPointer(window));
                    app->m_FramebufferResized = true;
                }
        };
    }
}
