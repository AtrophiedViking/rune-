#version 450

layout(set = 0, binding = 0) uniform sampler2D sceneColor;   // opaque
layout(set = 0, binding = 1) uniform sampler2D transAccum0;  // front accum
layout(set = 0, binding = 2) uniform sampler2D transReveal0; // front reveal
layout(set = 0, binding = 3) uniform sampler2D transAccum1;  // back accum
layout(set = 0, binding = 4) uniform sampler2D transReveal1; // back reveal

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColor;

void main()
{
    // Opaque base
    vec3 opaque = texture(sceneColor, uv).rgb;

    // --- Front layer ---
    vec4 accum0 = texture(transAccum0, uv);
    float rev0  = texture(transReveal0, uv).r;

    // --- Back layer ---
    vec4 accum1 = texture(transAccum1, uv);
    float rev1  = texture(transReveal1, uv).r;

    // If nothing in either layer, return opaque
    if (accum0.a <= 0.0001 && accum1.a <= 0.0001) {
        outColor = vec4(opaque, 1.0);
        return;
    }

    // Reconstruct colors
    vec3 trans0 = accum0.rgb / max(rev0, 1e-4);  // front
    vec3 trans1 = accum1.rgb / max(rev1, 1e-4);  // back

    float alpha0 = clamp(accum0.a, 0.0, 1.0);
    float alpha1 = clamp(accum1.a, 0.0, 1.0);

    // Composite back layer over opaque
    vec3 behind = opaque * (1.0 - alpha1) + trans1;

    // Composite front layer over that
    vec3 finalColor = behind * (1.0 - alpha0) + trans0;

    outColor = vec4(finalColor, 1.0);
}

