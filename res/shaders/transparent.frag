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
layout(location = 7) in vec2 fragSceneUV;

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

vec3 sampleEnvironment(vec3 dirWS)
{
    vec3 envDir = normalize(dirWS);

    // If your skybox needed a flip, apply the same one here:
    // envDir = vec3(envDir.x, envDir.y, -envDir.z);
    // or envDir = vec3(-envDir.x, envDir.y, envDir.z);

    return texture(envMap, envDir).rgb;
}

vec2 refractOffset(vec3 Nvs, vec3 Vvs)
{
    float eta = 1.0 / uMat.ior;
    vec3 Rvs = refract(-Vvs, Nvs, eta);

    // Project Rvs into screen space
    vec3 P = Rvs * uMat.thicknessFactor;
    vec4 clip = ubo.proj * vec4(P, 1.0);
    vec3 ndc = clip.xyz / clip.w;

    // Scale down for stability
    return ndc.xy * 0.25;
}



vec2 refractUVDepthAwareScreen(vec2 baseUV, vec3 N, vec3 V)
{
    mat3 viewRot = mat3(ubo.view); 

    vec3 Nvs = normalize(viewRot * N);
    vec3 Vvs = normalize(viewRot * V);
    
    vec2 dir = refractOffset(Nvs, Vvs);
    float steps = 16.0;

    float baseDepth = texture(sceneDepth, baseUV).r;
    float lastDepth = baseDepth;
    vec2  lastUV    = baseUV;

    for (float i = 1.0; i <= steps; i += 1.0)
    {
        vec2 uv = baseUV + dir * (i / steps);
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
            break;

        float d = texture(sceneDepth, uv).r;

        // we just went behind some opaque geometry
        if (d < lastDepth - 1e-3)
        {
            // simple lerp between last and current for stability
            float a = 0.5;
            return clamp(mix(lastUV, uv, a), vec2(0.001), vec2(0.999));
        }

        lastDepth = d;
        lastUV    = uv;
    }

    // no hit → just use the last UV (background / env)
    return clamp(lastUV, vec2(0.001), vec2(0.999));
}


vec2 refractUVSimple(vec3 worldPos, vec3 N, vec3 V)
{
    float eta = 1.0 / uMat.ior;
    vec3 R = refract(-V, N, eta);

    vec4 clip = ubo.proj * ubo.view * vec4(worldPos + R * uMat.thicknessFactor, 1.0);
    vec3 ndc  = clip.xyz / clip.w;

    vec2 uv = ndc.xy * 0.5 + 0.5;
    return clamp(uv, vec2(0.001), vec2(0.999));
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

    // Metallic-Roughness
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

    // Occlusion
    bool hasOccTex = (uMat.occlusionTT.rot_center_tex.w >= 0.0);

    float occlusion;
    if (hasOccTex) {
        vec2 uvOcc = getUV(getTexCoordIndex(uMat.occlusionTT));
        uvOcc = applyTextureTransform(uvOcc, uMat.occlusionTT);
        occlusion = texture(occlusionTex, uvOcc).r;
    } else {
        occlusion = 1.0;
    }

    // Emissive
    bool hasEmissiveTex = (uMat.emissiveTT.rot_center_tex.w >= 0.0);

    vec3 emissive;
    if (hasEmissiveTex) {
        vec2 uvEm = getUV(getTexCoordIndex(uMat.emissiveTT));
        uvEm = applyTextureTransform(uvEm, uMat.emissiveTT);
        emissive = texture(emissiveTex, uvEm).rgb;
    } else {
        emissive = vec3(0.0);
    }

    // Normal + view (WORLD SPACE)
    vec3 N = getNormalFromMap();                       // world
    vec3 V = normalize(ubo.camPos.xyz - fragWorldPos); // world

    // Base color to linear
    vec3 baseLinear = pow(baseColor.rgb, vec3(2.2));
    vec3 albedo     = baseLinear * fragColorVS;

    float F0_scalar = IorToF0(uMat.ior);
    vec3  F0        = vec3(F0_scalar);

    // clamp F0 so glass isn't chrome
    F0 = clamp(F0, 0.02, 0.08);
    F0 = mix(F0, albedo, metallic);

    // ─────────────────────────────────────────────
    // Environment Reflection (WORLD SPACE)
    // ─────────────────────────────────────────────
    vec3 Rref = reflect(-V, N);
    vec3 envReflect = sampleEnvironment(Rref);
    float NdotV_ref = max(dot(N, V), 0.0);
    vec3 Fspec = FresnelSchlick(NdotV_ref, F0);
    vec3 reflection = envReflect * Fspec;
    

    // Direct lighting (WORLD SPACE)
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 4; ++i) {
        vec3 Lpos   = ubo.lightPositions[i].xyz;
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

        vec3 numerator = NDF * G * F;
        float denom    = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 1e-4;
        vec3 specular  = numerator / denom;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        vec3  radiance = Lcolor * scalerAttenuation;

        Lo += (kD * albedo / 3.14159265 + specular) * radiance * NdotL;
    }

    vec3 ambient = albedo * 0.03 * occlusion;
    vec3 lit     = ambient + Lo;

    // Add environment reflection on top of direct lighting
    lit += reflection;

    float transmission = getTransmission();

    // ─────────────────────────────────────────────
    // Refraction: screen-space (VIEW SPACE) + env fallback (WORLD SPACE)
    // ─────────────────────────────────────────────

    // Convert N and V to VIEW SPACE for screen-space distortion
    mat3 viewRot = mat3(ubo.view); // world→view rotation
    vec3 Nvs = normalize(viewRot * N);
    vec3 Vvs = normalize(viewRot * V);

    // Screen-space distortion uses view-space N/V
    vec2 refractUV = fragSceneUV + refractOffset(Nvs, Vvs);
    refractUV = clamp(refractUV, vec2(0.001), vec2(0.999));
    vec3 sceneRefract = texture(sceneColor, refractUV).rgb;

    // WORLD-SPACE refraction for environment
    float eta = 1.0 / uMat.ior;
    vec3 Renv = refract(-V, N, eta);           // world
    vec3 envRefract = sampleEnvironment(Renv); // world-space env lookup

    sceneRefract = applyVolumeToTransmission(sceneRefract);
    envRefract   = applyVolumeToTransmission(envRefract);

    float NdotV = max(dot(N, V), 0.0);
    vec3 Fview  = FresnelSchlick(NdotV, F0);
    Fview = mix(Fview, vec3(1.0), 0.2); // slightly stronger Fresnel

    // bias toward env so screen-space isn't tiny/swimmy
    float sceneWeight = transmission * 0.4;
    float envWeight   = transmission - sceneWeight;
    vec3 refractedColor =
        envRefract   * envWeight +
        sceneRefract * sceneWeight;

    // cap how much refraction can replace lit/specular
    float transWeight = transmission * (1.0 - Fview.r);
    transWeight = min(transWeight, 0.6);

    float maxReplace = 0.4; // never replace more than 40% of lit
    float w = min(transWeight, maxReplace);
    
    vec3 colorNoEmissive = lit * (1.0 - w) + refractedColor * w;


    // Tone map + gamma
    vec3 color = vec3(1.0) - exp(-colorNoEmissive * ubo.exposure);
    color = pow(color, vec3(1.0 / ubo.gamma));

    // Emissive after tone mapping
    color += emissive;

    // Alpha / WBOIT
    float alpha = clamp((baseColor.a * (1.0 - transmission)), 0.4, 1.0);
    if (alpha <= 0.0) {
        discard;
    }

    outAccum  = vec4(color * alpha, alpha);
    outReveal = 1.0 - alpha;
}
