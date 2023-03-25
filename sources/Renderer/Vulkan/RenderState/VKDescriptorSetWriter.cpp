/*
 * VKDescriptorSetWriter.cpp
 *
 * This file is part of the "LLGL" project (Copyright (c) 2015-2019 by Lukas Hermanns)
 * See "LICENSE.txt" for license information.
 */

#include "VKDescriptorSetWriter.h"
#include <algorithm>


namespace LLGL
{


VKDescriptorSetWriter::VKDescriptorSetWriter(
    std::uint32_t numResourceViewsMax,
    std::uint32_t numReservedWrites,
    std::uint32_t numReservedCopies)
:
    bufferInfos_ { numResourceViewsMax },
    imageInfos_  { numResourceViewsMax }
{
    writes_.reserve(numReservedWrites);
    copies_.reserve(numReservedCopies);
}

void VKDescriptorSetWriter::Reset()
{
    writes_.clear();
    copies_.clear();
    numBufferInfos_ = 0;
    numImageInfos_ = 0;
}

VkDescriptorBufferInfo* VKDescriptorSetWriter::NextBufferInfo()
{
    if (numBufferInfos_ < bufferInfos_.size())
        return &(bufferInfos_[numBufferInfos_++]);
    else
        return nullptr;
}

VkDescriptorImageInfo* VKDescriptorSetWriter::NextImageInfo()
{
    if (numImageInfos_ < imageInfos_.size())
        return &(imageInfos_[numImageInfos_++]);
    else
        return nullptr;
}

VkWriteDescriptorSet* VKDescriptorSetWriter::NextWriteDescriptor()
{
    VkWriteDescriptorSet initialWriteDescriptor = {};
    {
        initialWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    }
    writes_.push_back(initialWriteDescriptor);
    return &(writes_.back());
}

VkCopyDescriptorSet* VKDescriptorSetWriter::NextCopyDescriptor()
{
    VkCopyDescriptorSet initialCopyDescriptor = {};
    {
        initialCopyDescriptor.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
    }
    copies_.push_back(initialCopyDescriptor);
    return &(copies_.back());
}

void VKDescriptorSetWriter::UpdateDescriptorSets(VkDevice device)
{
    if (!writes_.empty() || !copies_.empty())
    {
        vkUpdateDescriptorSets(
            device,
            static_cast<std::uint32_t>(writes_.size()),
            writes_.data(),
            static_cast<std::uint32_t>(copies_.size()),
            copies_.data()
        );
    }
}


} // /namespace LLGL



// ================================================================================