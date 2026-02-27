#define _CRT_SECURE_NO_WARNINGS
#include "loader/ktx_cubemap.h"
#include <ktx.h>
#include <ktxvulkan.h>
#include <tiny_gltf.h>
#include "gltf_textures.h"
#include "resources/images.h"
#include "resources/buffers.h"
#include "render/renderer.h"
#include "render/command_buffers.h"

#include "loader/gltf_materials.h"
#include "scene/texture.h"
#include "scene/model.h"
#include "scene/scene.h"
#include "core/context.h"
#include "core/config.h"
#include "core/state.h"



void copyBufferToImageRegions(State* state,
    VkBuffer buffer,
    VkImage image,
    const std::vector<VkBufferImageCopy>& regions)
{
    VkCommandBuffer cmd = beginSingleTimeCommands(state, state->renderer->commandPool);

    vkCmdCopyBufferToImage(cmd,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(regions.size()),
        regions.data());

    endSingleTimeCommands(state, cmd);
}


void imageCreateCube(State* state,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    uint32_t mipLevels,
    VkImage& image,
    VkDeviceMemory& imageMemory,
    VkMemoryPropertyFlags properties)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 6;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(state->context->device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(state->context->device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(state, memRequirements, properties);

    if (vkAllocateMemory(state->context->device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(state->context->device, image, imageMemory, 0);
}

void createCubeImageView(State* state,
    VkImage image,
    VkFormat format,
    uint32_t mipLevels,
    VkImageView& imageView)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = format;

    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    if (vkCreateImageView(state->context->device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create cube image view!");
    }
}

void textureCubeImageCreate(State* state, const std::string& texturePath)
{
    ktxTexture* kTexture;
    KTX_error_code result = ktxTexture_CreateFromNamedFile(
        texturePath.c_str(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &kTexture);

    if (result != KTX_SUCCESS) {
        throw std::runtime_error("failed to load ktx cube texture image!");
    }

    if (kTexture->numFaces != 6) {
        ktxTexture_Destroy(kTexture);
        throw std::runtime_error("ktx texture is not a cubemap (numFaces != 6)");
    }

    uint32_t texWidth = kTexture->baseWidth;
    uint32_t texHeight = kTexture->baseHeight;
    uint32_t mipLevels = kTexture->numLevels;

    VkFormat textureFormat = ktxTexture_GetVkFormat(kTexture);
    state->texture->format = textureFormat;
    state->texture->mipLevels = mipLevels;

    ktx_size_t imageSize = ktxTexture_GetDataSize(kTexture);
    ktx_uint8_t* ktxTextureData = ktxTexture_GetData(kTexture);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(state,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory);

    void* data;
    vkMapMemory(state->context->device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, ktxTextureData, imageSize);
    vkUnmapMemory(state->context->device, stagingBufferMemory);

    // NOTE: imageCreate must support arrayLayers=6 and CUBE_COMPATIBLE
    imageCreateCube(state,
        texWidth,
        texHeight,
        textureFormat,
        mipLevels,
        state->texture->cubeImage,
        state->texture->cubeImageMemory,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    transitionImageLayout(state,
        state->texture->cubeImage,
        textureFormat,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        mipLevels, 6);

    // Copy each face & mip level
    std::vector<VkBufferImageCopy> regions;
    regions.reserve(mipLevels * 6);

    for (uint32_t level = 0; level < mipLevels; ++level) {
        uint32_t mipWidth = std::max(1u, texWidth >> level);
        uint32_t mipHeight = std::max(1u, texHeight >> level);

        for (uint32_t face = 0; face < 6; ++face) {
            ktx_size_t offset;
            KTX_error_code offRes = ktxTexture_GetImageOffset(kTexture, level, 0, face, &offset);
            if (offRes != KTX_SUCCESS) {
                ktxTexture_Destroy(kTexture);
                throw std::runtime_error("failed to get ktx cubemap image offset");
            }

            VkBufferImageCopy region{};
            region.bufferOffset = offset;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;

            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = level;
            region.imageSubresource.baseArrayLayer = face;
            region.imageSubresource.layerCount = 1;

            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = { mipWidth, mipHeight, 1 };

            regions.push_back(region);
        }
    }

    copyBufferToImageRegions(state,
        stagingBuffer,
        state->texture->cubeImage,
        regions);

    vkDestroyBuffer(state->context->device, stagingBuffer, nullptr);
    vkFreeMemory(state->context->device, stagingBufferMemory, nullptr);

    // No need to generate mipmaps if KTX already has them
    transitionImageLayout(state,
        state->texture->cubeImage,
        textureFormat,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        mipLevels, 6);

    ktxTexture_Destroy(kTexture);
}

void createCubeSampler(State* state, VkSampler& sampler)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = (float)state->texture->mipLevels;
    samplerInfo.mipLodBias = 0.0f;

    if (vkCreateSampler(state->context->device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create cube sampler!");
    }
}
