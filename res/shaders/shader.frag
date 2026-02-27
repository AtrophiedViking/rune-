#version 450

// ─────────────────────────────────────────────
// Push Constants (glTF material semantics)
// ─────────────────────────────────────────────
layout(push_constant) uniform PushConstants {
    mat4 nodeMatrix;
    vec4  baseColorFactor;
    float metallicFactor;
    float roughnessFactor;

    int baseColorTextureSet;
    int physicalDescriptorTextureSet;
    int normalTextureSet;
    int occlusionTextureSet;
    int emissiveTextureSet;

    float alphaMask;
    float alphaMaskCutoff;

    float transmissionFactor;
    int   transmissionTextureIndex;
    int   transmissionTexCoordIndex;
    int   _padT;

    float thicknessFactor;
    int   thicknessTextureIndex;
    int   thicknessTexCoordIndex;
    vec4  attenuation;
} pc;

// ─────────────────────────────────────────────
// UBO (set = 0, binding = 0)
// ─────────────────────────────────────────────
layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;

    vec4 lightPositions[4];  // xyz = position (world), w = radius
    vec4 lightColors[4];     // rgb = color, w = intensity
    vec4 camPos;             // xyz = camera position (world)

    float exposure;
    float gamma;
    float prefilteredCubeMipLevels; // unused here
    float scaleIBLAmbient;          // unused here
} ubo;
layout(set = 0, binding = 1) uniform samplerCube envMap;
layout(set = 0, binding = 2) uniform sampler2D sceneColor;
layout(set = 0, binding = 3) uniform sampler2D sceneDepth;
// ─────────────────────────────────────────────
// UBO (set = 1, binding = 5)
// ─────────────────────────────────────────────
struct TexTransform {
    // xy = offset, zw = scale
    vec4 offset_scale;

    // x = rotation, y/z = center, w = texCoord (as float)
    vec4 rot_center_tex;
};

layout(std140, set = 1, binding = 0) uniform MaterialData {
    TexTransform baseColorTT;
    TexTransform mrTT;
    TexTransform normalTT;
    TexTransform occlusionTT;
    TexTransform emissiveTT;

    float thicknessFactor;
	float attenuationDistance;
	vec4 attenuationColor;
	float ior;
} uMat;




// ─────────────────────────────────────────────
// Material Textures (set = 1)
// ─────────────────────────────────────────────
layout(set = 1, binding = 1) uniform sampler2D baseColorTex;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessTex;
layout(set = 1, binding = 3) uniform sampler2D occlusionTex;
layout(set = 1, binding = 4) uniform sampler2D emissiveTex;
layout(set = 1, binding = 5) uniform sampler2D normalTex;
layout(set = 1, binding = 6) uniform sampler2D transmissionTex;
layout(set = 1, binding = 7) uniform sampler2D volumeTex;


// ─────────────────────────────────────────────
// Inputs from vertex shader
// ─────────────────────────────────────────────
layout(location = 0) in vec3 fragColorVS;
layout(location = 1) in vec2 fragTexCoordVS0;
layout(location = 2) in vec2 fragTexCoordVS1;
layout(location = 3) in vec3 fragWorldPos;
layout(location = 4) in vec3 fragNormal;
layout(location = 5) in vec3 fragTangent;
layout(location = 6) in vec3 fragBitangent;

// ─────────────────────────────────────────────
// Output
// ─────────────────────────────────────────────
layout(location = 0) out vec4 outAccum;   // color * alpha, alpha
layout(location = 1) out float outReveal; // revealage

// ─────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────
vec2 getUV(int setIndex)
{
    return (setIndex == 0) ? fragTexCoordVS0 : fragTexCoordVS1;
}

vec2 applyTextureTransform(vec2 uv, TexTransform tt)
{
    vec2 offset   = tt.offset_scale.xy;
    vec2 scale    = tt.offset_scale.zw;
    float rotation = tt.rot_center_tex.x;
    vec2 center   = tt.rot_center_tex.yz;

    uv -= center;

    float s = sin(rotation);
    float c = cos(rotation);
    uv = vec2(
        uv.x * c - uv.y * s,
        uv.x * s + uv.y * c
    );

    uv *= scale;
    uv += center;
    uv += offset;

    return uv;
}

int getTexCoordIndex(TexTransform tt)
{
    return int(max(tt.rot_center_tex.w, 0.0) + 0.5);
}

vec3 getNormalFromMap()
{
    bool hasNormalTex = (uMat.normalTT.rot_center_tex.w >= 0.0);

    if (!hasNormalTex) {
        return normalize(fragNormal);
    }

    vec2 uv = getUV(getTexCoordIndex(uMat.normalTT));
    uv = applyTextureTransform(uv, uMat.normalTT);

    vec3 tangentNormal = texture(normalTex, uv).xyz * 2.0 - 1.0;

    vec3 N0 = normalize(fragNormal);
    vec3 T  = normalize(fragTangent);
    T = normalize(T - N0 * dot(N0, T));
    vec3 B  = cross(N0, T) * sign(dot(fragBitangent, cross(N0, T)));

    mat3 TBN = mat3(T, B, N0);
    return normalize(TBN * tangentNormal);
}


