$input v_normal, v_uv, v_color0

#include "bgfx_shader.sh"

SAMPLER2D(s_uiAtlas, 0);

void main()
{
    gl_FragColor = texture2D(s_uiAtlas, v_uv);
}
