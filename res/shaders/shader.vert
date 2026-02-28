#version 450

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




layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;

    vec4 lightPositions[4];
    vec4 lightColors[4];
    vec4 camPos;

    float exposure;
    float gamma;
    float prefilteredCubeMipLevels;
    float scaleIBLAmbient;
} ubo;
layout(set = 0, binding = 1) uniform samplerCube envMap;
layout(set = 0, binding = 2) uniform sampler2D sceneColor;
layout(set = 0, binding = 3) uniform sampler2D sceneDepth;


// ─────────────────────────────────────────────
// Vertex Inputs
// ─────────────────────────────────────────────
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord0;
layout(location = 3) in vec2 inTexCoord1;
layout(location = 4) in vec3 inNormal;
layout(location = 5) in vec4 inTangent;

// ─────────────────────────────────────────────
// Vertex Outputs
// ─────────────────────────────────────────────
layout(location = 0) out vec3 fragColorVS;
layout(location = 1) out vec2 fragTexCoordVS0;
layout(location = 2) out vec2 fragTexCoordVS1;
layout(location = 3) out vec3 fragWorldPos;
layout(location = 4) out vec3 fragNormal;
layout(location = 5) out vec3 fragTangent;
layout(location = 6) out vec3 fragBitangent;
layout(location = 7) out vec2 fragSceneUV;

void main() {
    mat4 modelNode = ubo.model * pc.nodeMatrix;

    vec4 worldPos = modelNode * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(modelNode)));
    vec3 N = normalize(normalMatrix * inNormal);
    vec3 T = normalize(normalMatrix * inTangent.xyz);
    T = normalize(T - N * dot(N, T));
    vec3 B = cross(N, T) * inTangent.w;

    fragNormal   = N;
    fragTangent  = T;
    fragBitangent = B;

    fragColorVS    = inColor;
    fragTexCoordVS0 = inTexCoord0;
    fragTexCoordVS1 = inTexCoord1;

     vec4 clip = ubo.proj * ubo.view * worldPos;
    vec3 ndc  = clip.xyz / clip.w;
    fragSceneUV = ndc.xy * 0.5 + 0.5;

    gl_Position = clip;
}
