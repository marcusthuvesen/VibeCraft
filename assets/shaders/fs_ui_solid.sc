$input v_normal, v_uv, v_color0

#include "bgfx_shader.sh"

SAMPLER2D(s_uiAtlas, 0);

void main()
{
    // Keep the atlas bound for backends that require a sampled texture; tint is entirely from v_color0.
    gl_FragColor = v_color0 + texture2D(s_uiAtlas, v_uv) * 0.00001;
}
