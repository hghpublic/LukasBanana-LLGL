/*
 * VKPipelineLayout.cpp
 *
 * This file is part of the "LLGL" project (Copyright (c) 2015-2019 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "VKPipelineLayout.h"
#include "VKPoolSizeAccumulator.h"
#include "../VKTypes.h"
#include "../VKCore.h"
#include "../Texture/VKSampler.h"
#include "../../../Core/CoreUtils.h"
#include <LLGL/Misc/ForRange.h>
#include <LLGL/Container/SmallVector.h>
#include <algorithm>


namespace LLGL
{


VKPipelineLayout::VKPipelineLayout(VkDevice device, const PipelineLayoutDescriptor& desc) :
    pipelineLayout_       { device, vkDestroyPipelineLayout          },
    descriptorSetLayouts_ { { device, vkDestroyDescriptorSetLayout },
                            { device, vkDestroyDescriptorSetLayout },
                            { device, vkDestroyDescriptorSetLayout } },
    descriptorPool_       { device, vkDestroyDescriptorPool          },
    uniformDescs_         { desc.uniforms                            }
{
    /* Create Vulkan descriptor set layouts */
    if (!desc.heapBindings.empty())
        CreateBindingSetLayout(device, desc.heapBindings, heapBindings_, SetLayoutType_HeapBindings);
    if (!desc.bindings.empty())
        CreateBindingSetLayout(device, desc.bindings, bindings_, SetLayoutType_DynamicBindings);
    if (!desc.staticSamplers.empty())
        CreateImmutableSamplers(device, desc.staticSamplers);

    /* Create descriptor pool for dynamic descriptors and immutable samplers */
    if (!desc.bindings.empty() || !desc.staticSamplers.empty())
        CreateDescriptorPool(device);
    if (!desc.bindings.empty())
        CreateDescriptorCache(device, descriptorSetLayouts_[SetLayoutType_DynamicBindings]);
    if (!desc.staticSamplers.empty())
        CreateStaticDescriptorSet(device, descriptorSetLayouts_[SetLayoutType_ImmutableSamplers].Get());

    /* Don't create a VkPipelineLayout object if this instance only has push constants as those are part of the permutations for each PSO */
    if (!desc.heapBindings.empty() || !desc.bindings.empty() || !desc.staticSamplers.empty())
        pipelineLayout_ = CreateVkPipelineLayout(device);
}

std::uint32_t VKPipelineLayout::GetNumHeapBindings() const
{
    return static_cast<std::uint32_t>(heapBindings_.size());
}

std::uint32_t VKPipelineLayout::GetNumBindings() const
{
    return static_cast<std::uint32_t>(0); //TODO
}

std::uint32_t VKPipelineLayout::GetNumStaticSamplers() const
{
    return static_cast<std::uint32_t>(immutableSamplers_.size());
}

std::uint32_t VKPipelineLayout::GetNumUniforms() const
{
    return static_cast<std::uint32_t>(uniformDescs_.size());
}

void VKPipelineLayout::BindStaticDescriptorSet(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint) const
{
    if (staticDescriptorSet_ != VK_NULL_HANDLE)
    {
        vkCmdBindDescriptorSets(
            /*commandBuffer:*/      commandBuffer,
            /*pipelineBindPoint:*/  bindPoint,
            /*layout:*/             pipelineLayout_.Get(),
            /*firstSet:*/           descriptorSetBindSlots_[SetLayoutType_ImmutableSamplers],
            /*descriptorSetCount:*/ 1,
            /*pDescriptorSets:*/    &staticDescriptorSet_,
            /*dynamicOffsetCount:*/ 0,
            /*pDynamicOffsets*/     nullptr
        );
    }
}

void VKPipelineLayout::BindDynamicDescriptorSet(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint, VkDescriptorSet descriptorSet) const
{
    if (descriptorSet != VK_NULL_HANDLE)
    {
        vkCmdBindDescriptorSets(
            /*commandBuffer:*/      commandBuffer,
            /*pipelineBindPoint:*/  bindPoint,
            /*layout:*/             pipelineLayout_.Get(),
            /*firstSet:*/           descriptorSetBindSlots_[SetLayoutType_DynamicBindings],
            /*descriptorSetCount:*/ 1,
            /*pDescriptorSets:*/    &descriptorSet,
            /*dynamicOffsetCount:*/ 0,
            /*pDynamicOffsets*/     nullptr
        );
    }
}


/*
 * ======= Private: =======
 */

