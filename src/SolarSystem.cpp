#include "SolarSystem.h"
#include "Shader.h"
#include "Texture.h"
#include "Mesh.h"
#include "Model.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <iostream>
#include <filesystem>

// ==== Scene unit convention ====
// 1 scene unit = 1000 km.
// So Earth's radius (6,371 km) = 6.371 units.
// Earth's orbit (149.6 Gm = 149,600,000 km) = 149,600 units in REALISTIC mode.
//
// In COMPACT mode, orbit distances are compressed and the Sun is scaled down
// so the whole system fits in a viewable volume.

static constexpr float KM_TO_UNIT   = 0.001f;
static constexpr float GM_TO_UNIT   = 1000.0f; // 1 Gm = 1,000,000 km = 1000 units
static constexpr float COMPACT_ORBIT_SCALE = 0.002f; // ~500x compression
static constexpr float COMPACT_SUN_RADIUS  = 60.0f;
static constexpr float COMPACT_MOON_ORBIT_MUL = 15.0f;  // moons much farther in compact
static constexpr float COMPACT_MOON_SIZE_MUL  = 3.0f;

SolarSystem::SolarSystem() = default;
SolarSystem::~SolarSystem() = default;

void SolarSystem::init() {
    sphere = std::make_unique<SphereMesh>(96, 64);
    orbit  = std::make_unique<OrbitMesh>(1.0f, 256);

    planets.clear();

    // --- SUN ---
    {
        Planet sun;
        sun.name = "Sun";
        sun.texturePath = "textures/Unreal Assets/MS_Sun_BaseColor.png";
        sun.radiusKm = 696000.0f;
        sun.orbitRadiusGm = 0.0f;
        sun.orbitPeriodDays = 1.0f; // unused
        sun.rotationPeriodHours = 609.12f; // ~25.4 days at equator
        sun.axialTiltDeg = 7.25f;
        sun.isSun = true;
        planets.push_back(std::move(sun));
    }

    // --- MERCURY ---
    {
        Planet p;
        p.name = "Mercury";
        p.texturePath = "textures/2k_mercury.jpg";
        p.radiusKm = 2439.7f;
        p.orbitRadiusGm = 57.9f;
        p.orbitPeriodDays = 87.97f;
        p.rotationPeriodHours = 1407.6f;
        p.axialTiltDeg = 0.03f;
        planets.push_back(std::move(p));
    }

    // --- VENUS ---
    {
        Planet p;
        p.name = "Venus";
        p.texturePath = "textures/2k_venus_atmosphere.jpg";
        p.radiusKm = 6051.8f;
        p.orbitRadiusGm = 108.2f;
        p.orbitPeriodDays = 224.7f;
        p.rotationPeriodHours = -5832.5f; // retrograde
        p.axialTiltDeg = 177.4f;
        planets.push_back(std::move(p));
    }

    // --- EARTH + MOON ---
    {
        Planet p;
        p.name = "Earth";
        p.texturePath = "textures/2k_earth_daymap.jpg";
        p.radiusKm = 6371.0f;
        p.orbitRadiusGm = 149.6f;
        p.orbitPeriodDays = 365.25f;
        p.rotationPeriodHours = 23.93f;
        p.axialTiltDeg = 23.44f;

        Moon m;
        m.name = "Moon";
        m.texturePath = "textures/2k_moon.jpg";
        m.radiusKm = 1737.4f;
        m.orbitRadiusKm = 384400.0f;
        m.orbitPeriodDays = 27.32f;
        p.moons.push_back(std::move(m));

        planets.push_back(std::move(p));
    }

    // --- MARS ---
    {
        Planet p;
        p.name = "Mars";
        p.texturePath = "textures/2k_mars.jpg";
        p.radiusKm = 3389.5f;
        p.orbitRadiusGm = 227.9f;
        p.orbitPeriodDays = 686.97f;
        p.rotationPeriodHours = 24.62f;
        p.axialTiltDeg = 25.19f;
        planets.push_back(std::move(p));
    }

    // --- JUPITER ---
    {
        Planet p;
        p.name = "Jupiter";
        p.texturePath = "textures/2k_jupiter.jpg";
        p.radiusKm = 69911.0f;
        p.orbitRadiusGm = 778.5f;
        p.orbitPeriodDays = 4332.59f;
        p.rotationPeriodHours = 9.93f;
        p.axialTiltDeg = 3.13f;
        planets.push_back(std::move(p));
    }

    // --- SATURN + RINGS ---
    {
        Planet p;
        p.name = "Saturn";
        p.texturePath = "textures/2k_saturn.jpg";
        p.radiusKm = 58232.0f;
        p.orbitRadiusGm = 1434.0f;
        p.orbitPeriodDays = 10759.22f;
        p.rotationPeriodHours = 10.66f;
        p.axialTiltDeg = 26.73f;
        p.hasRings = true;
        p.ringTexturePath = "textures/2k_saturn_ring_alpha.png";
        p.ringInnerMul = 1.24f; // inner ring edge ~74,500 km / 60,268 km
        p.ringOuterMul = 2.27f; // outer edge of A-ring ~136,775 km
        planets.push_back(std::move(p));
    }

    // --- URANUS ---
    {
        Planet p;
        p.name = "Uranus";
        p.texturePath = "textures/2k_uranus.jpg";
        p.radiusKm = 25362.0f;
        p.orbitRadiusGm = 2871.0f;
        p.orbitPeriodDays = 30688.5f;
        p.rotationPeriodHours = -17.24f;
        p.axialTiltDeg = 97.77f;
        planets.push_back(std::move(p));
    }

    // --- NEPTUNE ---
    {
        Planet p;
        p.name = "Neptune";
        p.texturePath = "textures/2k_neptune.jpg";
        p.radiusKm = 24622.0f;
        p.orbitRadiusGm = 4495.0f;
        p.orbitPeriodDays = 60182.0f;
        p.rotationPeriodHours = 16.11f;
        p.axialTiltDeg = 28.32f;
        planets.push_back(std::move(p));
    }

    // Load textures
    for (auto& p : planets) {
        p.texture = loadTexture(p.texturePath, true);
        if (p.hasRings) {
            p.ringTex = loadTexture(p.ringTexturePath, true);
        }
        for (auto& m : p.moons) {
            m.texture = loadTexture(m.texturePath, true);
        }
    }

    // Optionally load a high-quality glTF mesh per body. If "models/M_<Name>.gltf"
    // exists alongside its .bin and .png files, use it in place of the UV sphere.
    for (auto& p : planets) {
        if (!p.isSun) {
            std::string gltfPath = "models/M_" + p.name + ".gltf";
            if (std::filesystem::exists(gltfPath)) {
                auto m = std::make_unique<Model>();
                bool flipUVs = (p.name == "Earth");
                if (m->loadFromFile(gltfPath, flipUVs) && m->loaded) {
                    std::cout << "[models] Using glTF mesh for " << p.name
                              << " (radius=" << m->modelRadius << ")\n";
                    p.model = std::move(m);
                } else {
                    std::cout << "[models] Failed to load " << gltfPath << "\n";
                }
            }
        }
        for (auto& mn : p.moons) {
            std::string mgPath = "models/M_" + mn.name + ".gltf";
            if (!std::filesystem::exists(mgPath)) continue;
            auto mdl = std::make_unique<Model>();
            if (mdl->loadFromFile(mgPath) && mdl->loaded) {
                std::cout << "[models] Using glTF mesh for " << mn.name << "\n";
                mn.model = std::move(mdl);
            }
        }
    }

    // Gravitational parameter GM (km^3/s^2) for each body, keyed by name
    // so reordering planets can't desync the values.
    auto setMu = [&](const std::string& name, float mu) {
        for (auto& p : planets) if (p.name == name) { p.muKm3PerS2 = mu; return; }
    };
    setMu("Sun",     1.32712440018e11f);
    setMu("Mercury", 2.2032e4f);
    setMu("Venus",   3.24859e5f);
    setMu("Earth",   3.98600e5f);
    setMu("Mars",    4.28284e4f);
    setMu("Jupiter", 1.26713e8f);
    setMu("Saturn",  3.7931e7f);
    setMu("Uranus",  5.794e6f);
    setMu("Neptune", 6.8351e6f);

    // SOI radii from patched-conics roadmap values
    auto setSOI = [&](const std::string& name, float soiKm) {
        for (auto& p : planets) if (p.name == name) { p.soiRadiusKm = soiKm; return; }
    };
    setSOI("Mercury",   112000.0f);
    setSOI("Venus",     616000.0f);
    setSOI("Earth",     924000.0f);
    setSOI("Mars",      577000.0f);
    setSOI("Jupiter",  48200000.0f);
    setSOI("Saturn",   54500000.0f);
    setSOI("Uranus",   51800000.0f);
    setSOI("Neptune",  86600000.0f);

    for (auto& p : planets)
        if (p.name == "Earth")
            for (auto& m : p.moons)
                if (m.name == "Moon") m.soiRadiusKm = 66100.0f;

    // Real-world orbital inclinations and ascending nodes (J2000 ecliptic)
    auto setOrbitPlane = [&](const std::string& name, float incl, float ascNode) {
        for (auto& p : planets)
            if (p.name == name) { p.inclDeg = incl; p.ascNodeDeg = ascNode; return; }
    };
    setOrbitPlane("Mercury", 7.005f,  48.33f);
    setOrbitPlane("Venus",   3.395f,  76.68f);
    setOrbitPlane("Earth",   0.0f,     0.0f);
    setOrbitPlane("Mars",    1.850f,  49.56f);
    setOrbitPlane("Jupiter", 1.305f, 100.46f);
    setOrbitPlane("Saturn",  2.485f, 113.67f);
    setOrbitPlane("Uranus",  0.773f,  74.01f);
    setOrbitPlane("Neptune", 1.769f, 131.78f);

    std::cout << "[SolarSystem] Initialized " << planets.size() << " bodies.\n";
}

