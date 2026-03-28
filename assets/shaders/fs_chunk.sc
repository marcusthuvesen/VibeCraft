$input v_normal, v_color0

#include "bgfx_shader.sh"

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 lightDir = normalize(vec3(0.35, 0.8, 0.2));
    float diffuse = max(dot(normal, lightDir), 0.0);
    float lighting = 0.3 + diffuse * 0.7;
    gl_FragColor = vec4(v_color0.rgb * lighting, v_color0.a);
}