// Converts the bitmask of LLGL::StageFlags to VkShaderStageFlags
static VkShaderStageFlags GetVkShaderStageFlags(long flags)
{
    VkShaderStageFlags bitmask = 0;

    if ((flags & StageFlags::VertexStage        ) != 0) { bitmask |= VK_SHADER_STAGE_VERTEX_BIT;                  }
    if ((flags & StageFlags::TessControlStage   ) != 0) { bitmask |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;    }
    if ((flags & StageFlags::TessEvaluationStage) != 0) { bitmask |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; }
    if ((flags & StageFlags::GeometryStage      ) != 0) { bitmask |= VK_SHADER_STAGE_GEOMETRY_BIT;                }
    if ((flags & StageFlags::FragmentStage      ) != 0) { bitmask |= VK_SHADER_STAGE_FRAGMENT_BIT;                }
    if ((flags & StageFlags::ComputeStage       ) != 0) { bitmask |= VK_SHADER_STAGE_COMPUTE_BIT;                 }

    return bitmask;
}

// Returns the appropriate VkDescriptorType enum entry for the specified binding descriptor
static VkDescriptorType GetVkDescriptorType(const BindingDescriptor& desc)
{
    switch (desc.type)
    {
        case ResourceType::Sampler:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        case ResourceType::Texture:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case ResourceType::Buffer:
            if ((desc.bindFlags & BindFlags::ConstantBuffer) != 0)
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            if ((desc.bindFlags & (BindFlags::Sampled | BindFlags::Storage)) != 0)
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        default:
            break;
    }
    VKTypes::MapFailed("ResourceType", "VkDescriptorType");
}

void VKPipelineLayout::CreateVkDescriptorSetLayout(
    VkDevice                                        device,
    SetLayoutType                                   setLayoutType,
    const ArrayView<VkDescriptorSetLayoutBinding>&  setLayoutBindings)
{
    VkDescriptorSetLayoutCreateInfo createInfo;
    {
        createInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.pNext        = nullptr;
        createInfo.flags        = 0;
        createInfo.bindingCount = static_cast<std::uint32_t>(setLayoutBindings.size());
        createInfo.pBindings    = setLayoutBindings.data();
    }
    auto result = vkCreateDescriptorSetLayout(device, &createInfo, nullptr, descriptorSetLayouts_[setLayoutType].ReleaseAndGetAddressOf());
    VKThrowIfFailed(result, "failed to create Vulkan descriptor set layout");
}

static void Convert(VkDescriptorSetLayoutBinding& dst, const BindingDescriptor& src)
{
    dst.binding             = src.slot;
    dst.descriptorType      = GetVkDescriptorType(src);
    dst.descriptorCount     = std::max(1u, src.arraySize);
    dst.stageFlags          = GetVkShaderStageFlags(src.stageFlags);
    dst.pImmutableSamplers  = nullptr;
}

void VKPipelineLayout::CreateBindingSetLayout(
    VkDevice                                device,
    const std::vector<BindingDescriptor>&   inBindings,
    std::vector<VKLayoutBinding>&           outBindings,
    SetLayoutType                           setLayoutType)
{
    /* Convert heap bindings to native descriptor set layout bindings and create Vulkan descriptor set layout */
    const auto numBindings = inBindings.size();
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings(numBindings);

    for_range(i, numBindings)
        Convert(setLayoutBindings[i], inBindings[i]);

    CreateVkDescriptorSetLayout(device, setLayoutType, setLayoutBindings);

    /* Create list of binding points (for later pass to 'VkWriteDescriptorSet::dstBinding') */
    outBindings.reserve(numBindings);
    for_range(i, numBindings)
    {
        outBindings.push_back(
            VKLayoutBinding
            {
                inBindings[i].slot,
                inBindings[i].stageFlags,
                setLayoutBindings[i].descriptorType
            }
        );
    }
}

static void Convert(VkDescriptorSetLayoutBinding& dst, const StaticSamplerDescriptor& src, const VkSampler* immutableSamplerVK)
{
    dst.binding             = src.slot;
    dst.descriptorType      = VK_DESCRIPTOR_TYPE_SAMPLER;
    dst.descriptorCount     = 1u;
    dst.stageFlags          = GetVkShaderStageFlags(src.stageFlags);
    dst.pImmutableSamplers  = immutableSamplerVK;
}

void VKPipelineLayout::CreateImmutableSamplers(VkDevice device, const ArrayView<StaticSamplerDescriptor>& staticSamplers)
{
    /* Create all immutable Vulkan samplers */
    immutableSamplers_.reserve(staticSamplers.size());
    for (const auto& staticSamplerDesc : staticSamplers)
        immutableSamplers_.push_back(VKSampler::CreateVkSampler(device, staticSamplerDesc.sampler));

    /* Convert heap bindings to native descriptor set layout bindings and create Vulkan descriptor set layout */
    const auto numBindings = staticSamplers.size();
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings(numBindings);

    for_range(i, numBindings)
        Convert(setLayoutBindings[i], staticSamplers[i], immutableSamplers_[i].GetAddressOf());

    CreateVkDescriptorSetLayout(device, SetLayoutType_ImmutableSamplers, setLayoutBindings);
}