float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159265 * denom * denom;

    return num / max(denom, 1e-4);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / max(denom, 1e-4);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1  = GeometrySchlickGGX(NdotV, roughness);
    float ggx2  = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}
float IorToF0(float ior)
{
    float x = (ior - 1.0) / (ior + 1.0);
    return x * x;
}
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

//GLTF extension helpers
float getTransmission()
{
    float t = pc.transmissionFactor;

    if (pc.transmissionTextureIndex >= 0) {
        // For now we ignore per-texture index and just use a single bound sampler
        vec2 uv = getUV(pc.transmissionTexCoordIndex);
        // If you later add a TexTransform for transmission, apply it here
        // uv = applyTextureTransform(uv, uMat.transmissionTT);

        t *= texture(transmissionTex, uv).r;
    }

    return clamp(t, 0.0, 1.0);
}

float getThickness()
{
    float thick = pc.thicknessFactor;

    if (pc.thicknessTextureIndex >= 0) {
        vec2 uv = getUV(pc.thicknessTexCoordIndex);
        // If you later add a TexTransform for volume, apply it here
        // uv = applyTextureTransform(uv, uMat.volumeTT);
        thick *= texture(volumeTex, uv).r;
    }

    return thick;
}

vec3 applyVolumeToTransmission(vec3 transmittedColor)
{
    float thick = getThickness();

    float dist = pc.attenuation.w;
    if (dist <= 0.0)
        return transmittedColor;

    float ratio = thick / max(dist, 1e-4);
    vec3 att = pow(pc.attenuation.xyz, vec3(ratio));

    return transmittedColor * att;
}

vec3 sampleEnvironment(vec3 dir)
{
    return texture(envMap, dir).rgb;
}


vec2 computeRefractedUV(vec3 worldPos, vec3 N, vec3 V)
{
    float eta = 1.0 / uMat.ior;
    vec3 R = refract(-V, N, eta);

    // Use glTF thicknessFactor to control distortion depth
    vec3 samplePos = worldPos + R * uMat.thicknessFactor;

    vec4 clip = ubo.proj * ubo.view * vec4(samplePos, 1.0);
    vec3 ndc  = clip.xyz / clip.w;

    return ndc.xy * 0.5 + 0.5;
}

vec2 refractUVDepthAware(vec3 worldPos, vec3 N, vec3 V)
{
    float eta = 1.0 / uMat.ior;
    vec3 R = refract(-V, N, eta);

    // March the ray in view space
    vec3 viewPos = (ubo.view * vec4(worldPos, 1.0)).xyz;
    vec3 viewDir = normalize((ubo.view * vec4(worldPos + R, 1.0)).xyz - viewPos);

    // Step size in view space (tune this)
    float stepSize = 0.05;
    float maxDistance = 5.0;

    for (float t = 0.0; t < maxDistance; t += stepSize)
    {
        vec3 p = viewPos + viewDir * t;

        // Project to clip space
        vec4 clip = ubo.proj * vec4(p, 1.0);
        vec3 ndc = clip.xyz / clip.w;
        vec2 uv = ndc.xy * 0.5 + 0.5;

        // Skip if outside screen
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
            continue;

        // Sample depth buffer
        float sceneDepthNDC = texture(sceneDepth, uv).r;

        // Convert depth to view space
        float sceneDepthView = -ubo.proj[3][2] / (sceneDepthNDC * 2.0 - 1.0 + ubo.proj[2][2]);

        // If ray is behind geometry → intersection found
        if (-p.z > sceneDepthView)
            return uv;
    }

    // Fallback: simple projection
    vec4 clip = ubo.proj * ubo.view * vec4(worldPos + R, 1.0);
    vec3 ndc = clip.xyz / clip.w;
    return ndc.xy * 0.5 + 0.5;
}


