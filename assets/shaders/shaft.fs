#version 330
// Fake light shaft: additive cone, bright at the apex (uv.y=0) and fading to the base.
in vec2 fragTexCoord;
in vec4 fragColor;
uniform float intensity;
out vec4 finalColor;
void main() {
    float along = clamp(fragTexCoord.y, 0.0, 1.0);
    float edge = 1.0 - abs(fragTexCoord.x * 2.0 - 1.0);
    float a = pow(1.0 - along, 1.6) * smoothstep(0.0, 0.6, edge);
    finalColor = vec4(fragColor.rgb * a * intensity, 1.0);
}
