#version 450

layout(set = 0, binding = 0) uniform UBO {
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

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

void main() {
    // NDC → view-space direction
    vec4 ndc = vec4(uv, 1.0, 1.0);
    vec4 viewDir = inverse(ubo.proj) * ndc;
    vec3 dirVS = normalize(viewDir.xyz);

    // Extract camera rotation (world→view)
    mat3 camRot = mat3(ubo.view);

    // Inverse rotation (view→world)
    mat3 invRot = transpose(camRot);

    // Convert view-space direction → world-space direction
    vec3 dirWS = normalize(invRot * dirVS);

    outColor = texture(envMap, dirWS);
}
