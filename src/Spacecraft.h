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

// Orbital elements derived from state vectors (computed on demand).
struct OrbitalElements {
    double semiMajorAxisKm = 0.0;  // semi-major axis, km
    double semiMinorAxisKm = 0.0;  // semi-minor axis, km
    double eccentricity    = 0.0;
    double inclinationDeg  = 0.0;
    bool   prograde        = true;
    double periodDays      = 0.0;
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

    // Keplerian orbit shape parameters (km). a=0 signals a hyperbolic/escape trajectory.
    float semiMajorAxisKm = 8000.0f;
    float semiMinorAxisKm = 8000.0f;
    float eccentricity    = 0.0f;
    float inclinationRad  = 0.0f;
    float orbitDirection  = 1.0f;         // kept for init; propagation uses orbitalFrame direction
    float gravParamKm3S2  = 3.986e5f;     // gravitational parameter (mu) of parent body, km^3/s^2
    float orbitalPeriodDays = 0.0f;

    float meanAnomaly = 0.0f;
    // Orbital reference frame: columns are [periapsisDir, semiMinorDir, orbitNormalDir] in world space.
    glm::mat3 orbitalFrame{1.0f};
    glm::vec3 localPositionSceneUnits{0.0f};  // scene units, relative to parent body center
    glm::vec3 localVelocityDir{0.0f};         // normalized velocity direction (used for LVLH frame)
    glm::vec3 worldPosition{0.0f};            // scene units, heliocentric

    // Ground-truth state vectors - updated every frame.
    int        parentBodyIdx = 3;         // index into SolarSystem::planets; changes on SOI transitions
    glm::dvec3 statePosKm{0.0};           // km, relative to parent body center
    glm::dvec3 stateVelKmS{0.0};          // km/s, relative to parent body

    // N-body absolute (heliocentric) state. Used only when config.nBody == true.
    glm::dvec3 nBodyAbsolutePosKm{0.0};
    glm::dvec3 nBodyAbsoluteVelKmS{0.0};

    // Attitude quaternions: LVLH is orbit-derived, User is accumulated input, World is composed result.
    glm::quat attitudeLvlh{1, 0, 0, 0};
    glm::quat attitudeUser{1, 0, 0, 0};
    glm::quat attitudeWorld{1, 0, 0, 0};

    glm::vec3 angularVelocity{0.0f};  // rad/s, in body frame

    float renderScale = 1.0f;

    // Propulsion state
    EngineData engineData;
    float propellantKg = 0.0f;  // remaining propellant mass
    float throttle     = 0.0f;  // 0.0 to 1.0
    bool  engineOn     = false;

    // Initialize spacecraft state from a mission configuration and the current solar system state.
    void init(const MissionConfig& cfg, const SolarSystem& sim);

    // Advance spacecraft state by one simulation timestep.
    void update(float simDtDays, float realDtSec, const SolarSystem& sim);

    // Apply an angular torque impulse (rad/s^2 * dt) to the spacecraft in body frame.
    void applyTorque(const glm::vec3& bodyTorque, float dtSec);

    // Zero out angular velocity (kills all rotation instantly).
    void stopRotation();

    // Compute the visual render scale so the spacecraft appears a reasonable size on screen.
    float computeRenderScale(float modelRadius) const;

    // Extract Euler angles (pitch/yaw/roll in degrees) relative to the LVLH frame.
    glm::vec3 getEulerAnglesLvlhDeg() const;

    // Current orbital speed as the magnitude of stateVelKmS, in km/s.
    float getOrbitalSpeedKmS() const;

    float currentMassKg()  const { return engineData.dryMassKg + propellantKg; }
    float currentThrustN() const {
        if (!engineOn || propellantKg <= 0.0f || engineData.maxThrustN <= 0.0f) return 0.0f;
        return engineData.maxThrustN * throttle;
    }

    // Compute classical orbital elements from position/velocity state vectors and a gravitational parameter.
    static OrbitalElements elementsFromState(glm::dvec3 posKm, glm::dvec3 velKmS, double muKm3S2);

    // Look up propulsion data for a spacecraft by matching its model filename against a known table.
    static EngineData lookupEngineData(const std::string& modelFile);

    // Rebuild orbitalFrame and Keplerian parameters from the current statePosKm/stateVelKmS/gravParamKm3S2.
    void recomputeFromState();

private:
    bool nBodyInitialized = false;

    // Check whether the spacecraft has crossed an SOI boundary and switch parent if so.
    void checkSOI(const SolarSystem& sim);

    // Switch the spacecraft's parent body to newIdx, re-expressing state vectors in the new frame.
    void switchParent(int newIdx, const SolarSystem& sim);

    // Build an orbital reference frame matrix from position, velocity, and gravitational parameter.
    static glm::mat3 computeOrbitalFrameFromState(glm::dvec3 posKm, glm::dvec3 velKmS, double mu);

    // Advance N-body state one full simulation timestep using sub-stepped RK4 integration.
    void updateNBody(float simDtSec, const SolarSystem& sim,
                     glm::dvec3 thrustAccKmS2 = glm::dvec3(0.0));

    // Determine which planet's SOI the spacecraft is currently inside and update parentBodyIdx.
    void updateNBodyParentBody(const SolarSystem& sim);

    // Perform one RK4 integration step on the N-body heliocentric state.
    void rk4Step(double dtSec, const SolarSystem& sim,
                 glm::dvec3 thrustAccKmS2 = glm::dvec3(0.0));
};
