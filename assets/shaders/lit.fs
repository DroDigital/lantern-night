#version 330
// One lit shader for the KayKit gradient atlas and Quaternius props.
// Linear-space lighting into an HDR target; tonemapping happens in the post pass.
#define MAX_LIGHTS 8
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 ambientColor;
uniform int lightCount;
uniform vec3 lightPos[MAX_LIGHTS];
uniform vec3 lightColor[MAX_LIGHTS];
uniform float lightRange[MAX_LIGHTS];
uniform vec3 fogColor;
uniform float fogDensity;
uniform float fogHeight;
uniform float fogHeightFalloff;
uniform vec3 emissive;
uniform float hitFlash;
uniform vec3 rimColor;
uniform float rimStrength;
uniform float minLight;
uniform float lightFalloff;
uniform float opacity;
uniform float time;
uniform samplerCube shadowCube;
uniform int shadowEnabled;
uniform float shadowFar;

float LanternShadow(vec3 lightPos) {
    vec3 d = fragPosition - lightPos;
    float cur = length(d);
    float bias = 0.04 + cur * 0.012;
    float lit = 0.0;
    vec3 offs[4] = vec3[](vec3(0.6, 0.6, 0.6), vec3(-0.6, -0.6, 0.6), vec3(-0.6, 0.6, -0.6), vec3(0.6, -0.6, -0.6));
    float r = 0.012 * cur;
    for (int k = 0; k < 4; ++k) {
        float closest = texture(shadowCube, d + offs[k] * r).r * shadowFar;
        lit += (cur - bias > closest) ? 0.0 : 1.0;
    }
    return mix(0.18, 1.0, lit * 0.25);
}
out vec4 finalColor;

void main() {
    vec4 tex = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    if (tex.a < 0.35) discard;
    vec3 albedo = pow(tex.rgb, vec3(2.2));
    vec3 N = normalize(fragNormal);
    if (!gl_FrontFacing) N = -N;
    vec3 toEye = viewPos - fragPosition;
    float dist = length(toEye);
    vec3 V = toEye / max(dist, 0.0001);

    vec3 light = ambientColor * (0.75 + 0.25 * N.y);
    vec3 spec = vec3(0.0);
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (i >= lightCount) break;
        vec3 L = lightPos[i] - fragPosition;
        float d = length(L);
        float r = max(lightRange[i], 0.001);
        if (d >= r) continue;
        L /= max(d, 0.0001);
        float x = clamp(1.0 - pow(d / r, 4.0), 0.0, 1.0);
        float att = (x * x) / (1.0 + lightFalloff * d * d);
        // wrapped diffuse keeps the chunky shapes soft and readable
        float ndl = clamp(dot(N, L) * 0.7 + 0.3, 0.0, 1.0);
        if (i == 0 && shadowEnabled == 1) att *= LanternShadow(lightPos[0]);
        light += lightColor[i] * att * ndl;
        if (opacity < 0.99) {
            // Water: sharp glints of the lights, gently rippled.
            vec3 Nw = normalize(N + vec3(sin(fragPosition.x * 3.1 + fragPosition.z * 1.7 + time * 1.3),
                                         0.0, cos(fragPosition.z * 2.7 - fragPosition.x * 1.3 + time * 1.1)) * 0.06);
            vec3 H = normalize(L + V);
            spec += lightColor[i] * att * pow(max(dot(Nw, H), 0.0), 48.0) * 5.0;
        }
    }
    vec3 col = albedo * light + spec;
    float rim = pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 3.0);
    col += rimColor * rim * rimStrength;
    col = max(col, albedo * minLight);
    col += emissive;

    float fogD = 1.0 - exp(-dist * fogDensity);
    float fogH = clamp(exp(-(fragPosition.y - fogHeight) * fogHeightFalloff), 0.0, 1.0);
    float fog = clamp(fogD * (0.55 + 0.45 * fogH), 0.0, 1.0);
    col = mix(col, fogColor, fog);
    col = mix(col, vec3(1.4), clamp(hitFlash, 0.0, 1.0) * 0.55);
    finalColor = vec4(col, opacity <= 0.0 ? 1.0 : opacity);
}