float SolarSystem::planetSceneRadius(size_t i) const {
    if (i >= planets.size()) return 1.0f;
    return planetRenderRadius(planets[i]);
}

float SolarSystem::planetRenderRadius(const Planet& p) const {
    if (p.isSun) return sunRenderRadius(p);
    return p.radiusKm * KM_TO_UNIT * planetSizeMul;
}

float SolarSystem::sunRenderRadius(const Planet& sun) const {
    if (scaleMode == ScaleMode::REALISTIC) return sun.radiusKm * KM_TO_UNIT * sunSizeMul;
    return COMPACT_SUN_RADIUS * sunSizeMul;
}

float SolarSystem::planetOrbitDistance(const Planet& p) const {
    float real = p.orbitRadiusGm * GM_TO_UNIT;
    if (scaleMode == ScaleMode::REALISTIC) return real;
    return real * compactOrbitScale;
}

float SolarSystem::moonRenderRadius(const Moon& m) const {
    float base = m.radiusKm * KM_TO_UNIT;
    if (scaleMode == ScaleMode::COMPACT) base *= COMPACT_MOON_SIZE_MUL;
    return base;
}

float SolarSystem::moonOrbitDistance(const Moon& m, const Planet& parent) const {
    float real = m.orbitRadiusKm * KM_TO_UNIT;
    if (scaleMode == ScaleMode::COMPACT) {
        // Push moons out so they're visibly separated from the planet
        return real * COMPACT_MOON_ORBIT_MUL + planetRenderRadius(parent) * 2.0f;
    }
    return real;
}

