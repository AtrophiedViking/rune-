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


void cubeImageCreate(
    State* state,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    uint32_t mipLevels,
    VkImageUsageFlags usage,
    VkImage& image,
    VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.extent = { width, height, 1 };
    info.mipLevels = mipLevels;
    info.arrayLayers = 6;
    info.format = format;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.usage = usage;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(state->context->device, &info, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("failed to create cube image!");

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(state->context->device, image, &memReq);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;
    alloc.memoryTypeIndex = findMemoryType(
        state, memReq, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(state->context->device, &alloc, nullptr, &imageMemory) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate cube image memory!");

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

void textureCubeImageCreate(
    State* state,
    const std::string& texturePath,
    VkImage& outImage,
    VkDeviceMemory& outImageMemory,
    VkFormat& outFormat,
    uint32_t& outMipLevels,
    uint32_t* outWidth,
    uint32_t* outHeight)
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
	std::cout << "Loaded KTX cubemap: " << texturePath << " (format: " << textureFormat << ", width: " << texWidth << ", height: " << texHeight << ", mipLevels: " << mipLevels << ")" << std::endl;
    outFormat = textureFormat;
    outMipLevels = mipLevels;

    ktx_size_t imageSize = ktxTexture_GetDataSize(kTexture);
    ktx_uint8_t* ktxTextureData = ktxTexture_GetData(kTexture);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(
        state,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data;
    vkMapMemory(state->context->device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, ktxTextureData, imageSize);
    vkUnmapMemory(state->context->device, stagingBufferMemory);

    cubeImageCreate(
        state,
        texWidth,
        texHeight,
        textureFormat,
        mipLevels,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        outImage,
        outImageMemory
    );

    transitionImageLayout(
        state,
        outImage,
        textureFormat,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        mipLevels,
        6
    );

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

    copyBufferToImageRegions(
        state,
        stagingBuffer,
        outImage,
        regions
    );

    vkDestroyBuffer(state->context->device, stagingBuffer, nullptr);
    vkFreeMemory(state->context->device, stagingBufferMemory, nullptr);

    transitionImageLayout(
        state,
        outImage,
        textureFormat,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        mipLevels,
        6
    );

    uint32_t Width = kTexture->baseWidth;
    uint32_t Height = kTexture->baseHeight;

    if (outWidth)  *outWidth = Width;
    if (outHeight) *outHeight = Height;

    ktxTexture_Destroy(kTexture);
}

void computeCubeImageCreate(
    State* state,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    uint32_t mipLevels,
    VkImage& outImage,
    VkDeviceMemory& outMemory)
{
    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT;

    cubeImageCreate(
        state,
        width,
        height,
        format,
        mipLevels,
        usage,
        outImage,
        outMemory
    );
}

void createCubeSampler(State* state, VkSampler& sampler, uint32_t mipLevels)
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
    samplerInfo.maxLod = (float)mipLevels;
    samplerInfo.mipLodBias = 0.0f;

    if (vkCreateSampler(state->context->device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create cube sampler!");
    }
}
