#include "VKRenderer.h"

namespace MilkShake
{
	namespace Graphics
	{
        void VKRenderer::InitWindow()
        {
            glfwInit();

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            m_Window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
            glfwSetWindowUserPointer(m_Window, this);
            glfwSetFramebufferSizeCallback(m_Window, FramebufferResizeCallback);

            glfwSetKeyCallback(m_Window, Input::KeyCallBack);
            glfwSetCursorPosCallback(m_Window, Input::CursorCallBack);
            glfwSetMouseButtonCallback(m_Window, Input::MouseCallBack);
            glfwSetScrollCallback(m_Window, Input::ScrollCallback);
        }
        void VKRenderer::InitVulkan()
        {
            CreateInstance();
            SetupValidationLayer();
            CreateSurface();
            PickPhysicalDevice();
            CreateLogicalDevice();

            CreateSwapChain();

            CreateImageViews();
            CreateRenderPass();
            CreateDescriptorSetLayout();
            // TODO: Already Create Ray-Tracing Pipeline
            //CreateGraphicsPipeline();
            CreateCommandPool();
            CreateDepthResources();
            CreateFramebuffers();

            CreateCommandBuffers();
            CreateSyncObjects();
            
            LoadModel("assets/models/Sponza/Sponza.gltf");

            InitRayTracing();
        }
        void VKRenderer::MainLoop()
        {
            m_Camera = Camera({ 0.0f, 0.0f, 1.0f });

            float prevTime = 0.0f;
            while (!glfwWindowShouldClose(m_Window))
            {
                float currentTime = glfwGetTime();
                float dt = currentTime - prevTime;

                glfwPollEvents();


                if (Input::IsKeyBeginPressed(GLFW_MOUSE_BUTTON_RIGHT))
                {
                    m_Camera.ResetMousePosition();
                    isCameraMove = true;
                    glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
                else if (Input::IsKeyEndPressed(GLFW_MOUSE_BUTTON_RIGHT))
                {
                    isCameraMove = false;
                    glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }

                if (Input::scroll > 0.0f || Input::scroll < 0.0f)
                {
                    m_Camera.MoveAlongZ(Input::scroll);
                }

                if (isCameraMove)
                {
                    m_Camera.ProcessMousesMovement();
                    m_Camera.Input(dt);

                    uniformData.frame = 0;
                }

                DrawFrame();
                Input::EndFrame();
                prevTime = currentTime;
            }

            vkDeviceWaitIdle(m_Device);
        }
        void VKRenderer::Cleanup()
        {
            for (auto& texture : m_Textures)
            {
                delete texture;
            }
            for (auto& model : m_Models)
            {
                delete model;
            }

            CleanRayTracing();
            for (auto& shaderModule : m_ShaderModules)
            {
                vkDestroyShaderModule(m_Device, shaderModule, nullptr);
            }
            m_ShaderModules.clear();

            CleanupSwapChain();

            vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);

            vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);

            vkDestroyDescriptorSetLayout(m_Device, m_DescriptorSetLayout, nullptr);

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
                vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
                vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
            }

            vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);

            vkDestroyDevice(m_Device, nullptr);

            if (enableValidationLayers)
            {
                vulkan::DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
            }

            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            vkDestroyInstance(m_Instance, nullptr);

            glfwDestroyWindow(m_Window);

            glfwTerminate();
        }

        void VKRenderer::CleanupSwapChain()
        {
            vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
            vkDestroyImage(m_Device, m_DepthImage, nullptr);
            vkFreeMemory(m_Device, m_DepthImageMemory, nullptr);

            for (auto framebuffer : m_SwapChainFramebuffers)
            {
                vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
            }

            for (auto imageView : m_SwapChainImageViews)
            {
                vkDestroyImageView(m_Device, imageView, nullptr);
            }

            vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
        }
        void VKRenderer::RecreateSwapChain()
        {
            int width = 0, height = 0;
            glfwGetFramebufferSize(m_Window, &width, &height);
            while (width == 0 || height == 0)
            {
                glfwGetFramebufferSize(m_Window, &width, &height);
                glfwWaitEvents();
            }

            vkDeviceWaitIdle(m_Device);

            CleanupSwapChain();

            CreateSwapChain();
            CreateImageViews();
            CreateDepthResources();
            CreateFramebuffers();
        }

        void VKRenderer::CreateInstance()
        {
            if (enableValidationLayers && !CheckValidationLayerSupport())
            {
                throw std::runtime_error("validation layers requested, but not available!");
            }

            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "Hello Triangle";
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "No Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_API_VERSION_1_1;

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;

            auto extensions = GetRequiredExtensions();
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
            if (enableValidationLayers)
            {
                createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
                createInfo.ppEnabledLayerNames = validationLayers.data();

                vulkan::SetupDebugMessengerCreateInfo(debugCreateInfo);
                createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
            }
            else
            {
                createInfo.enabledLayerCount = 0;

                createInfo.pNext = nullptr;
            }

            if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create m_Instance!");
            }

            // DEBUG: Print Vulkan Instance Extensions
            #ifdef MILKSHAKE_DEBUG
                std::cout << std::format("Vulkan Instance Extensions ({}) : \n", extensions.size());
                for (auto& extension : extensions)
                {
                    std::cout << extension << "\n";
                }
                std::cout << "\n";
            #endif
        }
        void VKRenderer::SetupValidationLayer()
        {
            if (!enableValidationLayers) return;

            VkDebugUtilsMessengerCreateInfoEXT createInfo;
            vulkan::SetupDebugMessengerCreateInfo(createInfo);

            if (vulkan::CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to set up debug messenger!");
            }
        }
        void VKRenderer::CreateSurface()
        {
            if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create window surface!");
            }
        }
        void VKRenderer::PickPhysicalDevice()
        {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

            if (deviceCount == 0)
            {
                throw std::runtime_error("failed to find GPUs with Vulkan support!");
            }

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

            for (const auto& device : devices)
            {
                if (IsDeviceSuitable(device))
                {
                    m_PhysicalDevice = device;
                    break;
                }
            }

            if (m_PhysicalDevice == VK_NULL_HANDLE)
            {
                throw std::runtime_error("failed to find a suitable GPU!");
            }

            #ifdef MILKSHAKE_DEBUG
                uint32_t extensionCount;
                vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr);
                std::vector<VkExtensionProperties> availableExtensions(extensionCount);
                vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, availableExtensions.data());

                // DEBUG: Print Vulkan Physical Device Properties
                std::cout << std::format("Vulkan Physical Device Extensions ({}) : \n", availableExtensions.size());
                for (auto& extension : availableExtensions)
                {
                    std::cout << extension.extensionName << "\n";
                }
                std::cout << "\n";
            #endif
        }
        void VKRenderer::CreateLogicalDevice()
        {
            QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

            float queuePriority = 1.0f;
            for (uint32_t queueFamily : uniqueQueueFamilies)
            {
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = queueFamily;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queuePriority;
                queueCreateInfos.push_back(queueCreateInfo);
            }


            VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracingPipelineFeatures{};
            raytracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            raytracingPipelineFeatures.rayTracingPipeline = VK_TRUE;

            VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
            accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            accelerationStructureFeatures.accelerationStructure = VK_TRUE;
            accelerationStructureFeatures.pNext = &raytracingPipelineFeatures;

            VkPhysicalDeviceVulkan12Features features12{};
            features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            features12.runtimeDescriptorArray = VK_TRUE;
            features12.descriptorIndexing = VK_TRUE;
            features12.bufferDeviceAddress = VK_TRUE;
            features12.pNext = &accelerationStructureFeatures;

            VkPhysicalDeviceFeatures2 features2{};
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &features12;

            // Let Vulkan fill in all features that hardware support
            vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &features2);

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            createInfo.pNext = &features2;
            createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
            createInfo.pQueueCreateInfos = queueCreateInfos.data();
            createInfo.pEnabledFeatures = nullptr;
            createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
            createInfo.ppEnabledExtensionNames = deviceExtensions.data();

            if (enableValidationLayers)
            {
                createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
                createInfo.ppEnabledLayerNames = validationLayers.data();
            }
            else
            {
                createInfo.enabledLayerCount = 0;
            }

            // DEBUG: Print Vulkan Device Extensions
            #ifdef MILKSHAKE_DEBUG
                std::cout << std::format("Vulkan Device Extensions ({}) : \n", deviceExtensions.size());
                for (auto& deviceExtension : deviceExtensions)
                {
                    std::cout << deviceExtension << "\n";
                }
                std::cout << "\n";
            #endif

            if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create logical device!");
            }

            vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
            vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue);
        }
        void VKRenderer::CreateSwapChain()
        {
            SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);

            VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
            VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
            VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

            uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
            if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
            {
                imageCount = swapChainSupport.capabilities.maxImageCount;
            }

            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = m_Surface;

            createInfo.minImageCount = imageCount;
            createInfo.imageFormat = surfaceFormat.format;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageExtent = extent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
            uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

            if (indices.graphicsFamily != indices.presentFamily)
            {
                createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                createInfo.queueFamilyIndexCount = 2;
                createInfo.pQueueFamilyIndices = queueFamilyIndices;
            }
            else
            {
                createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            }

            createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            createInfo.presentMode = presentMode;
            createInfo.clipped = VK_TRUE;

            if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create swap chain!");
            }

            vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
            m_SwapChainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, m_SwapChainImages.data());

            m_SwapChainImageFormat = surfaceFormat.format;
            m_SwapChainExtent = extent;
        }
        void VKRenderer::CreateImageViews()
        {
            m_SwapChainImageViews.resize(m_SwapChainImages.size());

            for (uint32_t i = 0; i < m_SwapChainImages.size(); i++)
            {
                m_SwapChainImageViews[i] = CreateImageView(m_SwapChainImages[i], m_SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
            }
        }
        void VKRenderer::CreateRenderPass()
        {
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = m_SwapChainImageFormat;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentDescription depthAttachment{};
            depthAttachment.format = Utility::FindDepthFormat(*this);
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkAttachmentReference colorAttachmentRef{};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentReference depthAttachmentRef{};
            depthAttachmentRef.attachment = 1;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;
            subpass.pDepthStencilAttachment = &depthAttachmentRef;

            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
            VkRenderPassCreateInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            renderPassInfo.pAttachments = attachments.data();
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 1;
            renderPassInfo.pDependencies = &dependency;

            if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create render pass!");
            }
        }
        void VKRenderer::CreateDescriptorSetLayout()
        {
            VkDescriptorSetLayoutBinding uboLayoutBinding{};
            uboLayoutBinding.binding = 0;
            uboLayoutBinding.descriptorCount = 1;
            uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboLayoutBinding.pImmutableSamplers = nullptr;
            uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

            VkDescriptorSetLayoutBinding samplerLayoutBinding{};
            samplerLayoutBinding.binding = 1;
            samplerLayoutBinding.descriptorCount = 1;
            samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerLayoutBinding.pImmutableSamplers = nullptr;
            samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };
            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            layoutInfo.pBindings = bindings.data();

            if (vkCreateDescriptorSetLayout(m_Device, &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create descriptor set layout!");
            }
        }
        void VKRenderer::CreateFramebuffers()
        {
            m_SwapChainFramebuffers.resize(m_SwapChainImageViews.size());

            for (size_t i = 0; i < m_SwapChainImageViews.size(); i++)
            {
                std::array<VkImageView, 2> attachments =
                {
                    m_SwapChainImageViews[i],
                    m_DepthImageView
                };

                VkFramebufferCreateInfo framebufferInfo{};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = m_RenderPass;
                framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                framebufferInfo.pAttachments = attachments.data();
                framebufferInfo.width = m_SwapChainExtent.width;
                framebufferInfo.height = m_SwapChainExtent.height;
                framebufferInfo.layers = 1;

                if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_SwapChainFramebuffers[i]) != VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create framebuffer!");
                }
            }
        }
        void VKRenderer::CreateCommandPool()
        {
            QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

            if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create graphics command pool!");
            }
        }
        void VKRenderer::CreateDepthResources()
        {
            VkFormat depthFormat = Utility::FindDepthFormat(*this);

            CreateImage(m_SwapChainExtent.width, m_SwapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_DepthImage, m_DepthImageMemory);
            m_DepthImageView = CreateImageView(m_DepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
        }

        VkImageView VKRenderer::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = format;
            viewInfo.subresourceRange.aspectMask = aspectFlags;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            VkImageView imageView;
            if (vkCreateImageView(m_Device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create texture image view!");
            }

            return imageView;
        }
        void VKRenderer::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
        {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = width;
            imageInfo.extent.height = height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.format = format;
            imageInfo.tiling = tiling;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = usage;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateImage(m_Device, &imageInfo, nullptr, &image) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create image!");
            }

            VkMemoryRequirements memRequirements;
            vkGetImageMemoryRequirements(m_Device, image, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = Utility::FindMemoryType(*this, memRequirements.memoryTypeBits, properties);

            if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to allocate image memory!");
            }

            vkBindImageMemory(m_Device, image, imageMemory, 0);
        }
        void VKRenderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
        {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = size;
            bufferInfo.usage = usage;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(m_Device, buffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = Utility::FindMemoryType(*this, memRequirements.memoryTypeBits, properties);

            if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to allocate buffer memory!");
            }

            vkBindBufferMemory(m_Device, buffer, bufferMemory, 0);
        }

        void VKRenderer::CreateCommandBuffers()
        {
            m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = m_CommandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = (uint32_t)m_CommandBuffers.size();

            if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to allocate command buffers!");
            }
        }
        void VKRenderer::CreateSyncObjects()
        {
            m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
                    vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
                    vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS)
                {
                    throw std::runtime_error("failed to create synchronization objects for a frame!");
                }
            }
        }

        void VKRenderer::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
        {
            /*
                Dispatch the ray tracing commands
            */
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_RtPipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_RtPipelineLayout, 0, 1, &m_RtDescriptorSet, 0, 0);

            PushConstantRay pcRay{};
            pcRay.lightPosition = glm::vec3(0.0f, 3.0f, 0.0f);
            pcRay.lightIntensity = 10.0f;
            vkCmdPushConstants(commandBuffer, m_RtPipelineLayout, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0, sizeof(PushConstantRay), &pcRay);

            VkStridedDeviceAddressRegionKHR emptySbtEntry = {};
            vkCmdTraceRaysKHR(
                commandBuffer,
                &m_ShaderBindingTables.raygen.stridedDeviceAddressRegion,
                &m_ShaderBindingTables.miss.stridedDeviceAddressRegion,
                &m_ShaderBindingTables.hit.stridedDeviceAddressRegion,
                &emptySbtEntry,
                m_SwapChainExtent.width,
                m_SwapChainExtent.height,
                1);

            Utility::TransitionImageLayout(*this, commandBuffer,
                m_StorageImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            Utility::TransitionImageLayout(*this, commandBuffer,
                m_SwapChainImages[imageIndex], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkImageCopy copyRegion{};
            copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            copyRegion.srcOffset = { 0, 0, 0 };
            copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            copyRegion.dstOffset = { 0, 0, 0 };
            copyRegion.extent = { m_SwapChainExtent.width, m_SwapChainExtent.height, 1 };
            vkCmdCopyImage(commandBuffer, m_StorageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_SwapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            Utility::TransitionImageLayout(*this, commandBuffer,
                m_StorageImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
            Utility::TransitionImageLayout(*this, commandBuffer,
                m_SwapChainImages[imageIndex], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        }

        void VKRenderer::DrawFrame()
        {
            vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

            uint32_t imageIndex;
            VkResult result = vkAcquireNextImageKHR(m_Device, m_SwapChain, UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                RecreateSwapChain();
                ReCreateStorageImage();
                uniformData.frame = 0;
                return;
            }
            else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            {
                throw std::runtime_error("failed to acquire swap chain image!");
            }

            UpdateRayTracingUniformBuffer();

            vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);
            vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            if (vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to begin recording command buffer!");
            }
            
            RecordCommandBuffer(m_CommandBuffers[m_CurrentFrame], imageIndex);
            //RenderImGui(imageIndex);

            if (vkEndCommandBuffer(m_CommandBuffers[m_CurrentFrame]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to record command buffer!");
            }

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;

            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

            VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentFrame] };
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to submit draw command buffer!");
            }

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            VkSwapchainKHR swapChains[] = { m_SwapChain };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;

            presentInfo.pImageIndices = &imageIndex;

            result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized)
            {
                m_FramebufferResized = false;
                RecreateSwapChain();
                ReCreateStorageImage();
                uniformData.frame = 0;
            }
            else if (result != VK_SUCCESS)
            {
                throw std::runtime_error("failed to present swap chain image!");
            }

            m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        }

        VkShaderModule VKRenderer::CreateShaderModule(const std::vector<uint32_t>& code)
        {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = code.size() * sizeof(uint32_t);
            createInfo.pCode = code.data();

            VkShaderModule shaderModule;
            if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create shader module!");
            }

            return shaderModule;
        }

        VkSurfaceFormatKHR VKRenderer::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
        {
            for (const auto& availableFormat : availableFormats)
            {
                if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return availableFormat;
                }
            }

            return availableFormats[0];
        }
        VkPresentModeKHR VKRenderer::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
        {
            for (const auto& availablePresentMode : availablePresentModes)
            {
                if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    return availablePresentMode;
                }
            }

            return VK_PRESENT_MODE_FIFO_KHR;
        }
        VkExtent2D VKRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
        {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            {
                return capabilities.currentExtent;
            }
            else
            {
                int width, height;
                glfwGetFramebufferSize(m_Window, &width, &height);

                VkExtent2D actualExtent =
                {
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)
                };

                actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
                actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

                return actualExtent;
            }
        }

        SwapChainSupportDetails VKRenderer::QuerySwapChainSupport(VkPhysicalDevice device)
        {
            SwapChainSupportDetails details;

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.capabilities);

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);

            if (formatCount != 0)
            {
                details.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.formats.data());
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

            if (presentModeCount != 0)
            {
                details.presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.presentModes.data());
            }

            return details;
        }
        bool VKRenderer::IsDeviceSuitable(VkPhysicalDevice device)
        {
            QueueFamilyIndices indices = FindQueueFamilies(device);

            bool extensionsSupported = CheckDeviceExtensionSupport(device);

            bool swapChainAdequate = false;
            if (extensionsSupported)
            {
                SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
                swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            }

            VkPhysicalDeviceFeatures supportedFeatures;
            vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

            return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
        }
        bool VKRenderer::CheckDeviceExtensionSupport(VkPhysicalDevice device)
        {
            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

            std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

            for (const auto& extension : availableExtensions)
            {
                requiredExtensions.erase(extension.extensionName);
            }

            return requiredExtensions.empty();
        }
        QueueFamilyIndices VKRenderer::FindQueueFamilies(VkPhysicalDevice device)
        {
            QueueFamilyIndices indices;

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            int i = 0;
            for (const auto& queueFamily : queueFamilies)
            {
                if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    indices.graphicsFamily = i;
                }

                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);

                if (presentSupport)
                {
                    indices.presentFamily = i;
                }

                if (indices.isComplete())
                {
                    break;
                }

                i++;
            }

            return indices;
        }
        std::vector<const char*> VKRenderer::GetRequiredExtensions()
        {
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;
            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            if (enableValidationLayers)
            {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }

            return extensions;
        }
        bool VKRenderer::CheckValidationLayerSupport()
        {
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            for (const char* layerName : validationLayers)
            {
                bool layerFound = false;

                for (const auto& layerProperties : availableLayers)
                {
                    if (strcmp(layerName, layerProperties.layerName) == 0)
                    {
                        layerFound = true;
                        break;
                    }
                }

                if (!layerFound)
                {
                    return false;
                }
            }

            return true;
        }

        // --------------------------------------------------------------------------------------------------------------------------
        //                                                     Post - Processing
        // --------------------------------------------------------------------------------------------------------------------------
        void VKRenderer::CreatePostDescriptor()
        {
            m_PostDescriptor.SetBindings(m_Device, {
                    {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}
                });
            //m_PostDescriptor.Write(m_Device, 0, m_scImageBuffer.Descriptor());
        }
        void VKRenderer::CreatePostPipeline()
        {
            auto vertShaderCode = Shader::ReadFile("assets/shaders/post.vert");
            auto fragShaderCode = Shader::ReadFile("assets/shaders/post.frag");

            VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
            VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

            VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
            vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertShaderStageInfo.module = vertShaderModule;
            vertShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragShaderStageInfo.module = fragShaderModule;
            fragShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

            VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = nullptr;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr;

            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            VkPipelineViewportStateCreateInfo viewportState{};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;

            VkPipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = VK_CULL_MODE_NONE;
            rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterizer.depthBiasEnable = VK_FALSE;

            VkPipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineDepthStencilStateCreateInfo depthStencil{};
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL; // TODO: WHY LESS_OR_EQUAL
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.stencilTestEnable = VK_FALSE;

            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
            colorBlending.blendConstants[0] = 0.0f;
            colorBlending.blendConstants[1] = 0.0f;
            colorBlending.blendConstants[2] = 0.0f;
            colorBlending.blendConstants[3] = 0.0f;

            std::vector<VkDynamicState> dynamicStates =
            {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            };
            VkPipelineDynamicStateCreateInfo dynamicState{};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
            dynamicState.pDynamicStates = dynamicStates.data();

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            //pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;

            if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PostPipelineLayout) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create pipeline layout!");
            }

            VkGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = &depthStencil;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = &dynamicState;
            pipelineInfo.layout = m_PostPipelineLayout;
            pipelineInfo.renderPass = m_PostRenderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

            if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_PostPipeline) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create graphics pipeline!");
            }

            vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
            vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);
        }

        // --------------------------------------------------------------------------------------------------------------------------
        //                                                     Load Model Stuff
        // --------------------------------------------------------------------------------------------------------------------------
        int VKRenderer::LoadModel(const std::filesystem::path& _filePath)
        {
            int id = m_Models.size();
            Model* newModel = new Model(id, "");
            newModel->LoadModel(*this, m_CommandPool, _filePath);

            m_Models.push_back(newModel);
            return id;
        }
        int VKRenderer::LoadTexture(const std::filesystem::path& _filePath)
        {
            auto targetTexture = m_TexturesMap.find(_filePath);
            if (targetTexture != m_TexturesMap.end())
                return targetTexture->second->GetAssetID();

            int id = m_TexturesMap.size();
            std::cout << "LOAD : " << _filePath << "\n";

            Texture* newTexture = new Texture(*this, m_CommandPool, _filePath, id, "");
            m_Textures.push_back(newTexture);
            m_TexturesMap[_filePath] = newTexture;

            return id;
        }
        int VKRenderer::CreateMaterial(Material _material)
        {
            int id = m_MaterialsMap.size();
            m_MaterialsMap[id] = _material;
            return id;
        }

        // --------------------------------------------------------------------------------------------------------------------------
        //                                                     Ray Tracing Stuff
        // --------------------------------------------------------------------------------------------------------------------------
        void VKRenderer::InitRayTracing()
        {
            // Requesting ray tracing properties
            m_RayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
            VkPhysicalDeviceProperties2 deviceProperties2{};
            deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            deviceProperties2.pNext = &m_RayTracingPipelineProperties;
            vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &deviceProperties2);

            m_AccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            VkPhysicalDeviceFeatures2 deviceFeatures2{};
            deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            deviceFeatures2.pNext = &m_AccelerationStructureFeatures;
            vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &deviceFeatures2);

            // Get the function pointers required for ray tracing
            vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(m_Device, "vkGetBufferDeviceAddressKHR"));
            vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(m_Device, "vkCmdBuildAccelerationStructuresKHR"));
            vkBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(m_Device, "vkBuildAccelerationStructuresKHR"));
            vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(m_Device, "vkCreateAccelerationStructureKHR"));
            vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(m_Device, "vkDestroyAccelerationStructureKHR"));
            vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(m_Device, "vkGetAccelerationStructureBuildSizesKHR"));
            vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(m_Device, "vkGetAccelerationStructureDeviceAddressKHR"));
            vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(m_Device, "vkCmdTraceRaysKHR"));
            vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(m_Device, "vkGetRayTracingShaderGroupHandlesKHR"));
            vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(m_Device, "vkCreateRayTracingPipelinesKHR"));

            for (Model* model : m_Models)
            {
                CreateBottomLevelAccelerationStructure(model);
            }
            CreateTopLevelAccelerationStructure();

            CreateStorageImage();
            CreateRayTracingUniformBuffer();
            CreateRayTracingPipeline();
            CreateShaderBindingTables();
            CreateRayTracingDescriptorSets();
        }
        void VKRenderer::CleanRayTracing()
        {
            vkDestroyPipeline(m_Device, m_RtPipeline, nullptr);
            vkDestroyPipelineLayout(m_Device, m_RtPipelineLayout, nullptr);
            vkDestroyDescriptorPool(m_Device, m_RtDescriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(m_Device, m_RtDescriptorSetLayout, nullptr);

            DestroyStorageImage();
            for (auto& blas : m_BottomLevelAS)
            {
                DeleteAccelerationStructure(blas);
            }
            DeleteAccelerationStructure(m_TopLevelAS);


            m_RtVertexBuffer.Destroy();
            m_RtIndexBuffer.Destroy();
            for (auto& rtTransformBuffer : m_RtTransformBuffers)
            {
                rtTransformBuffer.Destroy();
            }

            m_ShaderBindingTables.raygen.Destroy();
            m_ShaderBindingTables.miss.Destroy();
            m_ShaderBindingTables.hit.Destroy();

            m_RtUniformBuffer.Destroy();
            m_GeometryNodesBuffer.Destroy();
        }
        
        ScratchBuffer VKRenderer::CreateScratchBuffer(VkDeviceSize size)
        {
            ScratchBuffer scratchBuffer{};

            VkBufferCreateInfo bufferCreateInfo{};
            bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.size = size;
            bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            vkCreateBuffer(m_Device, &bufferCreateInfo, nullptr, &scratchBuffer.handle);

            VkMemoryRequirements memoryRequirements{};
            vkGetBufferMemoryRequirements(m_Device, scratchBuffer.handle, &memoryRequirements);

            VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
            memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

            VkMemoryAllocateInfo memoryAllocateInfo{};
            memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
            memoryAllocateInfo.allocationSize = memoryRequirements.size;
            memoryAllocateInfo.memoryTypeIndex = Utility::FindMemoryType(*this, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            vkAllocateMemory(m_Device, &memoryAllocateInfo, nullptr, &scratchBuffer.memory);
            vkBindBufferMemory(m_Device, scratchBuffer.handle, scratchBuffer.memory, 0);

            VkBufferDeviceAddressInfo bufferDeviceAddressInfo{};
            bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddressInfo.buffer = scratchBuffer.handle;
            scratchBuffer.deviceAddress = vkGetBufferDeviceAddressKHR(m_Device, &bufferDeviceAddressInfo);

            return scratchBuffer;
        }
        void VKRenderer::DeleteScratchBuffer(ScratchBuffer& scratchBuffer)
        {
            if (scratchBuffer.memory != VK_NULL_HANDLE) {
                vkFreeMemory(m_Device, scratchBuffer.memory, nullptr);
            }
            if (scratchBuffer.handle != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_Device, scratchBuffer.handle, nullptr);
            }
        }

        void VKRenderer::CreateAccelerationStructure(AccelerationStructure& accelerationStructure, VkAccelerationStructureTypeKHR type, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
        {
            // Buffer & Memory
            VkBufferCreateInfo bufferCreateInfo{};
            bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
            bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            vkCreateBuffer(m_Device, &bufferCreateInfo, nullptr, &accelerationStructure.buffer);

            VkMemoryRequirements memoryRequirements{};
            vkGetBufferMemoryRequirements(m_Device, accelerationStructure.buffer, &memoryRequirements);

            VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
            memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

            VkMemoryAllocateInfo memoryAllocateInfo{};
            memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
            memoryAllocateInfo.allocationSize = memoryRequirements.size;
            memoryAllocateInfo.memoryTypeIndex = Utility::FindMemoryType(*this, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            vkAllocateMemory(m_Device, &memoryAllocateInfo, nullptr, &accelerationStructure.memory);
            vkBindBufferMemory(m_Device, accelerationStructure.buffer, accelerationStructure.memory, 0);

            // Acceleration Structure
            VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
            accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            accelerationStructureCreateInfo.buffer = accelerationStructure.buffer;
            accelerationStructureCreateInfo.size = buildSizeInfo.accelerationStructureSize;
            accelerationStructureCreateInfo.type = type;
            // Acceleration Structure Device Address
            VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
            accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            accelerationDeviceAddressInfo.accelerationStructure = accelerationStructure.handle;
            accelerationStructure.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(m_Device, &accelerationDeviceAddressInfo);
        }
        void VKRenderer::DeleteAccelerationStructure(AccelerationStructure& accelerationStructure)
        {
            vkFreeMemory(m_Device, accelerationStructure.memory, nullptr);
            vkDestroyBuffer(m_Device, accelerationStructure.buffer, nullptr);
            vkDestroyAccelerationStructureKHR(m_Device, accelerationStructure.handle, nullptr);
        }

        void VKRenderer::CreateAccelerationStructureBuffer(AccelerationStructure& accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
        {
            VkBufferCreateInfo bufferCreateInfo{};
            bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.size = buildSizeInfo.accelerationStructureSize;
            bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

            vkCreateBuffer(m_Device, &bufferCreateInfo, nullptr, &accelerationStructure.buffer);

            VkMemoryRequirements memoryRequirements{};
            vkGetBufferMemoryRequirements(m_Device, accelerationStructure.buffer, &memoryRequirements);

            VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo{};
            memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

            VkMemoryAllocateInfo memoryAllocateInfo{};
            memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memoryAllocateInfo.pNext = &memoryAllocateFlagsInfo;
            memoryAllocateInfo.allocationSize = memoryRequirements.size;
            memoryAllocateInfo.memoryTypeIndex = Utility::FindMemoryType(*this, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            vkAllocateMemory(m_Device, &memoryAllocateInfo, nullptr, &accelerationStructure.memory);
            vkBindBufferMemory(m_Device, accelerationStructure.buffer, accelerationStructure.memory, 0);
        }

        AccelerationStructure VKRenderer::CreateBottomLevelAccelerationStructure(Model* model)
        {
            AccelerationStructure blas;

            m_BLAS_GeometryNodeOffsets.push_back(m_GeometryNodes.size());

            std::vector<VkTransformMatrixKHR> transformMatrices{};
            for (auto node : model->GetLinearNode())
            {
                if (!node->mesh.primitives.empty())
                {
                    for (auto primitive : node->mesh.primitives)
                    {
                        if (primitive.indexCount > 0)
                        {
                            VkTransformMatrixKHR transformMatrix{};
                            auto m = glm::mat3x4(glm::transpose(node->GetWorldMatrix()));
                            memcpy(&transformMatrix, (void*)&m, sizeof(glm::mat3x4));
                            transformMatrices.push_back(transformMatrix);
                        }
                    }
                }
            }

            Buffer transformBuffer;
            Utility::CreateBuffer(*this,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &transformBuffer, static_cast<uint32_t>(transformMatrices.size()) * sizeof(VkTransformMatrixKHR),
                transformMatrices.data());
            m_RtTransformBuffers.push_back(transformBuffer);

            // Build
            // One geometry per glTF node, so we can index materials using gl_GeometryIndexEXT
            uint32_t maxPrimCount{ 0 };
            std::vector<uint32_t> maxPrimitiveCounts{};
            std::vector<VkAccelerationStructureGeometryKHR> geometries{};
            std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos{};
            std::vector<VkAccelerationStructureBuildRangeInfoKHR*> pBuildRangeInfos{};
            std::vector<GeometryNode> geometryNodes{};
            for (auto node : model->GetLinearNode())
            {
                if (!node->mesh.primitives.empty())
                {
                    for (auto primitive : node->mesh.primitives)
                    {
                        if (primitive.indexCount > 0)
                        {
                            VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
                            VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
                            VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

                            vertexBufferDeviceAddress.deviceAddress = GetBufferDeviceAddress(model->GetVertexBuffer().buffer);
                            indexBufferDeviceAddress.deviceAddress = GetBufferDeviceAddress(model->GetIndexBuffer().buffer) + primitive.firstIndex * sizeof(uint32_t);
                            transformBufferDeviceAddress.deviceAddress = GetBufferDeviceAddress(transformBuffer.buffer) + static_cast<uint32_t>(geometryNodes.size()) * sizeof(VkTransformMatrixKHR);

                            VkAccelerationStructureGeometryKHR geometry{};
                            geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                            geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                            geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                            geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
                            geometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
                            geometry.geometry.triangles.maxVertex = model->GetVertices().size();
                            //geometry.geometry.triangles.maxVertex = primitive->vertexCount;
                            geometry.geometry.triangles.vertexStride = sizeof(VertexObject);
                            geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
                            geometry.geometry.triangles.indexData = indexBufferDeviceAddress;
                            geometry.geometry.triangles.transformData = transformBufferDeviceAddress;
                            geometries.push_back(geometry);
                            maxPrimitiveCounts.push_back(primitive.indexCount / 3);
                            maxPrimCount += primitive.indexCount / 3;

                            VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
                            buildRangeInfo.firstVertex = 0;
                            buildRangeInfo.primitiveOffset = 0; // primitive->firstIndex * sizeof(uint32_t);
                            buildRangeInfo.primitiveCount = primitive.indexCount / 3;
                            buildRangeInfo.transformOffset = 0;
                            buildRangeInfos.push_back(buildRangeInfo);

                            GeometryNode geometryNode{};
                            geometryNode.vertexBufferDeviceAddress = vertexBufferDeviceAddress.deviceAddress;
                            geometryNode.indexBufferDeviceAddress = indexBufferDeviceAddress.deviceAddress;

                            Material material = m_MaterialsMap[primitive.materialIndex];
                            geometryNode.textureIndexBaseColor = material.BaseColorTextureID;
                            geometryNode.textureIndexOcclusion = material.OcclusionTextureID;

                            geometryNodes.push_back(geometryNode);
                        }
                    }
                }
            }
            for (auto& rangeInfo : buildRangeInfos) 
            {
                pBuildRangeInfos.push_back(&rangeInfo);
            }

            // TODO: NOT SURE CORRECT WAY TO UPDATE BUFFER OR NOT?
            m_GeometryNodes.insert(m_GeometryNodes.end(), geometryNodes.begin(), geometryNodes.end());
            if (m_GeometryNodesBuffer.buffer != VK_NULL_HANDLE)
            {
                m_GeometryNodesBuffer.Destroy();
            }
            Utility::CreateBuffer(*this,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &m_GeometryNodesBuffer, static_cast<uint32_t>(m_GeometryNodes.size()) * sizeof(GeometryNode),
                m_GeometryNodes.data());

            // Get size info
            VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
            accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            accelerationStructureBuildGeometryInfo.geometryCount = static_cast<uint32_t>(geometries.size());
            accelerationStructureBuildGeometryInfo.pGeometries = geometries.data();

            const uint32_t numTriangles = maxPrimitiveCounts[0];

            VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
            accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            vkGetAccelerationStructureBuildSizesKHR(
                m_Device,
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &accelerationStructureBuildGeometryInfo,
                maxPrimitiveCounts.data(),
                &accelerationStructureBuildSizesInfo);

            CreateAccelerationStructureBuffer(blas, accelerationStructureBuildSizesInfo);

            VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
            accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            accelerationStructureCreateInfo.buffer = blas.buffer;
            accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
            accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            vkCreateAccelerationStructureKHR(m_Device, &accelerationStructureCreateInfo, nullptr, &blas.handle);

            // Create a small scratch buffer used during build of the bottom level acceleration structure
            ScratchBuffer scratchBuffer = CreateScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

            accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            accelerationStructureBuildGeometryInfo.dstAccelerationStructure = blas.handle;
            accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

            const VkAccelerationStructureBuildRangeInfoKHR* buildOffsetInfo = buildRangeInfos.data();

            // Build the acceleration structure on the device via a one-time command buffer submission
            // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
            VkCommandBuffer commandBuffer = Utility::BeginSingleTimeCommands(*this, m_CommandPool);
            vkCmdBuildAccelerationStructuresKHR(
                commandBuffer,
                1,
                &accelerationStructureBuildGeometryInfo,
                pBuildRangeInfos.data());
            Utility::EndSingleTimeCommands(*this, m_CommandPool, commandBuffer);

            VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
            accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            accelerationDeviceAddressInfo.accelerationStructure = blas.handle;
            blas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(m_Device, &accelerationDeviceAddressInfo);

            DeleteScratchBuffer(scratchBuffer);

            m_BottomLevelAS.push_back(blas);
            return blas;
        }
        void VKRenderer::CreateTopLevelAccelerationStructure()
        {
            VkTransformMatrixKHR transformMatrix = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f 
            };

            size_t BLAS_Count = m_BottomLevelAS.size();
            std::vector<VkAccelerationStructureInstanceKHR> instances(BLAS_Count);
            for (size_t i = 0; i < BLAS_Count; i++)
            {
                instances[i].transform = transformMatrix;
                instances[i].instanceCustomIndex = m_BLAS_GeometryNodeOffsets[i];
                instances[i].mask = 0xFF;
                instances[i].instanceShaderBindingTableRecordOffset = 0;
                instances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                instances[i].accelerationStructureReference = m_BottomLevelAS[i].deviceAddress;
            }

            Buffer instancesBuffer{};
            // Buffer for instance data
            Utility::CreateBuffer(*this,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &instancesBuffer,
                BLAS_Count * sizeof(VkAccelerationStructureInstanceKHR),
                instances.data());

            VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{};
            instanceDataDeviceAddress.deviceAddress = GetBufferDeviceAddress(instancesBuffer.buffer);

            VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
            accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
            accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
            accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
            accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
            accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

            // Get size info
            /*
                The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored. Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command, except that the hostAddress member of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
            */
            VkAccelerationStructureBuildGeometryInfoKHR acclerationStructureBuildGeometryInfo{};
            acclerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            acclerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            acclerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            acclerationStructureBuildGeometryInfo.geometryCount = 1;
            acclerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

            uint32_t primitiveCount = BLAS_Count;

            VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
            accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            vkGetAccelerationStructureBuildSizesKHR(m_Device,
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &acclerationStructureBuildGeometryInfo,
                &primitiveCount,
                &accelerationStructureBuildSizesInfo);

            CreateAccelerationStructureBuffer(m_TopLevelAS, accelerationStructureBuildSizesInfo);

            VkAccelerationStructureCreateInfoKHR acclerationStructureCreateInfo{};
            acclerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            acclerationStructureCreateInfo.buffer = m_TopLevelAS.buffer;
            acclerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
            acclerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            vkCreateAccelerationStructureKHR(m_Device, &acclerationStructureCreateInfo, nullptr, &m_TopLevelAS.handle);

            // Create a small scratch buffer used during build of the top level acceleration structure
            ScratchBuffer scratchBuffer = CreateScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

            VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
            accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            accelerationBuildGeometryInfo.dstAccelerationStructure = m_TopLevelAS.handle;
            accelerationBuildGeometryInfo.geometryCount = 1;
            accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
            accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

            VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
            accelerationStructureBuildRangeInfo.primitiveCount = BLAS_Count;
            accelerationStructureBuildRangeInfo.primitiveOffset = 0;
            accelerationStructureBuildRangeInfo.firstVertex = 0;
            accelerationStructureBuildRangeInfo.transformOffset = 0;
            std::vector<VkAccelerationStructureBuildRangeInfoKHR*> acclerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };
            
            // Build the acceleration structure on the device via a one-time command buffer submission
            // Some implementations may support acceleration structure building on the host (VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructureHostCommands), but we prefer device builds
            VkCommandBuffer commandBuffer = Utility::BeginSingleTimeCommands(*this, m_CommandPool);
            vkCmdBuildAccelerationStructuresKHR(
                commandBuffer,
                1,
                &accelerationBuildGeometryInfo,
                acclerationBuildStructureRangeInfos.data());
            Utility::EndSingleTimeCommands(*this, m_CommandPool, commandBuffer);

            VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
            accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            accelerationDeviceAddressInfo.accelerationStructure = m_TopLevelAS.handle;

            DeleteScratchBuffer(scratchBuffer);
            instancesBuffer.Destroy();
        }

        void VKRenderer::CreateShaderBindingTable(ShaderBindingTable& shaderBindingTable, uint32_t handleCount)
        {
            // Create buffer to hold all shader handles for the SBT
            Utility::CreateBuffer(*this,
                VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &shaderBindingTable,
                m_RayTracingPipelineProperties.shaderGroupHandleSize * handleCount);
            // Get the strided address to be used when dispatching the rays
            shaderBindingTable.stridedDeviceAddressRegion = GetSbtEntryStridedDeviceAddressRegion(shaderBindingTable.buffer, handleCount);
            // Map persistent
            shaderBindingTable.Map();
        }
        void VKRenderer::CreateShaderBindingTables()
        {
            const uint32_t handleSize = m_RayTracingPipelineProperties.shaderGroupHandleSize;
            const uint32_t handleSizeAligned = Utility::AlignedSize(m_RayTracingPipelineProperties.shaderGroupHandleSize, m_RayTracingPipelineProperties.shaderGroupHandleAlignment);
            const uint32_t groupCount = static_cast<uint32_t>(m_ShaderGroups.size());
            const uint32_t sbtSize = groupCount * handleSizeAligned;

            std::vector<uint8_t> shaderHandleStorage(sbtSize);
            vkGetRayTracingShaderGroupHandlesKHR(m_Device, m_RtPipeline, 0, groupCount, sbtSize, shaderHandleStorage.data());

            CreateShaderBindingTable(m_ShaderBindingTables.raygen, 1);
            CreateShaderBindingTable(m_ShaderBindingTables.miss, 2);    // handleCount = 2 : Handle miss(1) + shadow(2)
            CreateShaderBindingTable(m_ShaderBindingTables.hit, 1);

            // Copy handles
            memcpy(m_ShaderBindingTables.raygen.mapped, shaderHandleStorage.data(), handleSize);
            // We are using two miss shaders, so we need to get two handles for the miss shader binding table
            memcpy(m_ShaderBindingTables.miss.mapped, shaderHandleStorage.data() + handleSizeAligned, handleSize * 2);
            memcpy(m_ShaderBindingTables.hit.mapped, shaderHandleStorage.data() + handleSizeAligned * 3, handleSize);
        }
        
        void VKRenderer::CreateRayTracingPipeline()
        {
            // [DONE] TODO: Added TextureCount on model
            uint32_t imageCount = static_cast<uint32_t>(m_Textures.size());

            std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
            {
                // Binding 0: Top level acceleration structure
                VkDescriptorSetLayoutBinding{ 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr },
                // Binding 1: Ray tracing result image
                VkDescriptorSetLayoutBinding{ 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr },
                // Binding 2: Uniform buffer
                VkDescriptorSetLayoutBinding{ 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR, nullptr },
                // Binding 3: Texture image
                VkDescriptorSetLayoutBinding{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
                // Binding 4: Geometry node information SSBO
                VkDescriptorSetLayoutBinding{ 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
                // [DONE] TODO: Binding 5: All images used by the glTF model
                VkDescriptorSetLayoutBinding{ 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, nullptr },
            };

            // Unbound Set
            VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setLayoutBindingFlags{};
            setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
            setLayoutBindingFlags.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
            std::vector<VkDescriptorBindingFlagsEXT> descriptorBindingFlags =
            {
                0,
                0,
                0,
                0,
                0,
                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
            };
            setLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();

            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
            descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.pBindings = setLayoutBindings.data();
            descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
            descriptorSetLayoutCreateInfo.pNext = &setLayoutBindingFlags;
            vkCreateDescriptorSetLayout(m_Device, &descriptorSetLayoutCreateInfo, nullptr, &m_RtDescriptorSetLayout);

            VkPushConstantRange pushConstant{};
            pushConstant.offset = 0;
            pushConstant.size = sizeof(PushConstantRay);
            pushConstant.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
            pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutCreateInfo.pSetLayouts = &m_RtDescriptorSetLayout;
            pipelineLayoutCreateInfo.setLayoutCount = 1;
            pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
            pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
            vkCreatePipelineLayout(m_Device, &pipelineLayoutCreateInfo, nullptr, &m_RtPipelineLayout);

            /*
                Setup ray tracing shader groups
            */
            std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
            // Ray generation group
            {
                shaderStages.push_back(Shader::LoadShader(*this, "assets/shaders/raygen.rgen"));
                VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
                shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
                shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
                shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
                shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
                m_ShaderGroups.push_back(shaderGroup);
            }
            // Miss group
            {
                shaderStages.push_back(Shader::LoadShader(*this, "assets/shaders/miss.rmiss"));
                VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
                shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
                shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
                shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
                shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
                m_ShaderGroups.push_back(shaderGroup);

                // Second shader for shadows
                shaderStages.push_back(Shader::LoadShader(*this, "assets/shaders/shadow.rmiss"));
                shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
                m_ShaderGroups.push_back(shaderGroup);
            }
            // Closest hit group for doing texture lookups
            {
                shaderStages.push_back(Shader::LoadShader(*this, "assets/shaders/closesthit.rchit"));
                VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
                shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
                shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
                shaderGroup.closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
                shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

                // This group also uses an anyhit shader for doing transparency (see anyhit.rahit for details)
                shaderStages.push_back(Shader::LoadShader(*this, "assets/shaders/anyhit.rahit"));
                shaderGroup.anyHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
                m_ShaderGroups.push_back(shaderGroup);
            }

            /*
                Create the ray tracing pipeline
            */
            VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
            rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
            rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
            rayTracingPipelineCI.pStages = shaderStages.data();
            rayTracingPipelineCI.groupCount = static_cast<uint32_t>(m_ShaderGroups.size());
            rayTracingPipelineCI.pGroups = m_ShaderGroups.data();
            rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
            rayTracingPipelineCI.layout = m_RtPipelineLayout;
            vkCreateRayTracingPipelinesKHR(m_Device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &m_RtPipeline);
        }
        void VKRenderer::CreateRayTracingDescriptorSets()
        {
            // [DONE] TODO: Load Texture
            uint32_t imageCount = static_cast<uint32_t>(m_Textures.size());
            std::vector<VkDescriptorPoolSize> poolSizes = {
                { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount }
            };

            VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
            descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
            descriptorPoolCreateInfo.maxSets = 1;
            vkCreateDescriptorPool(m_Device, &descriptorPoolCreateInfo, nullptr, &m_RtDescriptorPool);

            VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableDescriptorCountAllocInfo{};
            uint32_t variableDescCounts[] = { imageCount };
            variableDescriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
            variableDescriptorCountAllocInfo.descriptorSetCount = 1;
            variableDescriptorCountAllocInfo.pDescriptorCounts = variableDescCounts;

            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
            descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocateInfo.descriptorPool = m_RtDescriptorPool;
            descriptorSetAllocateInfo.pSetLayouts = &m_RtDescriptorSetLayout;
            descriptorSetAllocateInfo.descriptorSetCount = 1;
            descriptorSetAllocateInfo.pNext = &variableDescriptorCountAllocInfo;
            vkAllocateDescriptorSets(m_Device, &descriptorSetAllocateInfo, &m_RtDescriptorSet);

            VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo{};
            descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
            descriptorAccelerationStructureInfo.pAccelerationStructures = &m_TopLevelAS.handle;

            VkWriteDescriptorSet accelerationStructureWrite{};
            accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            // The specialized acceleration structure descriptor has to be chained
            accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
            accelerationStructureWrite.dstSet = m_RtDescriptorSet;
            accelerationStructureWrite.dstBinding = 0;
            accelerationStructureWrite.descriptorCount = 1;
            accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

            VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, m_StorageImageView, VK_IMAGE_LAYOUT_GENERAL };

            std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
                // Binding 0: Top level acceleration structure
                accelerationStructureWrite,
                // Binding 1: Ray tracing result image
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = m_RtDescriptorSet,
                    .dstBinding = 1,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .pImageInfo = &storageImageDescriptor
                },
                // Binding 2: Uniform data
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = m_RtDescriptorSet,
                    .dstBinding = 2,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &m_RtUniformBuffer.descriptor
                },
                // Binding 4: Geometry node information SSBO
                VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = m_RtDescriptorSet,
                    .dstBinding = 4,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                    .pBufferInfo = &m_GeometryNodesBuffer.descriptor
                },
            };

            // Image descriptors for the image array
            std::vector<VkDescriptorImageInfo> textureDescriptors{};
            // [DONE] TODO: Load Model Texture
            for (auto& texture : m_Textures) {
                VkDescriptorImageInfo descriptor{};
                descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                descriptor.sampler = texture->GetSampler();
                descriptor.imageView = texture->GetImageView();
                textureDescriptors.push_back(descriptor);
            }

            VkWriteDescriptorSet writeDescriptorImgArray{};
            writeDescriptorImgArray.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeDescriptorImgArray.dstBinding = 5;
            writeDescriptorImgArray.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeDescriptorImgArray.descriptorCount = imageCount;
            writeDescriptorImgArray.dstSet = m_RtDescriptorSet;
            writeDescriptorImgArray.pImageInfo = textureDescriptors.data();
            writeDescriptorSets.push_back(writeDescriptorImgArray);

            vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
        }
        void VKRenderer::CreateRayTracingUniformBuffer()
        {
            Utility::CreateBuffer(*this,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                &m_RtUniformBuffer,
                sizeof(UniformBufferObject),
                &uniformData);

            m_RtUniformBuffer.Map();

            // TODO: Do I need to update at the start?
            //updateUniformBuffers();
        }

        void VKRenderer::UpdateRayTracingUniformBuffer()
        {
            
            glm::mat4 view = m_Camera.GetViewMatrix();
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), m_SwapChainExtent.width / (float)m_SwapChainExtent.height, 0.1f, 10.0f);
            proj[1][1] *= -1;

            uniformData.projInverse = glm::inverse(proj);
            uniformData.viewInverse = glm::inverse(view);
            // This value is used to accumulate multiple frames into the finale picture
            // It's required as ray tracing needs to do multiple passes for transparency
            // In this sample we use noise offset by this frame index to shoot rays for transparency into different directions
            // Once enough frames with random ray directions have been accumulated, it looks like proper transparency
            uniformData.frame++;
            memcpy(m_RtUniformBuffer.mapped, &uniformData, sizeof(uniformData));
        }

        uint64_t VKRenderer::GetBufferDeviceAddress(VkBuffer buffer)
        {
            VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
            bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAI.buffer = buffer;
            return vkGetBufferDeviceAddressKHR(m_Device, &bufferDeviceAI);
        }
        VkStridedDeviceAddressRegionKHR VKRenderer::GetSbtEntryStridedDeviceAddressRegion(VkBuffer buffer, uint32_t handleCount)
        {
            const uint32_t handleSizeAligned = Utility::AlignedSize(m_RayTracingPipelineProperties.shaderGroupHandleSize, m_RayTracingPipelineProperties.shaderGroupHandleAlignment);
            VkStridedDeviceAddressRegionKHR stridedDeviceAddressRegionKHR{};
            stridedDeviceAddressRegionKHR.deviceAddress = GetBufferDeviceAddress(buffer);
            stridedDeviceAddressRegionKHR.stride = handleSizeAligned;
            stridedDeviceAddressRegionKHR.size = handleCount * handleSizeAligned;
            return stridedDeviceAddressRegionKHR;
        }

        void VKRenderer::CreateStorageImage()
        {
            // Release resources if image is to be recreated
            if (m_StorageImage != VK_NULL_HANDLE) 
            {
                vkDestroyImageView(m_Device, m_StorageImageView, nullptr);
                vkDestroyImage(m_Device, m_StorageImage, nullptr);
                vkFreeMemory(m_Device, m_StorageImageMemory, nullptr);

                m_StorageImage = VK_NULL_HANDLE;
                m_StorageImageView = VK_NULL_HANDLE;
                m_StorageImageMemory = VK_NULL_HANDLE;
            }

            CreateImage(m_SwapChainExtent.width, m_SwapChainExtent.height,
                m_SwapChainImageFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                m_StorageImage, m_StorageImageMemory);

            m_StorageImageView = CreateImageView(m_StorageImage, m_SwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

            Utility::TransitionImageLayout(*this, m_CommandPool, m_StorageImage, m_SwapChainImageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        }
        void VKRenderer::DestroyStorageImage()
        {
            vkDestroyImageView(m_Device, m_StorageImageView, nullptr);
            vkDestroyImage(m_Device, m_StorageImage, nullptr);
            vkFreeMemory(m_Device, m_StorageImageMemory, nullptr);
        }
        void VKRenderer::ReCreateStorageImage()
        {
            CreateStorageImage();

            VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, m_StorageImageView, VK_IMAGE_LAYOUT_GENERAL };
            VkWriteDescriptorSet resultImageWrite{};
            resultImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            resultImageWrite.dstSet = m_RtDescriptorSet;
            resultImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            resultImageWrite.dstBinding = 1;
            resultImageWrite.pImageInfo = &storageImageDescriptor;
            resultImageWrite.descriptorCount = 1;

            vkUpdateDescriptorSets(m_Device, 1, &resultImageWrite, 0, VK_NULL_HANDLE);
        }

        // --------------------------------------------------------------------------------------------------------------------------
        //                                                     ImGui Stuff
        // --------------------------------------------------------------------------------------------------------------------------
        void VKRenderer::InitImGui()
        {	
            // Create Descriptor Pool for ImGui
            VkDescriptorPoolSize pool_sizes[] =
            {
                { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
            };

            VkDescriptorPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            pool_info.maxSets = 1000;
            pool_info.poolSizeCount = std::size(pool_sizes);
            pool_info.pPoolSizes = pool_sizes;

            VkDescriptorPool imguiPool;
            vkCreateDescriptorPool(m_Device, &pool_info, nullptr, &imguiPool);

            // Initialize ImGui library
            ImGui::CreateContext();
            ImGui_ImplGlfw_InitForVulkan(m_Window, true);

            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.Instance = m_Instance;
            init_info.PhysicalDevice = m_PhysicalDevice;
            init_info.Device = m_Device;
            init_info.Queue = m_GraphicsQueue;
            init_info.DescriptorPool = imguiPool;
            init_info.RenderPass = m_RenderPass;
            init_info.MinImageCount = 3;
            init_info.ImageCount = 3;
            init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

            ImGui_ImplVulkan_Init(&init_info);
            ImGui_ImplVulkan_CreateFontsTexture();
        }
        void VKRenderer::RenderImGui(size_t imageIndex)
        {
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_RenderPass;
            renderPassInfo.framebuffer = m_SwapChainFramebuffers[imageIndex];
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = m_SwapChainExtent;

            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = { {1.0f, 1.0f, 1.0f, 1.0f} };
            clearValues[1].depthStencil = { 1.0f, 0 };
            
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(m_CommandBuffers[m_CurrentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Render ImGui
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();

            ImGui::NewFrame();

            ImGui::ShowDemoWindow();

            ImGui::Render();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_CommandBuffers[m_CurrentFrame]);

            vkCmdEndRenderPass(m_CommandBuffers[m_CurrentFrame]);

        }
	}
}