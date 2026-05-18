#version 330 core
in vec3 vDir;
out vec4 FragColor;

uniform sampler2D uMilkyway;
uniform float     uBrightness;  // 0-2, default 0.85
uniform float     uYawDeg;      // rotation around Y (horizontal pan)
uniform float     uPitchDeg;    // rotation around X (tilt up/down)
uniform float     uRollDeg;     // rotation around Z (tilt left/right)

#define PI 3.14159265

// Rotation matrices applied to the sample direction so the panorama can be
// re-oriented without editing the image file.
mat3 rotX(float a) {
    float c = cos(a), s = sin(a);
    return mat3(1.0, 0.0, 0.0,
                0.0,   c,  -s,
                0.0,   s,   c);
}
mat3 rotY(float a) {
    float c = cos(a), s = sin(a);
    return mat3(  c, 0.0,   s,
                0.0, 1.0, 0.0,
                 -s, 0.0,   c);
}
mat3 rotZ(float a) {
    float c = cos(a), s = sin(a);
    return mat3(  c,  -s, 0.0,
                  s,   c, 0.0,
                0.0, 0.0, 1.0);
}

void main() {
    // Apply yaw → pitch → roll rotations to the view direction.
    vec3 d = normalize(vDir);
    d = rotY(radians(uYawDeg))   * d;
    d = rotX(radians(uPitchDeg)) * d;
    d = rotZ(radians(uRollDeg))  * d;

    // U: mirror horizontally for correct inside-sphere orientation.
    // V: stbi flips vertically on load so GL v=1 = top of file = north pole;
    //    use +asin so looking up (d.y=1) samples v=1.0 (north pole).
    float u = 0.5 - atan(d.z, d.x) / (2.0 * PI);
    float v = 0.5 + asin(clamp(d.y, -1.0, 1.0)) / PI;
    u = fract(u);

    vec3 color = texture(uMilkyway, vec2(u, v)).rgb;
    color *= uBrightness;
    FragColor = vec4(color, 1.0);
}
