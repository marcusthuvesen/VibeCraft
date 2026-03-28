$input v_normal, v_uv, v_color0

#include "bgfx_shader.sh"

SAMPLER2D(s_logo, 0);

void main()
{
    // PNGs often use an opaque black matte; treat dark pixels as transparent so the logo
    // sits cleanly on the menu clear color (true alpha in the file still works).
    vec4 color = texture2D(s_logo, v_uv);
    float lit = max(max(color.r, color.g), color.b);
    float a = color.a * smoothstep(0.03, 0.11, lit);
    gl_FragColor = vec4(color.rgb, a);
}
