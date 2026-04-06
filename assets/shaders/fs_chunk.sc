$input v_normal, v_uv, v_color0, v_worldPos

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
uniform vec4 u_cameraPos;
uniform vec4 u_chunkFog;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 sunDirection = normalize(u_sunDirection.xyz);
    vec3 moonDirection = normalize(u_moonDirection.xyz);

    // .w carries raw visibility (see RendererFrame chunk uniforms).
    float sunVis = clamp(u_sunLightColor.w, 0.0, 1.0);
    float moonVis = clamp(u_moonLightColor.w, 0.0, 1.0);

    // Minecraft-like readability: stronger face-based shading, plus weather-aware daylight contrast.
    float sunDiffuse = max(dot(normal, sunDirection), 0.0);
    float moonDiffuse = max(dot(normal, moonDirection), 0.0);
    float rain = clamp(u_chunkAnim.y, 0.0, 1.0);

    float sideAxis = max(abs(normal.x), abs(normal.z));
    float faceShade = 0.80;
    if (normal.y > 0.5)
    {
        faceShade = 1.00;
    }
    else if (normal.y < -0.5)
    {
        faceShade = 0.56;
    }
    else if (sideAxis > 0.5)
    {
        // Mild directional asymmetry makes crowns/terrain read less flat.
        faceShade = abs(normal.z) > abs(normal.x) ? 0.76 : 0.68;
    }

    float sunTerm = (0.18 + 0.82 * sunDiffuse) * (1.0 - rain * 0.35);
    float moonTerm = (0.14 + 0.86 * moonDiffuse) * (1.0 - rain * 0.12);
    float skyFill = mix(0.03, 0.11, sunVis) + mix(0.0, 0.03, moonVis);

    vec3 lighting =
        u_ambientLight.rgb
        + u_sunLightColor.rgb * sunTerm
        + u_moonLightColor.rgb * moonTerm
        + vec3(skyFill, skyFill, skyFill);
    lighting *= faceShade;
    lighting = clamp(lighting, vec3(0.0, 0.0, 0.0), vec3(1.45, 1.45, 1.45));
    vec4 atlasColor = texture2D(s_chunkAtlas, v_uv);
    // Cutout alpha avoids writing depth for fully transparent texels (flowers/torch),
    // which otherwise creates "holes" where background terrain disappears.
    if (atlasColor.a < 0.1)
    {
        discard;
    }
    vec3 litColor = atlasColor.rgb * v_color0.rgb * lighting;
    litColor = clamp(litColor, 0.0, 1.0);
    // Horizontal distance fog (Minecraft-style): hold terrain clarity nearby, then ramp toward the
    // horizon color until the far draw cutoff disappears into haze instead of hard-cutting.
    float distH = length(v_worldPos.xz - u_cameraPos.xz);
    float hazeStrength = clamp(u_biomeHaze.w, 0.0, 1.0);
    float fogStart = mix(u_chunkFog.x, u_chunkFog.x * 0.58, hazeStrength);
    float fogBase = smoothstep(fogStart, u_chunkFog.y, distH);
    float fogAmt = clamp(pow(fogBase, mix(1.35, 0.95, hazeStrength)), 0.0, 1.0);
    vec3 fogColor = u_biomeHaze.rgb;
    litColor = mix(litColor, fogColor, fogAmt);
    gl_FragColor = vec4(litColor, atlasColor.a * v_color0.a);
}