void SolarSystem::toggleScale() {
    scaleMode = (scaleMode == ScaleMode::COMPACT) ? ScaleMode::REALISTIC : ScaleMode::COMPACT;
    std::cout << "[SolarSystem] Scale: "
              << (scaleMode == ScaleMode::COMPACT ? "COMPACT" : "REALISTIC") << "\n";
}

void SolarSystem::computeFocusView(size_t bodyIdx,
                                   glm::vec3& outPos,
                                   float& outYaw,
                                   float& outPitch) const {
    if (bodyIdx >= planets.size()) bodyIdx = 0;
    const Planet& p = planets[bodyIdx];

    float r = p.isSun ? sunRenderRadius(p) : p.radiusKm * KM_TO_UNIT;
    // Place camera a few radii away, slightly above, offset along +Z for a nice view.
    // For planets we want the Sun to be somewhat behind us for dramatic lighting.
    glm::vec3 target = p.worldPos;

    glm::vec3 camOffset;
    if (p.isSun) {
        camOffset = glm::vec3(0.0f, r * 0.5f, r * 4.0f);
    } else {
        // Offset on the dayside: between Sun and planet, then pushed past.
        glm::vec3 sunToPlanet = glm::normalize(target); // Sun is at origin
        glm::vec3 up(0, 1, 0);
        glm::vec3 side = glm::normalize(glm::cross(sunToPlanet, up));
        camOffset = side * (r * 3.5f) + up * (r * 1.2f) - sunToPlanet * (r * 2.0f);
    }
    outPos = target + camOffset;

    glm::vec3 dir = glm::normalize(target - outPos);
    outPitch = glm::degrees(std::asin(dir.y));
    outYaw   = glm::degrees(std::atan2(dir.z, dir.x));
}

