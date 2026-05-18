#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 uView;
uniform mat4 uProj;

out vec3 vDir;

void main() {
    vDir = aPos;
    // Strip translation from view so skybox follows camera
    mat4 rotView = mat4(mat3(uView));
    vec4 pos = uProj * rotView * vec4(aPos, 1.0);
    // Force depth to be at far plane
    gl_Position = pos.xyww;
}
