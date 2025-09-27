#include "DenoiserHelper.h"

namespace MilkShake
{
    namespace Graphics
    {
        uint32_t FindMemoryType(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags wanted)
        {
            VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(phys, &mp);
            for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
                if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & wanted) == wanted) return i;
            assert(false); return 0;
        }

        ExportableBuffer MakeExportableBuffer(VkDevice dev, VkPhysicalDevice phys, VkDeviceSize size, VkBufferUsageFlags usage)
        {
            ExportableBuffer out{};
            out.size = size;

            VkExternalMemoryBufferCreateInfo extBuf{ VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO };
            extBuf.handleTypes = kMemHandle;

            VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            bci.size = size;
            bci.usage = usage;
            bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            bci.pNext = &extBuf;
            VK_CHECK(vkCreateBuffer(dev, &bci, nullptr, &out.buf));

            VkMemoryRequirements req{}; vkGetBufferMemoryRequirements(dev, out.buf, &req);

            VkExportMemoryAllocateInfo exportAlloc{ VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO };
            exportAlloc.handleTypes = kMemHandle;

            VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
            mai.allocationSize = req.size;
            mai.memoryTypeIndex = FindMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            mai.pNext = &exportAlloc;

            VK_CHECK(vkAllocateMemory(dev, &mai, nullptr, &out.mem));
            VK_CHECK(vkBindBufferMemory(dev, out.buf, out.mem, 0));

            #ifdef _WIN32
                // Get WIN32 handle
                auto pGetHandle = (PFN_vkGetMemoryWin32HandleKHR)vkGetDeviceProcAddr(dev, "vkGetMemoryWin32HandleKHR");
                VkMemoryGetWin32HandleInfoKHR gi{ VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
                gi.memory = out.mem;
                gi.handleType = kMemHandle;
                VK_CHECK(pGetHandle(dev, &gi, &out.handle));
            #else
                // Get FD
                auto pGetFd = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(dev, "vkGetMemoryFdKHR");
                VkMemoryGetFdInfoKHR gi{ VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR };
                gi.memory = out.mem;
                gi.handleType = kMemHandle;
                VK_CHECK(pGetFd(dev, &gi, &out.fd));
            #endif

            return out;
        }
        void DestroyExportableBuffer(VkDevice dev, ExportableBuffer buf)
        {
            vkDestroyBuffer(dev, buf.buf, nullptr);
            vkFreeMemory(dev, buf.mem, nullptr);
        }

        ExportableSemaphore MakeExportableSemaphore(VkDevice dev)
        {
            ExportableSemaphore out{};

            VkExportSemaphoreCreateInfo exportSem{ VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO };
            exportSem.handleTypes = kSemHandle;

            VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
            sci.pNext = &exportSem;
            VK_CHECK(vkCreateSemaphore(dev, &sci, nullptr, &out.sem));

            #ifdef _WIN32
                VkSemaphoreGetWin32HandleInfoKHR gi{ VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR };
                gi.semaphore = out.sem;
                gi.handleType = kSemHandle;
                auto vkGetSemaphoreWin32HandleKHR = (PFN_vkGetSemaphoreWin32HandleKHR)vkGetDeviceProcAddr(dev, "vkGetSemaphoreWin32HandleKHR");
                VK_CHECK(vkGetSemaphoreWin32HandleKHR(dev, &gi, &out.handle));
            #else
                VkSemaphoreGetFdInfoKHR gi{ VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR };
                gi.semaphore = out.sem;
                gi.handleType = kSemHandle;
                auto vkGetSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR)vkGetDeviceProcAddr(dev, "vkGetSemaphoreFdKHR");
                VK_CHECK(vkGetSemaphoreFdKHR(dev, &gi, &out.fd));
            #endif

            return out;
        }
        void DestroyExportableSemaphore(VkDevice dev, ExportableSemaphore semaphore)
        {
            vkDestroySemaphore(dev, semaphore.sem, nullptr);
        }

        // ------------------------------- CUDA imports
        CudaExtMem ImportCudaFromVulkanBuffer(const ExportableBuffer& b)
        {
            CudaExtMem out{}; out.size = size_t(b.size);

            cudaExternalMemoryHandleDesc md{}; md.size = out.size;
            #ifdef _WIN32
                md.type = cudaExternalMemoryHandleTypeOpaqueWin32; md.handle.win32.handle = b.handle;
            #else
                md.type = cudaExternalMemoryHandleTypeOpaqueFd; md.handle.fd = b.fd;
            #endif
            CUDA_CHECK(cudaImportExternalMemory(&out.ext, &md));

            cudaExternalMemoryBufferDesc bd{}; bd.offset = 0; bd.size = out.size;
            CUDA_CHECK(cudaExternalMemoryGetMappedBuffer(&out.devPtr, out.ext, &bd));
            return out;
        }

        cudaExternalSemaphore_t ImportCudaSemaphore(const ExportableSemaphore& s)
        {
            cudaExternalSemaphore_t out{};
            cudaExternalSemaphoreHandleDesc hd{};
            #ifdef _WIN32
                hd.type = cudaExternalSemaphoreHandleTypeOpaqueWin32; hd.handle.win32.handle = s.handle;
            #else
                hd.type = cudaExternalSemaphoreHandleTypeOpaqueFd; hd.handle.fd = s.fd;
            #endif
            CUDA_CHECK(cudaImportExternalSemaphore(&out, &hd));
            return out;
        }

        // ------------------------------- OptiX Denoiser wrapper
        void InitOptixDenoiser(OptixDenoiserCtx& O)
        {
            OPTIX_CHECK(optixInit());
            // Ensure a CUDA context exists
            CUDA_CHECK(cudaSetDevice(0));
            CUcontext cu; CU_CHECK(cuCtxGetCurrent(&cu));

            OptixDeviceContextOptions opts{};
            OPTIX_CHECK(optixDeviceContextCreate(cu, &opts, &O.ctx));

            OptixDenoiserOptions dopt{};
            dopt.guideAlbedo = 1;
            dopt.guideNormal = 1;

            OPTIX_CHECK(optixDenoiserCreate(O.ctx, OPTIX_DENOISER_MODEL_KIND_HDR, &dopt, &O.den));
        }

        OptixImage2D MakeImage2D(void* devPtrFloat4, int W, int H)
        {
            OptixImage2D img{};
            img.data = reinterpret_cast<CUdeviceptr>(devPtrFloat4);
            img.width = W;
            img.height = H;
            img.rowStrideInBytes = size_t(W) * 4 * sizeof(float); // VkImage must be R32G32B32A32_SFLOAT
            img.pixelStrideInBytes = 4 * sizeof(float);
            img.format = OPTIX_PIXEL_FORMAT_FLOAT4;
            return img;
        }

        // Runs denoiser: beauty in/out, albedo & normal as a guide
        void RunOptixDenoiser(OptixDenoiserCtx& O,
            void* beautyDevPtr, void* albedoDevPtr, void* normalDevPtr, void* outDevPtr,
            cudaExternalSemaphore_t cuWait, cudaExternalSemaphore_t cuSignal)
        {
            // Wait for Vulkan to finish writing buffers
            cudaExternalSemaphoreWaitParams w{};
            CUDA_CHECK(cudaWaitExternalSemaphoresAsync(&cuWait, &w, 1, 0 /*stream*/));

            OptixImage2D beauty = MakeImage2D(beautyDevPtr, O.W, O.H);
            OptixImage2D albedo = MakeImage2D(albedoDevPtr, O.W, O.H);
            OptixImage2D normal = MakeImage2D(normalDevPtr, O.W, O.H);
            OptixImage2D out = MakeImage2D(outDevPtr, O.W, O.H); // in-place

            OptixDenoiserGuideLayer guides{};
            guides.albedo = albedo;
            guides.normal = normal;

            OptixDenoiserLayer layer{};
            layer.input = beauty;
            layer.output = out;

            OptixDenoiserParams p{};
            p.blendFactor = 0.0f; // fully denoised

            OPTIX_CHECK(optixDenoiserInvoke(O.den, 0/*stream*/, &p,
                O.d_state, O.stateSize,
                &guides, &layer, 1,
                0, 0, O.d_scratch, O.scratchSize));

            // Signal Vulkan that output buffer is ready
            cudaExternalSemaphoreSignalParams s{};
            CUDA_CHECK(cudaSignalExternalSemaphoresAsync(&cuSignal, &s, 1, 0 /*stream*/));
        }

        // ------------------------------- Per-frame orchestration (pseudocode-ish)
        void RecordCopyImageToBuffer(VkCommandBuffer cmd, VkImage src, VkBuffer dst, int W, int H)
        {
            // Transition src to TRANSFER_SRC (barriers omitted for brevity)
            VkBufferImageCopy r{};
            r.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            r.imageSubresource.mipLevel = 0;
            r.imageSubresource.baseArrayLayer = 0;
            r.imageSubresource.layerCount = 1;
            r.imageExtent = { (uint32_t)W, (uint32_t)H, 1 };
            vkCmdCopyImageToBuffer(cmd, src, VK_IMAGE_LAYOUT_GENERAL, dst, 1, &r);
        }

        void RecordCopyBufferToImage(VkCommandBuffer cmd, VkBuffer src, VkImage dst, int W, int H)
        {
            // Transition dst to TRANSFER_DST (barriers omitted for brevity)
            VkBufferImageCopy w{};
            w.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            w.imageSubresource.mipLevel = 0;
            w.imageSubresource.baseArrayLayer = 0;
            w.imageSubresource.layerCount = 1;
            w.imageExtent = { (uint32_t)W, (uint32_t)H, 1 };
            vkCmdCopyBufferToImage(cmd, src, dst, VK_IMAGE_LAYOUT_GENERAL, 1, &w);
        }

        void CreateOptiXDenoiser(DenoiseInterop& I, VkDevice dev, VkPhysicalDevice phys, VkQueue q, uint32_t qf,
            int W, int H)
        {
            I.device = dev; I.phys = phys; I.queue = q; I.queueFamily = qf; I.W = W; I.H = H;

            // Exportable buffers for color & normal (RGBA32F)
            VkDeviceSize bytes = VkDeviceSize(W) * H * 4 * sizeof(float);
            I.colorBuf = MakeExportableBuffer(dev, phys, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            I.albedoBuf = MakeExportableBuffer(dev, phys, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            I.normalBuf = MakeExportableBuffer(dev, phys, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            I.outBuf = MakeExportableBuffer(dev, phys, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

            // Exportable semaphores (vk->cuda, cuda->vk)
            I.vkToCuda = MakeExportableSemaphore(dev);
            I.cudaToVk = MakeExportableSemaphore(dev);

            // CUDA import memory + semaphores
            I.cuColor = ImportCudaFromVulkanBuffer(I.colorBuf);
            I.cuAlbedo = ImportCudaFromVulkanBuffer(I.albedoBuf);
            I.cuNormal = ImportCudaFromVulkanBuffer(I.normalBuf);
            I.cuOut = ImportCudaFromVulkanBuffer(I.outBuf);
            
            I.cuWait = ImportCudaSemaphore(I.vkToCuda);
            I.cuSignal = ImportCudaSemaphore(I.cudaToVk);

            OptixDenoiserSizes sizes{};
            OPTIX_CHECK(optixDenoiserComputeMemoryResources(I.optix.den, W, H, &sizes));
            I.optix.stateSize = sizes.stateSizeInBytes;
            I.optix.scratchSize = sizes.withoutOverlapScratchSizeInBytes;
            I.optix.W = W; I.optix.H = H;

            CU_CHECK(cuMemAlloc(&I.optix.d_state, I.optix.stateSize));
            CU_CHECK(cuMemAlloc(&I.optix.d_scratch, I.optix.scratchSize));

            OPTIX_CHECK(optixDenoiserSetup(I.optix.den, 0/*stream*/, W, H, I.optix.d_state, I.optix.stateSize, I.optix.d_scratch, I.optix.scratchSize));
        }
        void DestroyOptiXDenoiser(DenoiseInterop& I)
        {
            CU_CHECK(cuMemFree(I.optix.d_state));
            CU_CHECK(cuMemFree(I.optix.d_scratch));

            DestroyExportableBuffer(I.device, I.colorBuf);
            DestroyExportableBuffer(I.device, I.albedoBuf);
            DestroyExportableBuffer(I.device, I.normalBuf);
            DestroyExportableBuffer(I.device, I.outBuf);

            DestroyExportableSemaphore(I.device, I.vkToCuda);
            DestroyExportableSemaphore(I.device, I.cudaToVk);
        }

        void DenoiseFrame(DenoiseInterop& I,
            VkImage noisyColorImage,
            VkImage albedoImage,   
            VkImage worldNormalImage,
            VkImage presentImage)    
        {
            // Copy images -> exportable buffers
            VkCommandBufferAllocateInfo cai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            cai.commandPool = I.cmdPool; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
            VkCommandBuffer cmd; VK_CHECK(vkAllocateCommandBuffers(I.device, &cai, &cmd));

            VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
            // (insert appropriate barriers to TRANSFER_SRC/TRANSFER_DST)
            RecordCopyImageToBuffer(cmd, noisyColorImage, I.colorBuf.buf, I.W, I.H);
            RecordCopyImageToBuffer(cmd, albedoImage, I.albedoBuf.buf, I.W, I.H);
            RecordCopyImageToBuffer(cmd, worldNormalImage, I.normalBuf.buf, I.W, I.H);
            VK_CHECK(vkEndCommandBuffer(cmd));

            VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
            si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
            // Signal vk->cuda semaphore when copies finish
            si.signalSemaphoreCount = 1; VkSemaphore sigSems[] = { I.vkToCuda.sem }; si.pSignalSemaphores = sigSems;
            VK_CHECK(vkQueueSubmit(I.queue, 1, &si, VK_NULL_HANDLE));
            VK_CHECK(vkQueueWaitIdle(I.queue)); // (or use proper waits on next submit)

            // CUDA/OptiX: wait, denoise, signal
            RunOptixDenoiser(I.optix, I.cuColor.devPtr, I.cuAlbedo.devPtr, I.cuNormal.devPtr, I.cuOut.devPtr, I.cuWait, I.cuSignal);

            // Wait on cuda->vk, copy buffer -> present image
            VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            VkSubmitInfo si2{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
            si2.waitSemaphoreCount = 1; VkSemaphore waitSems[] = { I.cudaToVk.sem }; si2.pWaitSemaphores = waitSems; si2.pWaitDstStageMask = &waitStage;

            VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
            RecordCopyBufferToImage(cmd, I.outBuf.buf, presentImage, I.W, I.H);
            VK_CHECK(vkEndCommandBuffer(cmd));
            si2.commandBufferCount = 1; si2.pCommandBuffers = &cmd;
            VK_CHECK(vkQueueSubmit(I.queue, 1, &si2, VK_NULL_HANDLE));
            VK_CHECK(vkQueueWaitIdle(I.queue)); // (in your engine, chain with present instead)
            vkFreeCommandBuffers(I.device, I.cmdPool, 1, &cmd);
        }
    }
}