// ─────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────
void main()
{
    // Base color (optional)
    bool hasBaseColorTex = (uMat.baseColorTT.rot_center_tex.w >= 0.0);

    vec4 baseColor;
    if (hasBaseColorTex) {
        vec2 uvBase = getUV(getTexCoordIndex(uMat.baseColorTT));
        uvBase = applyTextureTransform(uvBase, uMat.baseColorTT);
        vec4 baseSample = texture(baseColorTex, uvBase);
        baseColor = baseSample * pc.baseColorFactor;
    } else {
        baseColor = pc.baseColorFactor;
    }

    // Alpha mask
    if (pc.alphaMask > 0.5) {
        if (baseColor.a < pc.alphaMaskCutoff) {
            discard;
        }
    }

    // Metallic-Roughness (optional)
    bool hasMRTex = (uMat.mrTT.rot_center_tex.w >= 0.0);

    float metallic;
    float roughness;

    if (hasMRTex) {
        vec2 uvMR = getUV(getTexCoordIndex(uMat.mrTT));
        uvMR = applyTextureTransform(uvMR, uMat.mrTT);
        vec3 mrSample = texture(metallicRoughnessTex, uvMR).rgb;
        metallic  = clamp(mrSample.b * pc.metallicFactor, 0.0, 1.0);
        roughness = clamp(mrSample.g * pc.roughnessFactor, 0.04, 1.0);
    } else {
        metallic  = clamp(pc.metallicFactor, 0.0, 1.0);
        roughness = clamp(pc.roughnessFactor, 0.04, 1.0);
    }

    // Occlusion (optional)
    bool hasOccTex = (uMat.occlusionTT.rot_center_tex.w >= 0.0);

    float occlusion;
    if (hasOccTex) {
        vec2 uvOcc = getUV(getTexCoordIndex(uMat.occlusionTT));
        uvOcc = applyTextureTransform(uvOcc, uMat.occlusionTT);
        occlusion = texture(occlusionTex, uvOcc).r;
    } else {
        occlusion = 1.0;
    }

    // Emissive (optional)
    bool hasEmissiveTex = (uMat.emissiveTT.rot_center_tex.w >= 0.0);

    vec3 emissive;
    if (hasEmissiveTex) {
        vec2 uvEm = getUV(getTexCoordIndex(uMat.emissiveTT));
        uvEm = applyTextureTransform(uvEm, uMat.emissiveTT);
        emissive = texture(emissiveTex, uvEm).rgb;
    } else {
        emissive = vec3(0.0);
    }

    // Normal (optional via getNormalFromMap)
    vec3 N = getNormalFromMap();
    vec3 V = normalize(ubo.camPos.xyz - fragWorldPos);

   vec3 albedo = pow(baseColor.rgb * fragColorVS, vec3(2.2));

    float F0_scalar = IorToF0(uMat.ior);   // uses KHR_materials_ior
    vec3  F0        = vec3(F0_scalar);
    
    // Metallic overrides F0 toward albedo
    F0 = mix(F0, albedo, metallic);

    // Direct lighting
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 4; ++i) {
        vec3 Lpos = ubo.lightPositions[i].xyz;
        float radius = ubo.lightPositions[i].w;
        vec3 Lcolor = ubo.lightColors[i].rgb * ubo.lightColors[i].w;

        vec3 L = Lpos - fragWorldPos;
        float dist = length(L);
        if (radius > 0.0 && dist > radius) continue;

        L = normalize(L);
        vec3 H = normalize(V + L);

        float scalerAttenuation = 1.0 / max(dist * dist, 1e-4);
        if (radius > 0.0) {
            float falloff = clamp(1.0 - dist / radius, 0.0, 1.0);
            scalerAttenuation *= falloff * falloff;
        }

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator    = NDF * G * F;
        float denom       = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 1e-4;
        vec3 specular     = numerator / denom;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        vec3  radiance = Lcolor * scalerAttenuation;

        Lo += (kD * albedo / 3.14159265 + specular) * radiance * NdotL;
    }

    vec3 ambient = albedo * 0.03 * occlusion;
    vec3 colorOpaque = ambient + Lo + emissive;

    float transmission = getTransmission();
    // ─────────────────────────────────────────────
    // Refraction: screen-space + env fallback
    // ─────────────────────────────────────────────
    N = normalize(N);
    
    // Screen-space refracted UV
    vec2 refractUV = refractUVDepthAware(fragWorldPos, N, V);
    refractUV = clamp(refractUV, vec2(0.001), vec2(0.999));
    
    // Sample scene color (actual geometry behind)
    vec3 sceneRefract = texture(sceneColor, refractUV).rgb;
    
    // Env-based fallback (for off-screen / background)
    float eta = 1.0 / uMat.ior;
    vec3 Renv = refract(-V, N, eta);
    vec3 envRefract = texture(envMap, Renv).rgb;
    
    // Volume attenuation
    sceneRefract = applyVolumeToTransmission(sceneRefract);
    envRefract   = applyVolumeToTransmission(envRefract);
    
    // Fresnel at view angle using IOR-based F0
    float NdotV = max(dot(N, V), 0.0);
    vec3 Fview  = FresnelSchlick(NdotV, F0);
    
    // Blend scene vs env refraction (scene dominates)
    vec3 refractedColor = mix(envRefract, sceneRefract, transmission);
    
    // Portion that actually goes into transmission
    float transWeight = transmission * (1.0 - Fview.r);
    
    // Final color mix: opaque lighting vs refraction
    vec3 color = mix(colorOpaque, refractedColor, transWeight);
    

    color = vec3(1.0) - exp(-color * ubo.exposure);
    color = pow(color, vec3(1.0 / ubo.gamma));

     // Option A: simple, glTF-like
    //float alpha = baseColor.a * (1.0 - transmission);
    
    // or, if you want to ignore baseColor.a for transmission:
    float alpha = baseColor.a * (1.0 - (transmission/2));


    // discard fully transparent
    if (alpha <= 0.0) {
        discard;
    }

    // WBOIT outputs
    outAccum  = vec4(color * alpha, alpha); // rgb premultiplied, a = weight
    outReveal = 1.0 - alpha;


}
