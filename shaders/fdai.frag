#version 330 core
// Navball (FDAI) fragment shader.
//
// The sphere's model frame = spacecraft body frame, oriented so model +Z = nose
// (forward/prograde direction when unrotated). The camera always sits along
// world +Z, so n.z in model space measures "facing the nose" — used for the
// center highlight that mimics the Python scanline's lz term.
//
// uRadPole : RAD (radial-outward) direction expressed in navball/model space.

in vec3 vLocal;      // model-space position on unit sphere
in vec3 vWorldNorm;  // world-space surface normal

uniform vec3 uRadPole;  // RAD (radial-outward) direction in navball (model) frame
uniform vec3 uProDir;  // Prograde (velocity) direction in navball (model) frame

out vec4 FragColor;

#define PI 3.14159265

void main() {
    vec3 n = normalize(vLocal);

    // ---- Hemisphere coloring: blue = RAD side, orange = ARAD side ----
    float hemi = dot(n, normalize(uRadPole));
    vec3 blueCol   = vec3(0.107, 0.353, 0.651);   // radial-outward hemisphere
    vec3 orangeCol = vec3(0.659, 0.416, 0.106);   // radial-inward hemisphere
    vec3 col = (hemi >= 0.0) ? blueCol : orangeCol;

    // ---- Grid lines in LVLH orbital frame ----
    // Build an orthonormal LVLH frame from the two uniforms so the grid stays
    // fixed to the orbital reference regardless of spacecraft attitude.
    vec3 upDir  = normalize(uRadPole);
    vec3 fwdDir = normalize(uProDir - dot(uProDir, upDir) * upDir);
    vec3 rtDir  = cross(upDir, fwdDir);

    float sinLat  = clamp(dot(n, upDir), -1.0, 1.0);
    float latDeg  = degrees(asin(sinLat));
    float lat10   = abs(mod(latDeg + 90.0, 10.0) - 5.0);
    float lat30   = abs(mod(latDeg + 90.0, 30.0) - 15.0);
    if (lat10 > 4.82)  col = mix(col, vec3(0.80), 0.50);
    if (lat30 > 14.68) col = mix(col, vec3(0.95), 0.72);

    // Longitude lines every 15°, bold every 90° (0° = prograde, 180° = retrograde)
    float lenH   = length(n - sinLat * upDir);
    float lonDeg = 0.0;
    if (lenH > 0.001) {
        vec3 nH  = (n - sinLat * upDir) / lenH;
        lonDeg   = degrees(atan(dot(nH, rtDir), dot(nH, fwdDir)));
    }
    float lon15  = abs(mod(lonDeg + 180.0, 15.0) - 7.5);
    float lon90  = abs(mod(lonDeg + 180.0, 90.0) - 45.0);
    if (lon15 > 7.23) col = mix(col, vec3(0.75), 0.38);
    if (lon90 > 44.5) col = mix(col, vec3(0.90), 0.65);

    // ---- RAD/ARAD equatorial horizon band (white line at the boundary) ----
    if (abs(hemi) < 0.022) col = vec3(0.93);

    // ---- Spherical shading: highlight at the nose (model +Z = center of navball) ----
    // n.z in model space = how much the surface faces the nose direction.
    // Multiplied by camera-facing component so the lit spot is at the visible center.
    float camFace = clamp(vWorldNorm.z * 0.5 + 0.5, 0.0, 1.0);  // world +Z = toward camera
    float noseFace = clamp(n.z * 0.5 + 0.5, 0.0, 1.0);          // model +Z = nose
    float shade = 0.50 + 0.35 * camFace + 0.15 * noseFace;
    col *= shade;

    FragColor = vec4(col, 1.0);
}