void SolarSystem::update(float dtSeconds, float timeScale) {
    // timeScale = simulated days per real second
    float simDays = dtSeconds * timeScale;
    float simHours = simDays * 24.0f;

    for (auto& p : planets) {
        // Axial rotation (Sun is held stationary per design)
        if (!p.isSun && p.rotationPeriodHours != 0.0f) {
            float rotPerHour = 360.0f / p.rotationPeriodHours;
            p.rotationAngle += rotPerHour * simHours;
        }

        // Orbit around Sun
        if (!p.isSun && p.orbitPeriodDays > 0.0f) {
            float degPerDay = 360.0f / p.orbitPeriodDays;
            p.orbitAngle += degPerDay * simDays;
        }

        // Compute world position with orbital inclination applied.
        // Rotation order: Rx(i) tilts the orbit plane, then Ry(Ω) rotates
        // around the ecliptic normal to place the ascending node correctly.
        float r   = planetOrbitDistance(p);
        float ang = glm::radians(p.orbitAngle);
        float cx = std::cos(ang), sx = std::sin(ang);
        float ci = std::cos(glm::radians(p.inclDeg));
        float si = std::sin(glm::radians(p.inclDeg));
        float cO = std::cos(glm::radians(p.ascNodeDeg));
        float sO = std::sin(glm::radians(p.ascNodeDeg));

        // In-plane: (cx, 0, sx). Apply Rx(i) then Ry(Ω).
        float px1 = cx,    py1 = -sx * si, pz1 = sx * ci;
        p.worldPos = glm::vec3(
            (px1 * cO + pz1 * sO) * r,
            py1 * r,
            (-px1 * sO + pz1 * cO) * r);

        // Orbital velocity in km/s — derivative of position, same rotation applied.
        if (!p.isSun && p.orbitPeriodDays > 0.0f) {
            float rKm  = p.orbitRadiusGm * 1.0e6f;
            float vOrb = 2.0f * 3.14159265f * rKm / (p.orbitPeriodDays * 86400.0f);
            // d/dθ of in-plane: (-sx, 0, cx). Apply Rx(i) then Ry(Ω).
            float vx1 = -sx,  vy1 = -cx * si, vz1 = cx * ci;
            p.worldVelKmS = glm::vec3(
                (vx1 * cO + vz1 * sO) * vOrb,
                vy1 * vOrb,
                (-vx1 * sO + vz1 * cO) * vOrb);
        }

        // Moons: advance angle, compute worldPos and velocity
        for (auto& m : p.moons) {
            if (m.orbitPeriodDays > 0.0f) {
                m.orbitAngle += (360.0f / m.orbitPeriodDays) * simDays;
            }
            float ma = glm::radians(m.orbitAngle);
            float mOrbScene = m.orbitRadiusKm * KM_TO_UNIT;  // real scale, not compressed
            m.worldPos = p.worldPos + glm::vec3(std::cos(ma) * mOrbScene, 0.0f, std::sin(ma) * mOrbScene);

            if (m.orbitPeriodDays > 0.0f) {
                float mV = 2.0f * 3.14159265f * m.orbitRadiusKm / (m.orbitPeriodDays * 86400.0f);
                m.worldVelKmS = p.worldVelKmS + glm::vec3(-std::sin(ma) * mV, 0.0f, std::cos(ma) * mV);
            }
        }
    }
}

