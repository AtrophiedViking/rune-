#define _CRT_SECURE_NO_WARNINGS
#include <vulkan/vulkan.h>
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

// Base type
void readTextureTransform(const tinygltf::TextureInfo& info, TextureTransform& out)
{
    out.texCoord = info.texCoord;

    auto it = info.extensions.find("KHR_texture_transform");
    if (it == info.extensions.end()) return;

    const tinygltf::Value& ext = it->second;

    if (ext.Has("offset")) {
        const auto& arr = ext.Get("offset").Get<tinygltf::Value::Array>();
        out.offset = glm::vec2(arr[0].GetNumberAsDouble(), arr[1].GetNumberAsDouble());
    }
    if (ext.Has("scale")) {
        const auto& arr = ext.Get("scale").Get<tinygltf::Value::Array>();
        out.scale = glm::vec2(arr[0].GetNumberAsDouble(), arr[1].GetNumberAsDouble());
    }
    if (ext.Has("rotation")) {
        out.rotation = float(ext.Get("rotation").GetNumberAsDouble());
    }
    if (ext.Has("center")) {
        const auto& arr = ext.Get("center").Get<tinygltf::Value::Array>();
        out.center = glm::vec2(arr[0].GetNumberAsDouble(), arr[1].GetNumberAsDouble());
    }
    if (ext.Has("texCoord")) {
        out.texCoord = int(ext.Get("texCoord").GetNumberAsInt());
    }
}

// NormalTextureInfo overload
void readTextureTransform(const tinygltf::NormalTextureInfo& info, TextureTransform& out)
{
    // NormalTextureInfo has .texCoord too
    out.texCoord = info.texCoord;

    auto it = info.extensions.find("KHR_texture_transform");
    if (it == info.extensions.end()) return;

    const tinygltf::Value& ext = it->second;

    if (ext.Has("offset")) {
        const auto& arr = ext.Get("offset").Get<tinygltf::Value::Array>();
        out.offset = glm::vec2(arr[0].GetNumberAsDouble(), arr[1].GetNumberAsDouble());
    }
    if (ext.Has("scale")) {
        const auto& arr = ext.Get("scale").Get<tinygltf::Value::Array>();
        out.scale = glm::vec2(arr[0].GetNumberAsDouble(), arr[1].GetNumberAsDouble());
    }
    if (ext.Has("rotation")) {
        out.rotation = float(ext.Get("rotation").GetNumberAsDouble());
    }
    if (ext.Has("center")) {
        const auto& arr = ext.Get("center").Get<tinygltf::Value::Array>();
        out.center = glm::vec2(arr[0].GetNumberAsDouble(), arr[1].GetNumberAsDouble());
    }
    if (ext.Has("texCoord")) {
        out.texCoord = int(ext.Get("texCoord").GetNumberAsInt());
    }
}

// OcclusionTextureInfo overload
void readTextureTransform(const tinygltf::OcclusionTextureInfo& info, TextureTransform& out)
{
    out.texCoord = info.texCoord;

    auto it = info.extensions.find("KHR_texture_transform");
    if (it == info.extensions.end()) return;

    const tinygltf::Value& ext = it->second;

    if (ext.Has("offset")) {
        const auto& arr = ext.Get("offset").Get<tinygltf::Value::Array>();
        out.offset = glm::vec2(arr[0].GetNumberAsDouble(), arr[1].GetNumberAsDouble());
    }
    if (ext.Has("scale")) {
        const auto& arr = ext.Get("scale").Get<tinygltf::Value::Array>();
        out.scale = glm::vec2(arr[0].GetNumberAsDouble(), arr[1].GetNumberAsDouble());
    }
    if (ext.Has("rotation")) {
        out.rotation = float(ext.Get("rotation").GetNumberAsDouble());
    }
    if (ext.Has("center")) {
        const auto& arr = ext.Get("center").Get<tinygltf::Value::Array>();
        out.center = glm::vec2(arr[0].GetNumberAsDouble(), arr[1].GetNumberAsDouble());
    }
    if (ext.Has("texCoord")) {
        out.texCoord = int(ext.Get("texCoord").GetNumberAsInt());
    }
}

