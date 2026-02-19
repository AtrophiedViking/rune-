#version 450

layout(push_constant) uniform PushConstants {
    mat4 nodeMatrix;
    vec4 baseColorFactor;
    float metallicFactor;
    float roughnessFactor;
    int baseColorTextureSet;
    int physicalDescriptorTextureSet;
    int normalTextureSet;
    int occlusionTextureSet;
    int emissiveTextureSet;
    float alphaMask;
    float alphaMaskCutoff;
} pc;

layout(binding = 0) uniform UniformBufferObject {
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

// ─────────────────────────────────────────────
// Vertex Inputs
// ─────────────────────────────────────────────
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inTangent;

// ─────────────────────────────────────────────
// Vertex Outputs
// ─────────────────────────────────────────────
layout(location = 0) out vec3 fragColorVS;
layout(location = 1) out vec2 fragTexCoordVS;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 fragNormal;
layout(location = 4) out vec3 fragTangent;
layout(location = 5) out vec3 fragBitangent;

void main() {
    mat4 modelNode = pc.nodeMatrix;

    vec4 worldPos = modelNode * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(modelNode)));
    vec3 N = normalize(normalMatrix * inNormal);
    vec3 T = normalize(normalMatrix * inTangent.xyz);
    vec3 B = cross(N, T) * inTangent.w;

    fragNormal   = N;
    fragTangent  = T;
    fragBitangent = B;

    fragColorVS    = inColor;
    fragTexCoordVS = inTexCoord;

    gl_Position = ubo.proj * ubo.view * worldPos;
}