void SolarSystem::render(Shader& planetShader,
                         Shader& sunShader,
                         Shader& ringShader,
                         Shader& orbitShader,
                         const glm::mat4& view,
                         const glm::mat4& proj) {
    // ---- 1. Orbit paths ----
    if (showOrbits) {
        orbitShader.use();
        orbitShader.setMat4("uView", view);
        orbitShader.setMat4("uProj", proj);
        orbitShader.setVec4("uColor", planetOrbitColor);
        for (auto& p : planets) {
            if (p.isSun) continue;
            float r = planetOrbitDistance(p);
            glm::mat4 M = glm::mat4(1.0f);
            M = glm::rotate(M, glm::radians(p.ascNodeDeg), glm::vec3(0, 1, 0));
            M = glm::rotate(M, glm::radians(p.inclDeg),    glm::vec3(1, 0, 0));
            M = glm::scale(M, glm::vec3(r));
            orbitShader.setMat4("uModel", M);
            orbit->draw();
        }
    }

    // ---- 2. Sun ----
    {
        glDisable(GL_BLEND);   // ensure blend is off (rings draw after, may have left it on)
        Planet& sun = planets[0];
        sunShader.use();
        sunShader.setMat4("uView", view);
        sunShader.setMat4("uProj", proj);
        sunShader.setFloat("uEmissiveBoost", sunBrightness);
        sunShader.setInt("uTexture", 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sun.texture);

        float sR = sunRenderRadius(sun);
        glm::mat4 M(1.0f);
        M = glm::rotate(M, glm::radians(sun.axialTiltDeg), glm::vec3(0, 0, 1));
        M = glm::rotate(M, glm::radians(sun.rotationAngle), glm::vec3(0, 1, 0));
        M = glm::scale(M, glm::vec3(sR));
        sunShader.setMat4("uModel", M);
        sphere->draw();
    }

    // ---- 3. Planets (and their moons) ----
    planetShader.use();
    planetShader.setMat4("uView", view);
    planetShader.setMat4("uProj", proj);
    planetShader.setVec3("uLightPos", glm::vec3(0.0f));
    planetShader.setVec3("uLightColor", glm::vec3(1.0f));
    planetShader.setFloat("uAmbient", 0.02f);
    planetShader.setInt("uTexture", 0);

    for (size_t i = 1; i < planets.size(); ++i) {
        Planet& p = planets[i];

        float pR = planetRenderRadius(p);
        glm::mat4 T = glm::translate(glm::mat4(1.0f), p.worldPos);
        glm::mat4 R = glm::rotate(glm::mat4(1.0f), glm::radians(p.axialTiltDeg),
                                  glm::vec3(0, 0, 1));
        glm::mat4 Spin = glm::rotate(glm::mat4(1.0f), glm::radians(p.rotationAngle),
                                     glm::vec3(0, 1, 0));

        if (p.model) {
            // glTF path: mesh's native radius is modelRadius, so we scale by
            // (scene radius / model radius) so the mesh fills its intended size.
            float s = pR / std::max(p.model->modelRadius, 1e-6f);
            glm::mat4 Sc = glm::scale(glm::mat4(1.0f), glm::vec3(s));
            glm::mat4 M  = T * R * Spin * Sc;
            p.model->draw(planetShader, M);
        } else {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, p.texture);
            glm::mat4 Sc = glm::scale(glm::mat4(1.0f), glm::vec3(pR));
            glm::mat4 M  = T * R * Spin * Sc;
            planetShader.setMat4("uModel", M);
            glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(M)));
            planetShader.setMat3("uNormalMat", nm);
            sphere->draw();
        }

        // Moons
        for (auto& moon : p.moons) {
            float mR = moonRenderRadius(moon);
            float mOrb = moonOrbitDistance(moon, p);
            float ma = glm::radians(moon.orbitAngle);
            glm::vec3 moonLocal(std::cos(ma) * mOrb, 0.0f, std::sin(ma) * mOrb);
            glm::vec3 moonWorld = p.worldPos + moonLocal;

            glm::mat4 mT = glm::translate(glm::mat4(1.0f), moonWorld);

            if (moon.model) {
                float s = mR / std::max(moon.model->modelRadius, 1e-6f);
                glm::mat4 mSc = glm::scale(glm::mat4(1.0f), glm::vec3(s));
                glm::mat4 mM  = mT * mSc;
                moon.model->draw(planetShader, mM);
            } else {
                glm::mat4 mSc = glm::scale(glm::mat4(1.0f), glm::vec3(mR));
                glm::mat4 mM = mT * mSc;
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, moon.texture);
                planetShader.setMat4("uModel", mM);
                glm::mat3 mnm = glm::transpose(glm::inverse(glm::mat3(mM)));
                planetShader.setMat3("uNormalMat", mnm);
                sphere->draw();
            }
        }
    }

    // ---- 4. Rings (transparent, draw last) ----
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE); // rings visible from both sides

    ringShader.use();
    ringShader.setMat4("uView", view);
    ringShader.setMat4("uProj", proj);
    ringShader.setVec3("uLightPos", glm::vec3(0.0f));
    ringShader.setFloat("uAmbient", 0.1f);
    ringShader.setInt("uTexture", 0);

    for (auto& p : planets) {
        if (!p.hasRings) continue;
        float pR = planetRenderRadius(p);
        float rInner = pR * p.ringInnerMul;
        float rOuter = pR * p.ringOuterMul;

        // Build ring mesh on demand (one per planet with rings). Cached per-frame
        // for simplicity; for many ringed planets you'd cache this.
        RingMesh rm(rInner, rOuter, 256);

        glm::mat4 M = glm::translate(glm::mat4(1.0f), p.worldPos);
        M = glm::rotate(M, glm::radians(p.axialTiltDeg), glm::vec3(0, 0, 1));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, p.ringTex);
        ringShader.setMat4("uModel", M);
        rm.draw();
    }

    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

