#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vLocal;      // model-space surface normal (navball frame)
out vec3 vWorldNorm;  // world-space surface normal (for camera-facing shading)

void main() {
    vLocal    = normalize(aPos);
    vWorldNorm = normalize(mat3(uModel) * aPos);
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}
