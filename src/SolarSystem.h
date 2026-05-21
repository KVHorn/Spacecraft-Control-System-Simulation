#pragma once
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glad/gl.h>

class Shader;
class Model;

class Shader;
class SphereMesh;
class RingMesh;
class OrbitMesh;

// Scaling mode - toggled with 'T' at runtime.
// COMPACT = visually comfortable (orbits compressed).
// REALISTIC = orbits at true proportional scale (planets become specks).
enum class ScaleMode { COMPACT, REALISTIC };

struct Moon {
    std::string name;
    std::string texturePath;
    float radiusKm;         // real radius
    float orbitRadiusKm;    // from parent planet center (real)
    float orbitPeriodDays;  // real orbital period
    float soiRadiusKm = 0.0f;

    // runtime
    GLuint texture = 0;
    float orbitAngle = 0.0f;
    std::unique_ptr<Model> model;
    glm::vec3 worldPos{0.0f};       // real-scale scene units; set every update()
    glm::vec3 worldVelKmS{0.0f};    // km/s, heliocentric
};

struct Planet {
    std::string name;
    std::string texturePath;
    float radiusKm;            // real
    float orbitRadiusGm;       // real, in gigameters (million km) - Earth ~149.6
    float orbitPeriodDays;     // real
    float rotationPeriodHours; // real (negative = retrograde, e.g. Venus)
    float axialTiltDeg;        // real
    float muKm3PerS2 = 0.0f;   // gravitational parameter GM (km^3/s^2)

    bool isSun = false;
    float soiRadiusKm = 0.0f;       // 0 = Sun (infinite SOI)
    glm::vec3 worldVelKmS{0.0f};    // km/s; set every update()

    bool hasRings = false;
    std::string ringTexturePath;
    float ringInnerMul = 1.2f; // as multiple of planet radius
    float ringOuterMul = 2.3f;

    std::vector<Moon> moons;

    // Orbital plane orientation (J2000 ecliptic-relative)
    float inclDeg    = 0.0f;   // inclination, degrees
    float ascNodeDeg = 0.0f;   // longitude of ascending node, degrees

    // runtime
    GLuint texture = 0;
    GLuint ringTex = 0;
    float rotationAngle = 0.0f;
    float orbitAngle = 0.0f;
    glm::vec3 worldPos{0.0f};  // recomputed every frame

    // Optional high-quality glTF mesh (loaded from models/M_<Name>.gltf if present).
    // When non-null, rendering uses this in place of the shared UV sphere.
    std::unique_ptr<Model> model;
};

class SolarSystem {
public:
    SolarSystem();
    ~SolarSystem();

    // Load all textures and build meshes. Safe to call once after GL init.
    void init();

    // Advance simulation by `dtSeconds` of real time.
    // `timeScale` controls how many simulated DAYS pass per real second.
    void update(float dtSeconds, float timeScale);

    // Render everything. Shaders are reused across calls.
    void render(Shader& planetShader,
                Shader& sunShader,
                Shader& ringShader,
                Shader& orbitShader,
                const glm::mat4& view,
                const glm::mat4& proj);

    // Toggle between COMPACT (visual) and REALISTIC (true proportional) scale modes.
    void toggleScale();
    // Returns the currently active scale mode.
    ScaleMode mode() const { return scaleMode; }

    // Enable or disable rendering of planetary orbit path lines.
    void setShowOrbits(bool b) { showOrbits = b; }
    bool getShowOrbits() const { return showOrbits; }

    // Runtime visual / scale settings
    ScaleMode getScaleMode()        const { return scaleMode; }
    void      setScaleMode(ScaleMode m)   { scaleMode = m; }

    float getPlanetSizeMul()        const { return planetSizeMul; }
    float getSunSizeMul()           const { return sunSizeMul; }
    float getCompactOrbitScale()    const { return compactOrbitScale; }
    float getSunBrightness()        const { return sunBrightness; }
    const glm::vec4& getPlanetOrbitColor() const { return planetOrbitColor; }

    void setPlanetSizeMul(float v)         { planetSizeMul     = v; }
    void setSunSizeMul(float v)            { sunSizeMul        = v; }
    void setCompactOrbitScale(float v)     { compactOrbitScale = v; }
    void setSunBrightness(float v)         { sunBrightness     = v; }
    void setPlanetOrbitColor(glm::vec4 c)  { planetOrbitColor  = c; }

    // Returns the Sun's scene-space position (always the world origin).
    glm::vec3 sunPosition() const { return glm::vec3(0.0f); }

    // Number of bodies and access by index (0 = Sun, 1..8 = Mercury..Neptune).
    size_t bodyCount() const { return planets.size(); }
    const Planet& body(size_t i) const { return planets[i]; }

    // Returns a good camera position/orientation for viewing a given body.
    // `outPos` = camera position, `outYaw`/`outPitch` = camera Euler angles.
    void computeFocusView(size_t bodyIdx,
                          glm::vec3& outPos,
                          float& outYaw,
                          float& outPitch) const;

    // Scene-unit helpers (public for Spacecraft).
    // 1 scene unit = 1000 km. In COMPACT mode, orbits between planets are
    // compressed, but distances *within* a body's local neighborhood stay real.
    float kmToSceneUnits() const { return 0.001f; }
    float planetSceneRadius(size_t i) const;   // radius in scene units

    // Real (uncompressed) km position of body i (uses orbitRadiusGm, not scene units).
    glm::dvec3 planetRealPosKm(size_t i) const;
    glm::dvec3 planetVelKmS(size_t i) const;

    // Seeds each planet's initial orbit angle and a *snapshot* radius from real
    // heliocentric (x,y,z) km coordinates (e.g., from JPL Horizons at a date).
    // Keys are planet names ("Mercury", "Venus", ...). Bodies not in the map
    // retain their default analytic positions. Returns number of seeded bodies.
    size_t seedFromRealCoords(
        const std::unordered_map<std::string, std::array<double, 3>>& posKm);

private:
    std::vector<Planet> planets;
    std::unique_ptr<SphereMesh> sphere;
    std::unique_ptr<OrbitMesh> orbit;  // unit circle, scaled per planet

    ScaleMode scaleMode = ScaleMode::COMPACT;
    bool showOrbits = true;

    // Runtime visual / scale settings (user-adjustable)
    float     planetSizeMul     = 1.0f;
    float     sunSizeMul        = 1.0f;
    float     compactOrbitScale = 0.002f;   // orbit compression factor in COMPACT mode
    float     sunBrightness     = 1.6f;
    glm::vec4 planetOrbitColor  = glm::vec4(0.0f, 1.0f, 1.0f, 1.0f);

    // Scene-space render radius of a non-Sun planet, scaled by planetSizeMul.
    float planetRenderRadius(const Planet& p) const;
    // Scene-space orbit distance for a planet, compressed in COMPACT mode.
    float planetOrbitDistance(const Planet& p) const;
    // Scene-space render radius of the Sun, using a fixed size in COMPACT mode.
    float sunRenderRadius(const Planet& sun) const;
    // Scene-space render radius of a moon, enlarged in COMPACT mode for visibility.
    float moonRenderRadius(const Moon& m) const;
    // Scene-space orbit radius of a moon around its parent, pushed out in COMPACT mode.
    float moonOrbitDistance(const Moon& m, const Planet& parent) const;
};
