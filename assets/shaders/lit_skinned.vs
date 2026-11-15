#version 330
#define MAX_BONES 64
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform mat4 boneMatrices[MAX_BONES];
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec3 fragNormal;
out vec4 fragColor;
void main() {
    ivec4 id = ivec4(vertexBoneIndices);
    vec4 w = vertexBoneWeights;
    float sum = w.x + w.y + w.z + w.w;
    if (sum < 0.0001) { w = vec4(1.0, 0.0, 0.0, 0.0); id = ivec4(0); }
    mat4 skin = boneMatrices[id.x] * w.x + boneMatrices[id.y] * w.y + boneMatrices[id.z] * w.z + boneMatrices[id.w] * w.w;
    vec4 pos = skin * vec4(vertexPosition, 1.0);
    vec3 nrm = mat3(skin) * vertexNormal;
    fragPosition = vec3(matModel * pos);
    fragTexCoord = vertexTexCoord;
    fragNormal = normalize(vec3(matNormal * vec4(nrm, 0.0)));
    fragColor = vertexColor;
    gl_Position = mvp * pos;
}