// Overload for extension-style texture objects (tinygltf::Value::Object)
void readTextureTransform(const tinygltf::Value& texObj, TextureTransform& out)
{
    if (!texObj.IsObject())
        return;

    const auto& obj = texObj.Get<tinygltf::Value::Object>();

    if (obj.count("extensions") &&
        obj.at("extensions").Has("KHR_texture_transform"))
    {
        const auto& ext = obj.at("extensions").Get("KHR_texture_transform");

        if (ext.Has("offset")) {
            const auto& arr = ext.Get("offset").Get<tinygltf::Value::Array>();
            out.offset = glm::vec2(arr[0].Get<double>(), arr[1].Get<double>());
        }

        if (ext.Has("scale")) {
            const auto& arr = ext.Get("scale").Get<tinygltf::Value::Array>();
            out.scale = glm::vec2(arr[0].Get<double>(), arr[1].Get<double>());
        }

        if (ext.Has("rotation")) {
            out.rotation = (float)ext.Get("rotation").Get<double>();
        }

        if (ext.Has("texCoord")) {
            out.texCoord = ext.Get("texCoord").Get<int>();
        }
    }
}

//mipmaps
void generateMipmaps(State* state, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(state->context->physicalDevice, imageFormat, &formatProperties);
    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands(state, state->renderer->commandPool);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    endSingleTimeCommands(state, commandBuffer);
}

void createTextureFromMemory(State* state, const unsigned char* pixels, size_t size, int width, int height, int channels, Texture& outTex)
{
    VkDevice device = state->context->device;

    // 1. Determine format
    TextureRole role = outTex.role;
    bool linear =
        role == TextureRole::Normal ||
        role == TextureRole::MetallicRoughness ||
        role == TextureRole::Occlusion;
   
    VkFormat format;

    if (linear) {
        format = VK_FORMAT_R8G8B8A8_UNORM;
        if (channels == 3)
            format = VK_FORMAT_R8G8B8_UNORM;
    }
    else {
        format = VK_FORMAT_R8G8B8A8_SRGB;
        if (channels == 3)
            format = VK_FORMAT_R8G8B8_SRGB;
    }
    outTex.format = format;

    // 2. Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    createBuffer(
        state,
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory
    );

    // Copy pixel data
    void* data;
    vkMapMemory(device, stagingMemory, 0, size, 0, &data);
    memcpy(data, pixels, size);
    vkUnmapMemory(device, stagingMemory);

    // 3. Create Vulkan image
    outTex.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

    imageCreate(
        state,
        width,
        height,
        format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        outTex.textureImage,
        outTex.textureImageMemory,
        outTex.mipLevels,
        VK_SAMPLE_COUNT_1_BIT
    );

    // 4. Transition + copy + mipmaps
    transitionImageLayout(
        state,
        outTex.textureImage,
        format,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        outTex.mipLevels,
        1
    );

    copyBufferToImage(
        state,
        stagingBuffer,
        outTex.textureImage,
        width,
        height
    );

    generateMipmaps(
        state,
        outTex.textureImage,
        format,
        width,
        height,
        outTex.mipLevels
    );

    // 5. Create image view
    outTex.textureImageView = imageViewCreate(
        state,
        outTex.textureImage,
        format,
        VK_IMAGE_ASPECT_COLOR_BIT,
        outTex.mipLevels
    );

    // 6. Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(outTex.mipLevels);

    vkCreateSampler(device, &samplerInfo, nullptr, &outTex.textureSampler);

    // 7. Cleanup staging
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
}
void destroyTextures(State* state) {
    VkDevice device = state->context->device;

    for (Texture* tex : state->scene->textures) {
        if (tex->textureSampler != VK_NULL_HANDLE)
            vkDestroySampler(device, tex->textureSampler, nullptr);

        if (tex->textureImageView != VK_NULL_HANDLE)
            vkDestroyImageView(device, tex->textureImageView, nullptr);

        if (tex->textureImage != VK_NULL_HANDLE)
            vkDestroyImage(device, tex->textureImage, nullptr);

        if (tex->textureImageMemory != VK_NULL_HANDLE)
            vkFreeMemory(device, tex->textureImageMemory, nullptr);
    }

    state->scene->textures.clear();
}

