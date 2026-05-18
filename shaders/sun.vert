#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec2 vUV;
out vec3 vViewNorm;  // view-space normal for limb-darkening

void main() {
    vUV      = aUV;
    // For a unit sphere, aPos == surface normal in model space.
    // Transform to view space so the fragment shader knows which parts
    // face the camera (view +Z) vs face away.
    vViewNorm = normalize(mat3(uView * uModel) * aPos);
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}
