#pragma once

namespace shaders {

inline const char* kBackgroundVert = R"GLSL(
#version 330 core
out vec2 vUV;
void main() {
    vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
    vUV = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
)GLSL";

inline const char* kBackgroundFrag = R"GLSL(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform float uAspect;

void main() {
    vec3 top = vec3(0.005, 0.008, 0.014);
    vec3 bottom = vec3(0.027, 0.044, 0.068);
    vec3 color = mix(bottom, top, pow(vUV.y, 0.75));
    vec2 p = (vUV - 0.5) * vec2(uAspect, 1.0);
    float glow = exp(-dot(p, p) * 4.2);
    color += vec3(0.024, 0.060, 0.096) * glow;
    float vignette = smoothstep(1.15, 0.25, length(p));
    color *= 0.70 + 0.30 * vignette;
    FragColor = vec4(color, 1.0);
}
)GLSL";

inline const char* kFloorVert = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPosition;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uFloorY;
out vec3 vWorldPosition;

void main() {
    vec3 world = vec3(aPosition.x, uFloorY, aPosition.z);
    vWorldPosition = world;
    gl_Position = uProjection * uView * vec4(world, 1.0);
}
)GLSL";

inline const char* kFloorFrag = R"GLSL(
#version 330 core
in vec3 vWorldPosition;
out vec4 FragColor;
uniform vec3 uCameraPosition;
uniform vec2 uShadowCenter;
uniform float uShadowRadius;
uniform float uShadowStrength;

void main() {
    vec2 p = vWorldPosition.xz;
    vec2 fw = max(fwidth(p), vec2(0.0001));
    vec2 cell = abs(fract(p - 0.5) - 0.5) / fw;
    float minor = 1.0 - min(min(cell.x, cell.y), 1.0);

    vec2 majorCell = abs(fract(p / 5.0 - 0.5) - 0.5) / (fw / 5.0);
    float major = 1.0 - min(min(majorCell.x, majorCell.y), 1.0);

    float d = length(p);
    float fade = 1.0 - smoothstep(5.0, 22.0, d);
    float cameraDistance = length(vWorldPosition - uCameraPosition);
    fade *= 1.0 - smoothstep(10.0, 34.0, cameraDistance);

    float minorAlpha = minor * 0.26 * fade;
    float majorAlpha = major * 0.55 * fade;
    float gridAlpha = max(minorAlpha, majorAlpha);

    vec2 shadowDelta = p - uShadowCenter;
    float shadow = exp(-dot(shadowDelta, shadowDelta) / (uShadowRadius * uShadowRadius) * 2.4);
    float shadowAlpha = shadow * 0.36 * uShadowStrength;

    float alpha = gridAlpha + shadowAlpha * (1.0 - gridAlpha);
    if (alpha < 0.002) {
        discard;
    }

    vec3 minorColor = vec3(0.11, 0.30, 0.38);
    vec3 majorColor = vec3(0.24, 0.68, 0.82);
    vec3 gridColor = mix(minorColor, majorColor, majorAlpha / max(gridAlpha, 0.0001));
    vec3 color = gridColor * (gridAlpha / max(alpha, 0.0001));
    FragColor = vec4(color, alpha);
}
)GLSL";

inline const char* kMeshVert = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
out vec3 vWorldPosition;
out vec3 vNormal;
out vec2 vTexCoord;

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPosition = world.xyz;
    vNormal = uNormalMatrix * aNormal;
    vTexCoord = aTexCoord;
    gl_Position = uProjection * uView * world;
}
)GLSL";

