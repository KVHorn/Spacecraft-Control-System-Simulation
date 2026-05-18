#version 330 core
in vec3 vWorldPos;
in vec2 vUV;

uniform sampler2D uTexture;
uniform vec3 uLightPos;
uniform float uAmbient;

out vec4 FragColor;

void main() {
    // UV.x maps radially from inner to outer - sample the ring strip
    vec4 tex = texture(uTexture, vUV);

    // Simple lighting: distance falloff from sun direction (rings lit but self-shadowed a bit)
    vec3 L = normalize(uLightPos - vWorldPos);
    float litFactor = max(abs(L.y), 0.2); // rings are flat; abs() so both faces lit

    vec3 color = tex.rgb * (litFactor + uAmbient);
    FragColor = vec4(color, tex.a);
}
