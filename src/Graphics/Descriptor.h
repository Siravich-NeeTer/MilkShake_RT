#pragma once

#include <stdio.h>
#include <string>
#include <vector>
#include <cassert>

#include <vulkan/vulkan.h>

namespace MilkShake
{
	namespace Graphics
	{
        class Descriptor
        {
            public:
                std::vector<VkDescriptorSetLayoutBinding> bindingTable;

                VkDescriptorSetLayout descSetLayout;
                VkDescriptorPool descPool;
                VkDescriptorSet descSet;    // Could be  vector<VkDescriptorSet> for multiple sets;

                void SetBindings(const VkDevice device, std::vector<VkDescriptorSetLayoutBinding> _bt);
                void Destroy(VkDevice device);

                // Any data can be written into a descriptor set.  Apparently I need only these few types:
                void Write(VkDevice& device, uint32_t index, const VkBuffer& buffer);
                void Write(VkDevice& device, uint32_t index, const VkDescriptorImageInfo& textureDesc);
                void Write(VkDevice& device, uint32_t index, const std::vector<VkDescriptorImageInfo>& textures);
                void Write(VkDevice& device, uint32_t index, const VkAccelerationStructureKHR& tlas);
        };
	}
}