glm::dvec3 SolarSystem::planetRealPosKm(size_t i) const {
    if (i == 0 || i >= planets.size()) return glm::dvec3(0.0);
    const Planet& p = planets[i];
    double r  = (double)p.orbitRadiusGm * 1.0e6;
    double a  = glm::radians((double)p.orbitAngle);
    double ci = std::cos(glm::radians((double)p.inclDeg));
    double si = std::sin(glm::radians((double)p.inclDeg));
    double cO = std::cos(glm::radians((double)p.ascNodeDeg));
    double sO = std::sin(glm::radians((double)p.ascNodeDeg));
    double cx = std::cos(a), sx = std::sin(a);
    double px1 = cx, py1 = -sx * si, pz1 = sx * ci;
    return glm::dvec3(
        (px1 * cO + pz1 * sO) * r,
        py1 * r,
        (-px1 * sO + pz1 * cO) * r);
}

glm::dvec3 SolarSystem::planetVelKmS(size_t i) const {
    if (i >= planets.size()) return glm::dvec3(0.0);
    return glm::dvec3(planets[i].worldVelKmS);
}

size_t SolarSystem::seedFromRealCoords(
    const std::unordered_map<std::string, std::array<double, 3>>& posKm) {
    size_t n = 0;
    for (auto& p : planets) {
        auto it = posKm.find(p.name);
        if (it == posKm.end()) continue;
        double x = it->second[0];
        double z = it->second[2]; // J2000 z -> our scene z; Y is ecliptic-normal
        // Set orbit angle from real position in ecliptic (XZ plane for us)
        double angleRad = std::atan2(z, x);
        p.orbitAngle = (float)(angleRad * 180.0 / 3.14159265358979);
        // Optionally refine the semi-major axis to match the current |r|
        // (kept as nominal to avoid drift; advanced user can toggle later)
        ++n;
    }
    return n;
}
