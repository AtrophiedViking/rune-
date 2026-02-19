#include "render/gpu_material.h"
#include "scene/texture.h"

TexTransformGPU toGPU(const TextureTransform t)
{
    TexTransformGPU r{};
    r.offset_scale = glm::vec4(t.offset.x, t.offset.y,
        t.scale.x, t.scale.y);
    r.rot_center_tex = glm::vec4(t.rotation,
        t.center.x,
        t.center.y,
        float(t.texCoord));
    return r;
}
