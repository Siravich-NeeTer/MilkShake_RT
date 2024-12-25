#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stb_image.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

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

#include "Model.h"
#include "Camera.h"

#include "shaders/shared_structs.h"

#include "Descriptor.h"

#include "Utilities/VKUtilities.h"
#include "Utilities/VKValidation.h"
#include "Utilities/AcclerationStructure.h"

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

        struct UniformBufferObject
        {
            alignas(16) glm::mat4 model;
            alignas(16) glm::mat4 view;
            alignas(16) glm::mat4 proj;
        };

        class VKRenderer
        {
            public:
                void Run()
                {
                    InitWindow();
                    InitVulkan();
                    InitImGui();
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

                VkCommandPool m_CommandPool;

                VkImage m_DepthImage;
                VkDeviceMemory m_DepthImageMemory;
                VkImageView m_DepthImageView;

                VkDescriptorPool m_DescriptorPool;
                std::vector<VkDescriptorSet> m_DescriptorSets;

                std::vector<VkCommandBuffer> m_CommandBuffers;

                std::vector<VkSemaphore> m_ImageAvailableSemaphores;
                std::vector<VkSemaphore> m_RenderFinishedSemaphores;
                std::vector<VkFence> m_InFlightFences;
                uint32_t m_CurrentFrame = 0;

                bool m_FramebufferResized = false;

                // Hold ShaderModule from Shader::LoadShader functions
                std::vector<VkShaderModule> m_ShaderModules;

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
                void CreateFramebuffers();
                void CreateCommandPool();
                void CreateDepthResources();

                VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
                void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
                void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);

                void CreateCommandBuffers();
                void CreateSyncObjects();

                // - Helper Functions
                // -- !!Renderer Loop Functions!!
                void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
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
            // Getter & Setter (Vulkan-Core Component)
            public:
                void AddShaderModule(const VkShaderModule& _shaderModule) { m_ShaderModules.push_back(_shaderModule); }

                VkDevice GetDevice() const { return m_Device; }
                VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
                VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }

            // Post-Process
            private:
                void CreatePostDescriptor();
                void CreatePostPipeline();

                VkPipelineLayout m_PostPipelineLayout;
                VkPipeline m_PostPipeline;
                VkRenderPass m_PostRenderPass;
                Descriptor m_PostDescriptor;


            // Loading Model & Texture Stuff
            public:
                int LoadModel(const std::filesystem::path& _filePath);
                int LoadTexture(const std::filesystem::path& _filePath);
                int CreateMaterial(Material _material);

            private:
                // TODO: Deal with m_ModelsMap
                std::vector<Model*> m_Models;
                std::map<std::filesystem::path, Model*> m_ModelsMap;

                std::vector<Texture*> m_Textures;
                std::map<std::filesystem::path, Texture*> m_TexturesMap;

                std::map<int, Material> m_MaterialsMap;

                Camera m_Camera;
                bool isCameraMove;

            // Acceleration Structures + Ray-Tracing
            private:
                // Function pointers for ray tracing related stuff
                PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
                PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
                PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
                PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
                PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
                PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
                PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
                PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
                PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
                PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;

                // Available features and properties
                VkPhysicalDeviceRayTracingPipelinePropertiesKHR  m_RayTracingPipelineProperties{};
                VkPhysicalDeviceAccelerationStructureFeaturesKHR m_AccelerationStructureFeatures{};

                VkPipeline m_RtPipeline;
                VkPipelineLayout m_RtPipelineLayout;

                VkDescriptorPool m_RtDescriptorPool;
                VkDescriptorSet m_RtDescriptorSet;
                VkDescriptorSetLayout m_RtDescriptorSetLayout;

                std::vector<AccelerationStructure> m_BottomLevelAS;
                AccelerationStructure m_TopLevelAS{};

                VkImage m_StorageImage;
                VkDeviceMemory m_StorageImageMemory;
                VkImageView m_StorageImageView;

                Buffer m_RtVertexBuffer;
                Buffer m_RtIndexBuffer;
                uint32_t m_RtIndexCount{ 0 };
                std::vector<Buffer> m_RtTransformBuffers;

                struct UniformData
                {
                    glm::mat4 viewInverse;
                    glm::mat4 projInverse;
                    uint32_t frame{ 0 };
                } uniformData;
                Buffer m_RtUniformBuffer;

                struct GeometryNode 
                {
                    uint64_t vertexBufferDeviceAddress;
                    uint64_t indexBufferDeviceAddress;
                    int32_t textureIndexBaseColor = -1;
                    int32_t textureIndexOcclusion = -1;
                };
                std::vector<GeometryNode> m_GeometryNodes;
                std::vector<int> m_BLAS_GeometryNodeOffsets;
                Buffer m_GeometryNodesBuffer;

                std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_ShaderGroups{};
                struct ShaderBindingTables
                {
                    ShaderBindingTable raygen;
                    ShaderBindingTable miss;
                    ShaderBindingTable hit;

                } m_ShaderBindingTables;

                // Core Ray-Tracing Function
                void InitRayTracing();
                void CleanRayTracing();

                // Create a scratch buffer to hold temporary data for a ray tracing acceleration structure
                ScratchBuffer CreateScratchBuffer(VkDeviceSize size);
                void DeleteScratchBuffer(ScratchBuffer& scratchBuffer);

                void CreateAccelerationStructure(AccelerationStructure& accelerationStructure, VkAccelerationStructureTypeKHR type, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);
                void DeleteAccelerationStructure(AccelerationStructure& accelerationStructure);

                void CreateAccelerationStructureBuffer(AccelerationStructure& accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);

                // Create the bottom level acceleration structure contains the scene's actual geometry (vertices, triangles)
                AccelerationStructure CreateBottomLevelAccelerationStructure(Model* model);
                // The top level acceleration structure contains the scene's object instances
                void CreateTopLevelAccelerationStructure();

                /*
                    Create the Shader Binding Tables that binds the programs and top-level acceleration structure

                    SBT Layout used in this sample:

                        /-------------------\
                        |       raygen      |
                        |-------------------|
                        |   miss + shadow   |
                        |-------------------|
                        |     hit + any     |
                        \-------------------/

                */
                void CreateShaderBindingTable(ShaderBindingTable& shaderBindingTable, uint32_t handleCount);
                void CreateShaderBindingTables();

                void CreateRayTracingPipeline();
                void CreateRayTracingDescriptorSets();
                void CreateRayTracingUniformBuffer();

                void UpdateRayTracingUniformBuffer();

                // Gets the device address from a buffer that's required for some of the buffers used for ray tracing
                uint64_t GetBufferDeviceAddress(VkBuffer buffer);
                VkStridedDeviceAddressRegionKHR GetSbtEntryStridedDeviceAddressRegion(VkBuffer buffer, uint32_t handleCount);

                void CreateStorageImage();
                void DestroyStorageImage();
                void ReCreateStorageImage();
            
            // ImGui stuff
            private:
                void InitImGui();
                void RenderImGui(size_t imageIndex);
        };
    }
}
