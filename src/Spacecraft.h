#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

class SolarSystem;

// Real propulsion data for a spacecraft, looked up by model filename.
struct EngineData {
    std::string engineName;
    float maxThrustN       = 0.0f;  // 0 = no propulsion system
    float ispSec           = 0.0f;
    float propellantMassKg = 0.0f;
    float wetMassKg        = 0.0f;
    float dryMassKg        = 0.0f;
};

// Orbital elements derived from state vectors (on demand).
struct OrbElements {
    double a          = 0.0;   // semi-major axis, km
    double b          = 0.0;   // semi-minor axis, km
    double e          = 0.0;   // eccentricity
    double incDeg     = 0.0;   // inclination, degrees
    bool   prograde   = true;
    double periodDays = 0.0;
};

struct MissionConfig {
    int   spacecraftType = 0;
    int   startingBody   = 3;
    float semiMajorKm    = 8000.0f;
    float semiMinorKm    = 8000.0f;
    float inclinationDeg = 28.5f;
    bool  prograde       = true;

    std::string modelFile;
    float       modelScale = 1.0f;
    float       modelRotX  = 0.0f;
    float       modelRotY  = 0.0f;
    float       modelRotZ  = 0.0f;

    bool  realScale    = false;
    float lengthMeters = 11.0f;
    bool  nBody        = false;   // true = N-body RK4, false = patched conics
};

namespace scLabels {
    const char* const kSpacecraft[] = { "Apollo CSM", "Soyuz", "Probe (unmanned)", "Dragon 2 (Crew)" };
}

class Spacecraft {
public:
    MissionConfig config;

    float a = 8000.0f, b = 8000.0f, e = 0.0f;
    float inclRad = 0.0f;
    float dir = 1.0f;           // kept for init; propagation uses orbFrame direction
    float muKm3PerS2 = 3.986e5f;
    float periodDays = 0.0f;

    float meanAnomaly = 0.0f;
    glm::mat3 orbFrame{1.0f};   // cols: [periDir, semiMinorDir, normalDir] → world space
    glm::vec3 localPos{0.0f};   // scene units, relative to parent (derived from statePos)
    glm::vec3 localVel{0.0f};   // normalized velocity direction (for LVLH frame)
    glm::vec3 worldPos{0.0f};

    // Ground-truth state vectors (Stage 1+). Set every update().
    int        parentBodyIdx = 3;    // active parent; mutable for Stage 2 SOI transitions
    glm::dvec3 statePos{0.0};        // km, relative to parent body
    glm::dvec3 stateVel{0.0};        // km/s, relative to parent body

    // N-body absolute state (heliocentric km / km/s). Used when config.nBody == true.
    glm::dvec3 nbodyAbsPos{0.0};
    glm::dvec3 nbodyAbsVel{0.0};

    // Attitude: LVLH frame (from orbit), user delta (from input), composed world
    glm::quat attitudeLvlh{1, 0, 0, 0};
    glm::quat attitudeUser{1, 0, 0, 0};
    glm::quat attitudeWorld{1, 0, 0, 0};

    glm::vec3 angularVel{0.0f}; // rad/s, body frame

    float renderScale = 1.0f;

    // Propulsion
    EngineData engineData;
    float propellantKg = 0.0f;  // remaining propellant
    float throttle     = 0.0f;  // 0.0–1.0
    bool  engineOn     = false; // main engine switch

    void init(const MissionConfig& cfg, const SolarSystem& sim);
    void update(float simDtDays, float realDtSec, const SolarSystem& sim);
    void applyTorque(const glm::vec3& bodyTorque, float dtSec);
    void stopRotation();
    float computeRenderScale(float modelRadius) const;
    glm::vec3 getEulerDegreesLvlh() const;

    // Orbital speed in km/s — magnitude of stateVel.
    float getOrbitalSpeedKmS() const;

    float currentMassKg()  const { return engineData.dryMassKg + propellantKg; }
    float currentThrustN() const {
        if (!engineOn || propellantKg <= 0.0f || engineData.maxThrustN <= 0.0f) return 0.0f;
        return engineData.maxThrustN * throttle;
    }

    // Convert state vectors to orbital elements. Used on SOI transitions (Stage 2+).
    static OrbElements elementsFromState(glm::dvec3 posKm, glm::dvec3 velKmS, double muKm3S2);

    // Look up engine data by model filename (stem match against known spacecraft table).
    static EngineData lookupEngineData(const std::string& modelFile);

    // Rebuild orbFrame + Kepler vars from current statePos/stateVel/muKm3PerS2.
    void recomputeFromState();

private:
    bool nbodyInitialized = false;

    void checkSOI(const SolarSystem& sim);
    void switchParent(int newIdx, const SolarSystem& sim);
    static glm::mat3 orbFrameFromState(glm::dvec3 posKm, glm::dvec3 velKmS, double mu);

    void updateNBody(float simDtSec, const SolarSystem& sim,
                     glm::dvec3 thrustAccKmS2 = glm::dvec3(0.0));
    void updateNBodyParent(const SolarSystem& sim);
    void rk4Step(double dtSec, const SolarSystem& sim,
                 glm::dvec3 thrustAccKmS2 = glm::dvec3(0.0));
};
