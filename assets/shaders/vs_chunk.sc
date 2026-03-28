$input a_position, a_normal, a_texcoord0, a_color0
$output v_normal, v_uv, v_color0

#include "bgfx_shader.sh"

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_normal = a_normal;
    v_uv = a_texcoord0;
    v_color0 = a_color0;
}
