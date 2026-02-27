#include "loader/gltf_extensions.h"
#include "loader/gltf_textures.h"
#include "scene/materials.h"
#include "scene/texture.h"
#include "tiny_gltf.h"


void parseTransmissionExtension(const tinygltf::Material& m, Material& mat, uint32_t baseTextureIndex, std::unordered_map<int, TextureRole>& roles)
{
    auto it = m.extensions.find("KHR_materials_transmission");
    if (it == m.extensions.end())
        return;

    const tinygltf::Value& ext = it->second;

    // transmissionFactor
    if (ext.Has("transmissionFactor"))
        mat.transmissionFactor = (float)ext.Get("transmissionFactor").Get<double>();

    // transmissionTexture
    if (ext.Has("transmissionTexture"))
    {
        const auto& tex = ext.Get("transmissionTexture");

        int idx = tex.Get("index").Get<int>();
        mat.transmissionTextureIndex = baseTextureIndex + idx;

        if (tex.Has("texCoord"))
            mat.transmissionTexCoordIndex = tex.Get("texCoord").Get<int>();

        // KHR_texture_transform inside extension
        readTextureTransform(tex, mat.transmissionTransform);

        roles[mat.transmissionTextureIndex] = TextureRole::BaseColor;
        // You can add a dedicated Transmission role later if desired
    }
}
void parseVolumeExtension(const tinygltf::Material& m, Material& mat, uint32_t baseTextureIndex, std::unordered_map<int, TextureRole>& roles)
{
    auto it = m.extensions.find("KHR_materials_volume");
    if (it == m.extensions.end())
        return;

    const tinygltf::Value& ext = it->second;

    // thicknessFactor
    if (ext.Has("thicknessFactor"))
        mat.thicknessFactor = (float)ext.Get("thicknessFactor").Get<double>();

    // attenuationColor (vec3)
    if (ext.Has("attenuationColor"))
    {
        const auto& arr = ext.Get("attenuationColor").Get<tinygltf::Value::Array>();
        mat.attenuationColor = glm::vec4(
            arr[0].Get<double>(),
            arr[1].Get<double>(),
            arr[2].Get<double>(),
            1.0f
        );
    }

    // attenuationDistance
    if (ext.Has("attenuationDistance"))
        mat.attenuationDistance = (float)ext.Get("attenuationDistance").Get<double>();

    // thicknessTexture
    if (ext.Has("thicknessTexture"))
    {
        const auto& tex = ext.Get("thicknessTexture");

        int idx = tex.Get("index").Get<int>();
        mat.thicknessTextureIndex = baseTextureIndex + idx;

        if (tex.Has("texCoord"))
            mat.thicknessTexCoordIndex = tex.Get("texCoord").Get<int>();

        roles[mat.thicknessTextureIndex] = TextureRole::Occlusion;
        // Or create a dedicated Thickness role later
    }
}
void parseIorExtension(const tinygltf::Material& m, Material& mat)
{
    auto it = m.extensions.find("KHR_materials_ior");
    if (it == m.extensions.end())
        return;

    const tinygltf::Value& ext = it->second;

    if (ext.Has("ior"))
        mat.ior = (float)ext.Get("ior").Get<double>();
}
