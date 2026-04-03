$input v_normal, v_uv, v_color0

#include "bgfx_shader.sh"

SAMPLER2D(s_uiAtlas, 0);

void main()
{
    // Straight multiply — keep minimal so UI/menu cannot drift into bogus channels on
    // unusual GPUs (saturation/gamma boosts lived in fs_chunk-style tuning).
    gl_FragColor = texture2D(s_uiAtlas, v_uv) * v_color0;
}
