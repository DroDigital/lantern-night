#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform float intensity;
out vec4 finalColor;
void main() {
    vec4 t = texture(texture0, fragTexCoord) * fragColor;
    finalColor = vec4(t.rgb * t.a * intensity, 1.0);
}
