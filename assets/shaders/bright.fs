#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float threshold;
uniform float knee;
out vec4 finalColor;
void main() {
    vec3 c = texture(texture0, fragTexCoord).rgb;
    float br = max(c.r, max(c.g, c.b));
    float soft = clamp(br - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.0001);
    float contrib = max(soft, br - threshold) / max(br, 0.0001);
    finalColor = vec4(c * contrib, 1.0);
}
