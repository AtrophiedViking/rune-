#version 450

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

layout(set = 0, binding = 2) uniform sampler2D sceneColor;
layout(set = 0, binding = 3) uniform sampler2D sceneDepth;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4  outSSRColor;
layout(location = 1) out float outSSRMask;

// --------------------------------------------------------
// Helpers
// --------------------------------------------------------
vec2 projectToUV(vec3 viewPos)
{
    vec4 clip = ubo.proj * vec4(viewPos, 1.0);
    vec3 ndc  = clip.xyz / clip.w;
    return ndc.xy * 0.5 + 0.5;
}

float depthFromView(vec3 viewPos)
{
    vec4 clip = ubo.proj * vec4(viewPos, 1.0);
    float ndcZ = clip.z / clip.w;
    return ndcZ * 0.5 + 0.5;
}

vec3 reconstructViewPos(vec2 uv, float depth01)
{
    float z = depth01 * 2.0 - 1.0;
    vec2 ndcXY = uv * 2.0 - 1.0;

    vec4 clip = vec4(ndcXY, z, 1.0);
    vec4 view = inverse(ubo.proj) * clip;
    return view.xyz / view.w;
}

// --------------------------------------------------------
// SSR raymarch
// --------------------------------------------------------
vec3 computeSSR(vec3 Pvs, vec3 Vvs, float roughness, out float mask)
{
    vec3 Nvs = vec3(0.0, 0.0, 1.0);
    vec3 Rvs = normalize(reflect(-Vvs, Nvs));

    if (Rvs.z >= -0.05) {
        mask = 0.0;
        return vec3(0.0);
    }

    float maxDistance = 50.0;
    float stepSize    = mix(0.1, 1.0, roughness);
    float thickness   = 0.1;

    vec3 rayPos = Pvs;
    vec3 rayDir = Rvs;

    for (int i = 0; i < 64; ++i)
    {
        rayPos += rayDir * stepSize;

        vec2 suv = projectToUV(rayPos);
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0)
            break;

        float sceneD = texture(sceneDepth, suv).r;
        float rayD   = depthFromView(rayPos);

        if (rayD > sceneD - thickness)
        {
            vec3 a = rayPos - rayDir * stepSize;
            vec3 b = rayPos;

            for (int j = 0; j < 8; ++j)
            {
                vec3 mid = 0.5 * (a + b);
                float midD = depthFromView(mid);
                vec2 midUV = projectToUV(mid);
                float sceneMidD = texture(sceneDepth, midUV).r;

                if (midD > sceneMidD - thickness)
                    b = mid;
                else
                    a = mid;
            }

            vec2 finalUV = projectToUV(b);
            mask = 1.0;
            return texture(sceneColor, finalUV).rgb;
        }

        if (length(rayPos - Pvs) > maxDistance)
            break;
    }

    mask = 0.0;
    return vec3(0.0);
}

void main()
{
    float depth01 = texture(sceneDepth, uv).r;

    if (depth01 >= 1.0)
    {
        outSSRColor = vec4(0.0);
        outSSRMask  = 0.0;
        return;
    }

    vec3 Pvs = reconstructViewPos(uv, depth01);
    vec3 Vvs = normalize(-Pvs);

    float roughness = 0.2;

    float mask;
    vec3 color = computeSSR(Pvs, Vvs, roughness, mask);

    outSSRColor = vec4(color, 1.0);
    outSSRMask  = mask;
}
