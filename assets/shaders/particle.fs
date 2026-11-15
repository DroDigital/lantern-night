#version 330
in vec2 uv;
in vec4 color;
out vec4 finalColor;
void main() {
    float d = length(uv - 0.5) * 2.0;
    float a = clamp(1.0 - d, 0.0, 1.0);
    a = a * a;
    finalColor = vec4(color.rgb * color.a * a, 1.0);
}
