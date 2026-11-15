#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;   // HDR scene
uniform sampler2D bloomA;     // half-res blur
uniform sampler2D bloomB;     // quarter-res blur
uniform float bloomStrength;
uniform float exposure;
uniform float vignette;       // 0..1 strength
uniform float vignetteRadius;
uniform vec3 vignetteColor;
uniform vec3 shadowTint;
uniform float saturation;
uniform float flash;          // full-screen flash (parry)
uniform vec3 flashColor;
uniform int tonemapper;       // 0 = ACES, 1 = AgX
out vec4 finalColor;

vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Minimal AgX (Troy Sobotka's curve, polynomial fit by Benjamin Wrensch).
vec3 agxContrast(vec3 x) {
    vec3 x2 = x * x; vec3 x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}
vec3 agx(vec3 c) {
    const mat3 inMat = mat3(0.842479062253094, 0.0423282422610123, 0.0423756549057051,
                            0.0784335999999992, 0.878468636469772, 0.0784336,
                            0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    const mat3 outMat = mat3(1.19687900512017, -0.0528968517574562, -0.0529716355144438,
                             -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
                             -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
    const float minEv = -12.47393, maxEv = 4.026069;
    c = inMat * c;
    c = clamp(log2(max(c, 1e-10)), minEv, maxEv);
    c = (c - minEv) / (maxEv - minEv);
    c = agxContrast(c);
    c = outMat * c;
    return pow(max(c, 0.0), vec3(2.2));  // back to linear for the shared gamma step
}

void main() {
    vec2 uv = fragTexCoord;
    vec3 hdr = texture(texture0, uv).rgb;
    vec3 bloom = texture(bloomA, uv).rgb * 0.6 + texture(bloomB, uv).rgb * 0.4;
    vec3 c = (hdr + bloom * bloomStrength) * exposure;
    c = (tonemapper == 1) ? agx(c) : aces(c);
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(vec3(luma), c, saturation);
    c += shadowTint * (1.0 - smoothstep(0.0, 0.25, luma)) * 0.04;
    vec2 d = uv - 0.5;
    d.x *= 1.25;
    float v = smoothstep(vignetteRadius, vignetteRadius - 0.45, length(d));
    c = mix(vignetteColor, c, mix(1.0, v, vignette));
    c = mix(c, flashColor, clamp(flash, 0.0, 1.0));
    c = pow(max(c, 0.0), vec3(1.0 / 2.2));
    finalColor = vec4(c, 1.0);
}