void textureImageCreate(State* state, std::string texturePath) {
    // Load KTX2 texture instead of using stb_image
    ktxTexture* kTexture;
    KTX_error_code result = ktxTexture_CreateFromNamedFile(
        texturePath.c_str(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &kTexture);

    if (result != KTX_SUCCESS) {
        throw std::runtime_error("failed to load ktx texture image!");
    }

    // Get texture dimensions and data
    uint32_t texWidth = kTexture->baseWidth;
    uint32_t texHeight = kTexture->baseHeight;
    ktx_size_t imageSize = ktxTexture_GetImageSize(kTexture, 0);
    ktx_uint8_t* ktxTextureData = ktxTexture_GetData(kTexture);

    VkFormat textureFormat = ktxTexture_GetVkFormat(kTexture);

    // Save the actual format used for image creation so image views use the
    // identical format (required by Vulkan unless VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT).
    state->texture->format = textureFormat;

    state->texture->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(state, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(state->context->device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, ktxTextureData, imageSize);
    vkUnmapMemory(state->context->device, stagingBufferMemory);

    imageCreate(state, texWidth, texHeight, textureFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, state->texture->textureImage, state->texture->textureImageMemory, state->texture->mipLevels, VK_SAMPLE_COUNT_1_BIT);

    transitionImageLayout(state, state->texture->textureImage, textureFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, state->texture->mipLevels, 1);
    copyBufferToImage(state, stagingBuffer, state->texture->textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    vkDestroyBuffer(state->context->device, stagingBuffer, nullptr);
    vkFreeMemory(state->context->device, stagingBufferMemory, nullptr);
    generateMipmaps(state, state->texture->textureImage, textureFormat, texWidth, texHeight, state->texture->mipLevels);
    ktxTexture_Destroy(kTexture);
};
void textureImageDestroy(State* state) {
    if (state->texture->textureImage != VK_NULL_HANDLE)
        vkDestroyImage(state->context->device, state->texture->textureImage, nullptr);

    if (state->texture->textureImageMemory != VK_NULL_HANDLE)
        vkFreeMemory(state->context->device, state->texture->textureImageMemory, nullptr);
};

void textureImageViewCreate(State* state) {
    // Use the exact VkFormat the image was created with.
    VkFormat viewFormat = state->texture->format;
    if (viewFormat == VK_FORMAT_UNDEFINED) {
        // Fallback, but ideally format should always be set when image is created.
        viewFormat = VK_FORMAT_R8G8B8A8_SRGB;
    }
    state->texture->textureImageView = imageViewCreate(state, state->texture->textureImage, viewFormat, VK_IMAGE_ASPECT_COLOR_BIT, state->texture->mipLevels);
};
void textureImageViewDestroy(State* state) {
    vkDestroyImageView(state->context->device, state->texture->textureImageView, nullptr);
};

void textureSamplerCreate(State* state) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(state->context->physicalDevice, &properties);
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

    PANIC(vkCreateSampler(state->context->device, &samplerInfo, nullptr, &state->texture->textureSampler), "failed to create texture sampler!");
};
void textureSamplerDestroy(State* state) {
    vkDestroySampler(state->context->device, state->texture->textureSampler, nullptr);
};

void createModelTextures(State* state, Model* model, const tinygltf::Model& gltf, std::unordered_map<int, TextureRole>& textureRoles) 
{
    const uint32_t baseIndex = model->baseTextureIndex;

    // If the glTF has embedded or external images
    if (!gltf.images.empty())
    {
        state->scene->textures.reserve(
            state->scene->textures.size() + gltf.images.size()
        );

        for (int i = 0; i < (int)gltf.images.size(); i++)
        {
            const tinygltf::Image& img = gltf.images[i];

            Texture* tex = new Texture{};

            int globalIndex = baseIndex + i;

            // Assign role if known, otherwise default to BaseColor
            if (textureRoles.count(globalIndex))
                tex->role = textureRoles[globalIndex];
            else
                tex->role = TextureRole::BaseColor;

            // Upload to GPU
            createTextureFromMemory(
                state,
                img.image.data(),
                img.image.size(),
                img.width,
                img.height,
                img.component,
                *tex
            );

            state->scene->textures.push_back(tex);
        }
    }
    else
    {
        // No images in glTF → use fallback texture
        Texture* tex = new Texture{};

        textureImageCreate(state, state->config->DEFAULT_TEXTURE_PATH);
        textureImageViewCreate(state);
        textureSamplerCreate(state);

        tex->textureImageView = state->texture->textureImageView;
        tex->textureSampler = state->texture->textureSampler;
        tex->role = TextureRole::BaseColor;

        state->scene->textures.push_back(tex);
    }
}
