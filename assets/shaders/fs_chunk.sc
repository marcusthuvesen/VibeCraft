$input v_normal, v_uv, v_color0

#include "bgfx_shader.sh"

SAMPLER2D(s_chunkAtlas, 0);

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 lightDir = normalize(vec3(0.35, 0.8, 0.2));
    float diffuse = max(dot(normal, lightDir), 0.0);
    float lighting = 0.3 + diffuse * 0.7;
    vec4 atlasColor = texture2D(s_chunkAtlas, v_uv);
    vec3 litColor = atlasColor.rgb * v_color0.rgb * lighting;
    gl_FragColor = vec4(litColor, v_color0.a);
}
