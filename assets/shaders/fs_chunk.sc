$input v_normal, v_uv, v_color0

#include "bgfx_shader.sh"

SAMPLER2D(s_chunkAtlas, 0);
uniform vec4 u_sunDirection;
uniform vec4 u_sunLightColor;
uniform vec4 u_moonDirection;
uniform vec4 u_moonLightColor;
uniform vec4 u_ambientLight;
uniform vec4 u_chunkAnim;
uniform vec4 u_biomeHaze;
uniform vec4 u_biomeGrade;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 sunDirection = normalize(u_sunDirection.xyz);
    vec3 moonDirection = normalize(u_moonDirection.xyz);

    // .w carries raw visibility (see RendererFrame chunk uniforms).
    float sunVis = clamp(u_sunLightColor.w, 0.0, 1.0);
    float moonVis = clamp(u_moonLightColor.w, 0.0, 1.0);

    // Keep chunk lighting simple so the albedo reads cleanly instead of being heavily graded in shader.
    float sunDiffuse = max(dot(normal, sunDirection), 0.0);
    float moonDiffuse = max(dot(normal, moonDirection), 0.0);
    float up = max(normal.y, 0.0);
    // Slight hemispheric bias so topsoil and flat faces read a bit warmer (reference-style readability).
    float skyFill = 0.095 + up * 0.13 * (0.42 + 0.58 * sunVis);
    float hemi = mix(0.94, 1.06, up);

    vec3 lighting =
        u_ambientLight.rgb * hemi
        + u_sunLightColor.rgb * sunDiffuse
        + u_moonLightColor.rgb * moonDiffuse
        + vec3(skyFill, skyFill, skyFill);
    lighting = clamp(lighting, vec3(0.0, 0.0, 0.0), vec3(1.55, 1.55, 1.55));
    vec4 atlasColor = texture2D(s_chunkAtlas, v_uv);
    // Cutout alpha avoids writing depth for fully transparent texels (flowers/torch),
    // which otherwise creates "holes" where background terrain disappears.
    if (atlasColor.a < 0.1)
    {
        discard;
    }
    vec3 litColor = atlasColor.rgb * v_color0.rgb * lighting;
    litColor = clamp(litColor, 0.0, 1.0);
    gl_FragColor = vec4(litColor, atlasColor.a * v_color0.a);
}
