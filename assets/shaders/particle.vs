#version 330
// Instanced camera-facing billboards. Per-vertex: corner. Per-instance: position+size, colour.
in vec2 corner;
in vec4 instPosSize;
in vec4 instColor;
uniform mat4 viewProj;
uniform vec3 camRight;
uniform vec3 camUp;
out vec2 uv;
out vec4 color;
void main() {
    vec3 p = instPosSize.xyz + (camRight * corner.x + camUp * corner.y) * instPosSize.w;
    uv = corner + 0.5;
    color = instColor;
    gl_Position = viewProj * vec4(p, 1.0);
}
