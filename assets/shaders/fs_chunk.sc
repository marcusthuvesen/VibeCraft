$input v_normal, v_uv, v_color0

#include "bgfx_shader.sh"

SAMPLER2D(s_chunkAtlas, 0);
uniform vec4 u_sunDirection;
uniform vec4 u_sunLightColor;
uniform vec4 u_moonDirection;
uniform vec4 u_moonLightColor;
uniform vec4 u_ambientLight;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 sunDirection = normalize(u_sunDirection.xyz);
    vec3 moonDirection = normalize(u_moonDirection.xyz);
    float sunDiffuse = max(dot(normal, sunDirection), 0.0);
    float moonDiffuse = max(dot(normal, moonDirection), 0.0);
    vec3 lighting =
        u_ambientLight.rgb
        + u_sunLightColor.rgb * sunDiffuse
        + u_moonLightColor.rgb * moonDiffuse;
    lighting = clamp(lighting, vec3(0.0, 0.0, 0.0), vec3(1.6, 1.6, 1.6));
    vec4 atlasColor = texture2D(s_chunkAtlas, v_uv);
    vec3 litColor = atlasColor.rgb * v_color0.rgb * lighting;
    gl_FragColor = vec4(litColor, v_color0.a);
}