inline const char* kMeshFrag = R"GLSL(
#version 330 core
in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vTexCoord;
out vec4 FragColor;
uniform vec3 uCameraPosition;
uniform vec3 uBaseColor;
uniform sampler2D uDiffuseTexture;
uniform int uHasTexture;
uniform vec3 uSpecularColor;
uniform vec3 uEmissiveColor;
uniform float uShininess;
uniform float uOpacity;

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec3 baseColor = uBaseColor;
    if (uHasTexture != 0) {
        vec3 texel = texture(uDiffuseTexture, vec2(vTexCoord.x, 1.0 - vTexCoord.y)).rgb;
        baseColor *= pow(texel, vec3(2.2));
    }
    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing) {
        N = -N;
    }
    vec3 V = normalize(uCameraPosition - vWorldPosition);
    vec3 keyDirection = normalize(vec3(0.45, 0.85, 0.55));
    vec3 fillDirection = normalize(vec3(-0.72, 0.32, 0.22));
    vec3 rimDirection = normalize(vec3(-0.25, 0.35, -0.90));

    float key = max(dot(N, keyDirection), 0.0);
    float fill = max(dot(N, fillDirection), 0.0);
    float rim = max(dot(N, rimDirection), 0.0);

    vec3 ambient = mix(vec3(0.034, 0.043, 0.058), vec3(0.16, 0.19, 0.24), N.y * 0.5 + 0.5);
    vec3 color = baseColor * (ambient
        + vec3(1.00, 0.86, 0.70) * key * 2.00
        + vec3(0.18, 0.38, 0.78) * fill * 0.85
        + vec3(0.12, 0.55, 0.78) * rim * 0.72);

    vec3 H = normalize(keyDirection + V);
    float specular = pow(max(dot(N, H), 0.0), max(uShininess, 2.0));
    color += vec3(1.0, 0.90, 0.74) * specular * uSpecularColor * 2.6;
    float broadSpecular = pow(max(dot(N, H), 0.0), 18.0);
    color += vec3(0.78, 0.88, 1.0) * broadSpecular * uSpecularColor * 0.55;

    vec3 reflected = reflect(-V, N);
    vec3 sky = mix(vec3(0.008, 0.015, 0.024), vec3(0.19, 0.36, 0.52),
                   smoothstep(-0.15, 0.85, reflected.y));
    float envFresnel = 0.035 + 0.965 * pow(1.0 - max(dot(N, V), 0.0), 5.0);
    color += sky * envFresnel * 0.85;

    vec3 H2 = normalize(fillDirection + V);
    color += vec3(0.35, 0.62, 1.0) * pow(max(dot(N, H2), 0.0), max(uShininess, 2.0)) * uSpecularColor * 0.45;

    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 4.0);
    color += vec3(0.20, 0.72, 1.0) * fresnel * 0.22;
    color += uEmissiveColor * 0.55;

    color = aces(color);
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, uOpacity);
}
)GLSL";

inline const char* kWireFrag = R"GLSL(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
uniform float uAlpha;
void main() {
    FragColor = vec4(uColor, uAlpha);
}
)GLSL";

inline const char* kUiColorVert = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;
uniform vec2 uViewport;
out vec4 vColor;
void main() {
    vec2 ndc = vec2(aPosition.x / uViewport.x * 2.0 - 1.0,
                    1.0 - aPosition.y / uViewport.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vColor = aColor;
}
)GLSL";

inline const char* kUiColorFrag = R"GLSL(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)GLSL";

inline const char* kUiTextVert = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
uniform vec2 uViewport;
out vec2 vTexCoord;
out vec4 vColor;
void main() {
    vec2 ndc = vec2(aPosition.x / uViewport.x * 2.0 - 1.0,
                    1.0 - aPosition.y / uViewport.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)GLSL";

inline const char* kUiTextFrag = R"GLSL(
#version 330 core
in vec2 vTexCoord;
in vec4 vColor;
out vec4 FragColor;
uniform sampler2D uFont;
void main() {
    float alpha = texture(uFont, vTexCoord).r * vColor.a;
    if (alpha < 0.004) {
        discard;
    }
    FragColor = vec4(vColor.rgb, alpha);
}
)GLSL";

} // namespace shaders
