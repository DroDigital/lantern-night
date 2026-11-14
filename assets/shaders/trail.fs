#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform float intensity;
out vec4 finalColor;
void main() {
    // u = age along the ribbon (0 newest), v = across (0 base, 1 tip)
    float a = fragColor.a * (1.0 - fragTexCoord.x) * smoothstep(0.0, 0.35, fragTexCoord.y);
    finalColor = vec4(fragColor.rgb * a * intensity, 1.0);
}
