#version 330 core
in vec2  vUV;
in vec3  vViewNorm;

uniform sampler2D uTexture;
uniform float     uEmissiveBoost;

out vec4 FragColor;

void main() {
    vec3 texCol = texture(uTexture, vUV).rgb;

    // Limb darkening: fragments facing the camera (view +Z) are brighter.
    // This makes the centre of the visible disk always the brightest point,
    // regardless of how the sphere is oriented or how the camera orbits it,
    // eliminating the "spinning texture" artefact.
    float facing = max(0.0, vViewNorm.z);
    float limb   = 0.35 + 0.65 * pow(facing, 0.35);

    vec3 color = texCol * uEmissiveBoost * limb;
    FragColor  = vec4(color, 1.0);
}
