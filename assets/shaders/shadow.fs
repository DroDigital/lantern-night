#version 330
// Lantern shadow pass: store distance to the light (normalised by the far plane).
in vec3 fragPosition;
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec3 shadowLightPos;
uniform float shadowFar;
out vec4 finalColor;
void main() {
    if (texture(texture0, fragTexCoord).a < 0.35) discard;
    finalColor = vec4(length(fragPosition - shadowLightPos) / shadowFar, 0.0, 0.0, 1.0);
}
