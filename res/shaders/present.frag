#version 450

layout(set = 0, binding = 0) uniform sampler2D sceneColor;   // opaque
layout(set = 0, binding = 1) uniform sampler2D transAccum;   // accum
layout(set = 0, binding = 2) uniform sampler2D transReveal;  // reveal

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 opaque = texture(sceneColor, uv).rgb;

    vec4 accum  = texture(transAccum,  uv);
    float reveal = texture(transReveal, uv).r;

    // No transparent contribution
    if (accum.a <= 0.0001) {
        outColor = vec4(opaque, 1.0);
        return;
    }

    // Reconstruct transparent color
    vec3 transColor = accum.rgb / max(accum.a, 1e-4);
    float transAlpha = 1.0 - reveal;

    // Correct WBOIT composite
    vec3 result = transColor * transAlpha + opaque * reveal;

    outColor = vec4(result, 1.0);
}
