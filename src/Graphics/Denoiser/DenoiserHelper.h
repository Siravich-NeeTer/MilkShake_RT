#pragma once

#define NOMINMAX
#ifdef _WIN32
    #define VK_USE_PLATFORM_WIN32_KHR 1
    #include <windows.h>              // for HANDLE
#endif
#include <vulkan/vulkan.h>
#ifdef _WIN32
    #include <vulkan/vulkan_win32.h>  // for PFN_vkGetMemoryWin32HandleKHR
#endif

#include <cuda.h>
#include <cuda_runtime_api.h>
#include <optix.h>
#include <optix_stubs.h>

#include <cassert>
#include <vector>
#include <cstring>
#include <cstdio>

// ------------------------------- Error helpers
#define VK_CHECK(x) do { VkResult err = (x); if (err) { fprintf(stderr, "VK err %d at %s:%d\n", err, __FILE__, __LINE__); std::abort(); } } while(0)
#define CU_CHECK(x) do { CUresult err = (x); if (err) { const char* s=nullptr; cuGetErrorString(err,&s); fprintf(stderr, "CU err %d (%s) at %s:%d\n", err, s?s:"?", __FILE__, __LINE__); std::abort(); } } while(0)
#define CUDA_CHECK(x) do { cudaError_t err = (x); if (err) { fprintf(stderr, "CUDA err %d (%s) at %s:%d\n", err, cudaGetErrorString(err), __FILE__, __LINE__); std::abort(); } } while(0)
#define OPTIX_CHECK(x) do { OptixResult r = (x); if (r != OPTIX_SUCCESS) { fprintf(stderr, "OptiX err %d at %s:%d\n", r, __FILE__, __LINE__); std::abort(); } } while(0)


namespace MilkShake
{
	namespace Graphics
	{
        #ifdef _WIN32
            static const VkExternalMemoryHandleTypeFlagBits     kMemHandle = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
            static const VkExternalSemaphoreHandleTypeFlagBits  kSemHandle = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
        #else
            static const VkExternalMemoryHandleTypeFlagBits     kMemHandle = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
            static const VkExternalSemaphoreHandleTypeFlagBits  kSemHandle = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        #endif
		
        uint32_t FindMemoryType(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags wanted);
        struct ExportableBuffer {
            VkBuffer        buf{};
            VkDeviceMemory  mem{};
            VkDeviceSize    size{};
            #ifdef _WIN32
                HANDLE          handle{};
            #else
                int             fd{ -1 };
            #endif
        };
        ExportableBuffer MakeExportableBuffer(VkDevice dev, VkPhysicalDevice phys, VkDeviceSize size, VkBufferUsageFlags usage);
        void DestroyExportableBuffer(VkDevice dev, ExportableBuffer buf);

        struct ExportableSemaphore {
            VkSemaphore sem{};
            #ifdef _WIN32
                HANDLE      handle{};
            #else
                int         fd{ -1 };
            #endif
        };
        ExportableSemaphore MakeExportableSemaphore(VkDevice dev);
        void DestroyExportableSemaphore(VkDevice dev, ExportableSemaphore semaphore);

        // ------------------------------- CUDA imports
        struct CudaExtMem {
            cudaExternalMemory_t ext{};
            void* devPtr{};
            size_t size{};
        };

        CudaExtMem ImportCudaFromVulkanBuffer(const ExportableBuffer& b);
        cudaExternalSemaphore_t ImportCudaSemaphore(const ExportableSemaphore& s);

        // ------------------------------- OptiX Denoiser wrapper (normal guide only)
        struct OptixDenoiserCtx {
            OptixDeviceContext ctx{};
            OptixDenoiser       den{};
            CUdeviceptr         d_state{};
            CUdeviceptr         d_scratch{};
            size_t              stateSize{}, scratchSize{};
            int                 W{}, H{};
        };

        void OptixInitDenoiserNormalOnly(OptixDenoiserCtx& O, int W, int H);
        OptixImage2D MakeImage2D(void* devPtrFloat4, int W, int H);

        // Runs denoiser: beauty in/out, albedo & normal as a guid
        void RunOptixDenoiser(OptixDenoiserCtx& O,
            void* beautyDevPtr, void* albedoDevPtr, void* normalDevPtr, void* outDevPtr,
            cudaExternalSemaphore_t cuWait, cudaExternalSemaphore_t cuSignal);

        // ------------------------------- Per-frame Denoise Operation
        struct DenoiseInterop {
            // Vulkan
            VkDevice device{};
            VkPhysicalDevice phys{};
            VkQueue  queue{};
            uint32_t queueFamily{};
            VkCommandPool cmdPool{};

            // Exportable buffers for color & normal
            ExportableBuffer colorBuf{};
            ExportableBuffer albedoBuf{};
            ExportableBuffer normalBuf{};
            ExportableBuffer outBuf{};

            // Exportable semaphores
            ExportableSemaphore vkToCuda{};
            ExportableSemaphore cudaToVk{};

            // CUDA views
            CudaExtMem cuColor{};
            CudaExtMem cuAlbedo{};
            CudaExtMem cuNormal{};
            CudaExtMem cuOut{};
            cudaExternalSemaphore_t cuWait{};
            cudaExternalSemaphore_t cuSignal{};

            // OptiX
            OptixDenoiserCtx optix{};
            int W{}, H{};
        };

        void RecordCopyImageToBuffer(VkCommandBuffer cmd, VkImage src, VkBuffer dst, int W, int H);
        void RecordCopyBufferToImage(VkCommandBuffer cmd, VkBuffer src, VkImage dst, int W, int H);
        void CreateDenoiser(DenoiseInterop& I, VkDevice dev, VkPhysicalDevice phys, VkQueue q, uint32_t qf, int W, int H);
        void DestroyDenoiser(DenoiseInterop& I);

        void DenoiseFrame(DenoiseInterop& I,
            VkImage noisyColorImage,
            VkImage albedoImage,
            VkImage worldNormalImage,
            VkImage presentImage);
	}
}