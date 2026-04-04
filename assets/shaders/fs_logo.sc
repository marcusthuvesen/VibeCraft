$input v_normal, v_uv, v_color0

#include "bgfx_shader.sh"

SAMPLER2D(s_logo, 0);

void main()
{
    // PNGs often use an opaque black matte; treat dark pixels as transparent so the logo
    // sits cleanly on the menu clear color (true alpha in the file still works).
    // Some exports also carry magenta matte/chroma in fully-opaque texels.
    vec4 color = texture2D(s_logo, v_uv);
    float lit = max(max(color.r, color.g), color.b);
    float darkKeep = smoothstep(0.03, 0.11, lit);
    float magentaDist = distance(color.rgb, vec3(1.0, 0.0, 1.0));
    float magentaKeep = smoothstep(0.14, 0.32, magentaDist);
    float a = color.a * darkKeep * magentaKeep;
    gl_FragColor = vec4(color.rgb, a);
}