VKPtr<VkPipelineLayout> VKPipelineLayout::CreateVkPipelineLayout(VkDevice device, const ArrayView<VkPushConstantRange>& pushConstantRanges)
{
    /* Create native Vulkan pipeline layout with up to 3 descriptor sets */
    SmallVector<VkDescriptorSetLayout, SetLayoutType_Num> setLayoutsVK;

    for_range(i, SetLayoutType_Num)
    {
        if (descriptorSetLayouts_[i].Get() != VK_NULL_HANDLE)
        {
            /* Add descriptor set layout and assign binding slot, i.e. 'layout(set = N)' for SPIR-V code */
            descriptorSetBindSlots_[i] = static_cast<std::uint32_t>(setLayoutsVK.size());
            setLayoutsVK.push_back(descriptorSetLayouts_[i].Get());
        }
    }

    VkPipelineLayoutCreateInfo layoutCreateInfo;
    {
        layoutCreateInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.pNext                      = nullptr;
        layoutCreateInfo.flags                      = 0;
        layoutCreateInfo.setLayoutCount             = static_cast<std::uint32_t>(setLayoutsVK.size());
        layoutCreateInfo.pSetLayouts                = setLayoutsVK.data();
        if (pushConstantRanges.empty())
        {
            layoutCreateInfo.pushConstantRangeCount = 0;
            layoutCreateInfo.pPushConstantRanges    = nullptr;
        }
        else
        {
            layoutCreateInfo.pushConstantRangeCount = static_cast<std::uint32_t>(pushConstantRanges.size());
            layoutCreateInfo.pPushConstantRanges    = pushConstantRanges.data();
        }
    }
    VKPtr<VkPipelineLayout> pipelineLayout{ device, vkDestroyPipelineLayout };
    auto result = vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, pipelineLayout.ReleaseAndGetAddressOf());
    VKThrowIfFailed(result, "failed to create Vulkan pipeline layout");
    return pipelineLayout;
}

void VKPipelineLayout::CreateDescriptorPool(VkDevice device)
{
    /* Accumulate descriptor pool sizes for all dynamic resources and immutable samplers */
    VKPoolSizeAccumulator poolSizeAccum;

    for (const auto binding : bindings_)
        poolSizeAccum.Accumulate(binding.descriptorType);

    if (!immutableSamplers_.empty())
        poolSizeAccum.Accumulate(VK_DESCRIPTOR_TYPE_SAMPLER, static_cast<std::uint32_t>(immutableSamplers_.size()));

    poolSizeAccum.Finalize();

    /* Create Vulkan descriptor pool */
    VkDescriptorPoolCreateInfo poolCreateInfo;
    {
        poolCreateInfo.sType            = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.pNext            = nullptr;
        poolCreateInfo.flags            = 0;
        poolCreateInfo.maxSets          = 2;
        poolCreateInfo.poolSizeCount    = poolSizeAccum.Size();
        poolCreateInfo.pPoolSizes       = poolSizeAccum.Data();
    }
    auto result = vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, descriptorPool_.ReleaseAndGetAddressOf());
    VKThrowIfFailed(result, "failed to create Vulkan descriptor pool for static samplers");
}

void VKPipelineLayout::CreateDescriptorCache(VkDevice device, VkDescriptorSet setLayout)
{
    /*
    Don't account descriptors in the dynamic cache for immutable samplers,
    so accumulate pool sizes only for dynamiuc resources here
    */
    VKPoolSizeAccumulator poolSizeAccum;
    for (const auto binding : bindings_)
        poolSizeAccum.Accumulate(binding.descriptorType);
    poolSizeAccum.Finalize();

    /* Allocate unique descriptor cache */
    descriptorCache_ = MakeUnique<VKDescriptorCache>(device, descriptorPool_, setLayout, poolSizeAccum.Size(), poolSizeAccum.Data(), bindings_);
}

void VKPipelineLayout::CreateStaticDescriptorSet(VkDevice device, VkDescriptorSet setLayout)
{
    /* Allocate descriptor set for immutable samplers */
    VkDescriptorSetAllocateInfo allocInfo;
    {
        allocInfo.sType                 = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pNext                 = nullptr;
        allocInfo.descriptorPool        = descriptorPool_.Get();
        allocInfo.descriptorSetCount    = 1;
        allocInfo.pSetLayouts           = &setLayout;
    }
    auto result = vkAllocateDescriptorSets(device, &allocInfo, &staticDescriptorSet_);
    VKThrowIfFailed(result, "failed to allocate Vulkan descriptor sets");
}


} // /namespace LLGL



// ================================================================================
