#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

uniform sampler2D uTexture;
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform float uAmbient;

out vec4 FragColor;

void main() {
    vec3 albedo = texture(uTexture, vUV).rgb;

    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);
    float diff = max(dot(N, L), 0.0);

    // Tiny ambient so dark side isn't pure black but still clearly night
    vec3 color = albedo * (uLightColor * diff + vec3(uAmbient));

    FragColor = vec4(color, 1.0);
}
