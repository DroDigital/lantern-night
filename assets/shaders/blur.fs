#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 direction;   // texel step * axis
out vec4 finalColor;
void main() {
    float w[5] = float[](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);
    vec3 c = texture(texture0, fragTexCoord).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        c += texture(texture0, fragTexCoord + direction * float(i)).rgb * w[i];
        c += texture(texture0, fragTexCoord - direction * float(i)).rgb * w[i];
    }
    finalColor = vec4(c, 1.0);
}
