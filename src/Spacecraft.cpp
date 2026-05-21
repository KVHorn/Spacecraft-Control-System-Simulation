#include "Spacecraft.h"
#include "SolarSystem.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <algorithm>
#include <iostream>

static constexpr float  TWO_PI  = 6.28318530717958647692f;
static constexpr double TWO_PId = 6.28318530717958647692;

// solveKepler
// Purpose: Solve Kepler's equation M = E - e*sin(E) for the eccentric anomaly E.
// Inputs:  meanAnomaly - mean anomaly in radians (any range; normalized internally)
//          eccentricity - orbit eccentricity (0 = circle, <1 = ellipse)
// Actions: Iterates Newton-Raphson up to 12 times until convergence.
// Returns: Eccentric anomaly E in radians.
static float solveKepler(float meanAnomaly, float eccentricity) {
    while (meanAnomaly >  3.14159265f) meanAnomaly -= TWO_PI;
    while (meanAnomaly < -3.14159265f) meanAnomaly += TWO_PI;
    float E = meanAnomaly;
    for (int i = 0; i < 12; ++i) {
        float residual      = E - eccentricity * std::sin(E) - meanAnomaly;
        float residualSlope = 1.0f - eccentricity * std::cos(E);
        float correction    = residual / residualSlope;
        E -= correction;
        if (std::fabs(correction) < 1e-7f) break;
    }
    return E;
}

// lookupEngineData
// Purpose: Find real propulsion parameters for a spacecraft by matching its model filename.
// Inputs:  modelFile - relative path to the spacecraft's .glb/.gltf model file
// Actions: Strips directory and extension from modelFile to get the stem, then searches
//          a hard-coded table of known spacecraft for a substring match.
// Returns: EngineData populated with thrust, Isp, and mass values, or a generic fallback.
EngineData Spacecraft::lookupEngineData(const std::string& modelFile) {
    // Strip directory and extension to get stem for matching
    std::string stem = modelFile;
    auto slash = stem.find_last_of("/\\");
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    auto dot = stem.rfind('.');
    if (dot != std::string::npos) stem = stem.substr(0, dot);

    // Table: { substring_key, {engineName, maxThrustN, ispSec, propellantKg, wetKg, dryKg} }
    // Sources: NASA fact sheets, Wikipedia, Gunter's Space Page (see CONTEXT.md research)
    struct Entry { const char* key; EngineData data; };
    static const Entry table[] = {
        // SpaceX Crew Dragon: 16x Draco biprop thrusters for orbital maneuvering
        {"Dragon",          {"Draco x16",             6400.0f,  300.0f,   1388.0f,   9616.0f,  8228.0f}},
        // Apollo-Soyuz: Apollo CSM Service Propulsion System AJ10-137
        {"Apollo Soyuz",    {"AJ10-137 SPS",         91190.0f, 314.5f,  18410.0f,  28800.0f, 10390.0f}},
        // Ares I upper stage: J-2X (rocket stage, not a pilotable spacecraft)
        {"Ares 1",          {"J-2X",               1307000.0f, 448.0f, 135100.0f, 172000.0f, 36900.0f}},
        // Cassini orbiter with Huygens attached: R-4D biprop (490 N, one engine fires at a time)
        {"Cassini",         {"R-4D",                   490.0f, 312.0f,   3132.0f,   5655.0f,  2523.0f}},
        // Chandra: TR-308 biprop apogee thrusters (4x472 N, used only for orbit insertion)
        {"Chandra",         {"TR-308",                 472.0f, 322.0f,   1070.0f,   5860.0f,  4790.0f}},
        // CloudSat: 4x hydrazine monoprop thrusters, attitude/translation only
        {"CloudSat",        {"Hydrazine x4",            17.8f, 220.0f,    147.0f,    995.0f,   848.0f}},
        // Deep Space 1: NSTAR ion engine - 92 mN at max, Isp 3100 s
        {"Deep Space 1",    {"NSTAR Ion",              0.092f,3100.0f,     82.0f,    486.0f,   373.0f}},
        // Hubble: no propulsion; reboosts from visiting Shuttle only
        {"Hubble",          {"(none)",                  0.0f,   0.0f,      0.0f,  11110.0f, 11110.0f}},
        // JWST: SCAT biprop thrusters (thrust estimate; exact value not public)
        {"James Webb",      {"SCAT biprop",            220.0f, 310.0f,    301.0f,   6200.0f,  5899.0f}},
        // Juno: LEROS-1b biprop main engine
        {"Juno",            {"LEROS-1b",               645.0f, 317.0f,   2032.0f,   3625.0f,  1593.0f}},
        // Magellan post-VOI: 16x 445 N hydrazine (only a few fire at once; use single-thruster value)
        {"Magellan",        {"Hydrazine MRM",          445.0f, 220.0f,    133.0f,   1168.0f,  1035.0f}},
        // Mars Global Surveyor: LEROS-1b (slightly derated to 596 N as-flown)
        {"Mars Global",     {"LEROS-1b",               596.0f, 318.0f,    361.0f,   1030.0f,   669.0f}},
        // MRO: 6x MR-106 hydrazine monoprop (170 N each) for MOI; 1020 N combined
        {"Mars Recon",      {"MR-106 x6",             1020.0f, 228.0f,   1149.0f,   2180.0f,  1031.0f}},
        // Nancy Grace Roman: 4x 22 N hydrazine at L2
        {"Nancy",           {"Hydrazine x4",            88.0f, 220.0f,   1100.0f,   4166.0f,  3066.0f}},
        // Space Shuttle OMS: 2x AJ10-190 (53,400 N combined)
        {"Space Shuttle",   {"AJ10-190 OMS x2",      53400.0f, 316.0f,  10830.0f, 110000.0f, 78000.0f}},
        // Voyager 1 & 2: 16x MR-103 hydrazine thrusters (~1.1 N each; ~4 active at once)
        {"Voyager",         {"MR-103 x16",              4.48f, 227.0f,    100.0f,    815.0f,   715.0f}},
    };

    for (const auto& entry : table)
        if (stem.find(entry.key) != std::string::npos) return entry.data;

    // Unknown model: provide a modest generic thruster so the spacecraft isn't inert
    return {"Generic thruster", 100.0f, 220.0f, 50.0f, 1000.0f, 950.0f};
}

// init
// Purpose: Set up a new spacecraft in orbit around its starting body.
// Inputs:  cfg - mission configuration (orbit shape, model, physics mode, etc.)
//          sim - current solar system state, used to read the parent body's gravity parameter
// Actions: Clamps orbital parameters, computes eccentricity and period, builds the orbital
//          reference frame, and initializes propulsion from the engine lookup table.
void Spacecraft::init(const MissionConfig& cfg, const SolarSystem& sim) {
    config = cfg;

    parentBodyIdx = std::clamp(cfg.startingBody, 0, (int)sim.bodyCount() - 1);
    size_t parentIdx = (size_t)parentBodyIdx;
    const Planet& parent = sim.body(parentIdx);

    gravParamKm3S2 = (parent.muKm3PerS2 > 0.0f) ? parent.muKm3PerS2 : 3.986e5f;

    semiMajorAxisKm = std::max(cfg.semiMajorKm, 100.0f);
    semiMinorAxisKm = std::clamp(cfg.semiMinorKm, 100.0f, semiMajorAxisKm);
    float axisRatio = semiMinorAxisKm / semiMajorAxisKm;
    eccentricity    = std::sqrt(std::max(0.0f, 1.0f - axisRatio * axisRatio));
    inclinationRad  = glm::radians(cfg.inclinationDeg);
    orbitDirection  = cfg.prograde ? 1.0f : -1.0f;

    float orbitalPeriodSec = TWO_PI * std::sqrt((semiMajorAxisKm * semiMajorAxisKm * semiMajorAxisKm) / gravParamKm3S2);
    orbitalPeriodDays      = orbitalPeriodSec / 86400.0f;

    meanAnomaly       = 0.0f;
    nBodyInitialized  = false;
    attitudeUser      = glm::quat(1, 0, 0, 0);
    angularVelocity   = glm::vec3(0.0f);

    engineData    = lookupEngineData(cfg.modelFile);
    propellantKg  = engineData.propellantMassKg;
    throttle      = 0.0f;
    engineOn      = false;

    // Build orbital frame: periapsis along +X, normal encodes prograde/retrograde.
    // orbitNormalDir = (0, -sin(i), cos(i)) for prograde; flipped for retrograde.
    float ci = std::cos(inclinationRad), si = std::sin(inclinationRad);
    glm::vec3 orbitNormalDir(0.0f, -si, ci);
    if (!cfg.prograde) orbitNormalDir = -orbitNormalDir;
    glm::vec3 periapsisDir(1.0f, 0.0f, 0.0f);
    glm::vec3 semiMinorDir = glm::normalize(glm::cross(orbitNormalDir, periapsisDir));
    orbitalFrame = glm::mat3(periapsisDir, semiMinorDir, orbitNormalDir);

    renderScale = computeRenderScale(1.0f); // fallback; gets updated when model loads
}

// computeRenderScale
// Purpose: Determine the visual size multiplier so the spacecraft looks correct on screen.
// Inputs:  modelRadius - the bounding radius of the loaded 3D model in model-local units
// Actions: In real-scale mode, maps meters to scene units. Otherwise scales relative to orbit size.
// Returns: Scale factor to apply to the spacecraft's model transform.
float Spacecraft::computeRenderScale(float modelRadius) const {
    // 1 scene unit = 1000 km = 1,000,000 m, so 1 m = 1e-6 units
    if (config.realScale) {
        // lengthMeters is the spacecraft's visible physical length.
        // Ignore modelRadius; caller handles model scale independently.
        return std::max(1e-6f, config.lengthMeters * 1.0e-6f);
    }
    // Scale to ~1% of orbit semi-major axis so the ship looks small next to planets.
    return std::clamp(semiMajorAxisKm * 8.0e-6f, 0.005f, 0.8f);
}

// applyTorque
// Purpose: Add an angular acceleration impulse to the spacecraft's body-frame rotation rate.
// Inputs:  bodyTorque - angular acceleration vector in body frame (rad/s^2 per axis)
//          dtSec      - timestep duration in real seconds
// Actions: Multiplies torque by dt and adds the result to angularVelocity.
void Spacecraft::applyTorque(const glm::vec3& bodyTorque, float dtSec) {
    angularVelocity += bodyTorque * dtSec;
}

// stopRotation
// Purpose: Instantly zero out all angular velocity (RCS dead-stop command).
void Spacecraft::stopRotation() {
    angularVelocity = glm::vec3(0.0f);
}

// update
// Purpose: Advance the spacecraft's position, velocity, attitude, and propellant by one timestep.
// Inputs:  simDtDays - simulated time elapsed this frame in days
//          realDtSec - real elapsed wall-clock time this frame in seconds (for attitude damping)
//          sim       - current solar system state (planet positions/velocities for SOI checks)
// Actions: Computes thrust acceleration, propagates orbit (Kepler or N-body RK4), checks SOI
//          boundaries, updates the LVLH frame, integrates attitude from angular velocity with
//          damping, and composes the world-space attitude quaternion.
void Spacecraft::update(float simDtDays, float realDtSec, const SolarSystem& sim) {
    float simDtSec = simDtDays * 86400.0f;

    // Compute thrust acceleration using last frame's world attitude
    float thrustN = currentThrustN();
    glm::dvec3 thrustDeltaVel(0.0);
    glm::dvec3 thrustAccKmS2(0.0);
    if (thrustN > 0.0f) {
        float mass = currentMassKg();
        if (mass > 0.0f) {
            // Body -Z is the prograde / "forward" direction in LVLH convention
            glm::vec3 thrustDir = -(glm::mat3_cast(attitudeWorld)[2]);
            float accKmS2   = thrustN / mass / 1000.0f;
            thrustDeltaVel  = glm::dvec3(thrustDir) * (double)(accKmS2 * simDtSec);
            thrustAccKmS2   = glm::dvec3(thrustDir) * (double)accKmS2;
        }
        float propellantBurnKg = thrustN / (engineData.ispSec * 9.80665f) * simDtSec;
        propellantKg = std::max(0.0f, propellantKg - propellantBurnKg);
    }

    // N-body mode: bypass Kepler entirely; worldPosition/localPositionSceneUnits set inside updateNBody
    if (config.nBody) {
        updateNBody(simDtSec, sim, thrustAccKmS2);
    } else if (semiMajorAxisKm <= 0.0f) {
        // Escape/hyperbolic trajectory: propagate linearly
        statePosKm  += stateVelKmS * (double)simDtSec;
        stateVelKmS += thrustDeltaVel;
        checkSOI(sim);
        float kmToU = sim.kmToSceneUnits();
        localPositionSceneUnits = glm::vec3(statePosKm) * kmToU;
        localVelocityDir = (glm::length(stateVelKmS) > 1e-9)
                         ? glm::normalize(glm::vec3(stateVelKmS))
                         : glm::vec3(0.0f, 0.0f, 1.0f);
        worldPosition = sim.body((size_t)parentBodyIdx).worldPos + localPositionSceneUnits;
        if (thrustN > 0.0f) recomputeFromState();
    } else {
        // Advance mean anomaly (retrograde is encoded in orbitalFrame, so always positive increment)
        meanAnomaly += (TWO_PI / orbitalPeriodDays) * simDtDays;

        // Kepler position/velocity in the orbital plane
        float E     = solveKepler(meanAnomaly, eccentricity);
        float xOrb  = semiMajorAxisKm * (std::cos(E) - eccentricity);
        float yOrb  = semiMinorAxisKm * std::sin(E);
        float denom = 1.0f - eccentricity * std::cos(E);
        float vxNorm = -std::sin(E) / denom;
        float vyNorm = (semiMinorAxisKm / semiMajorAxisKm) * std::cos(E) / denom;
        float velLen = std::sqrt(vxNorm*vxNorm + vyNorm*vyNorm);
        if (velLen > 0.0f) { vxNorm /= velLen; vyNorm /= velLen; }

        statePosKm  = glm::dvec3(orbitalFrame * glm::vec3(xOrb, yOrb, 0.0f));
        double distKm = glm::length(statePosKm);
        double vis_viva = (distKm > 0.0) ? (double)gravParamKm3S2 * (2.0/distKm - 1.0/semiMajorAxisKm) : 0.0;
        double speedKmS = (vis_viva > 0.0) ? std::sqrt(vis_viva) : 0.0;
        glm::vec3 velDir = orbitalFrame * glm::vec3(vxNorm, vyNorm, 0.0f);
        stateVelKmS  = glm::dvec3(velDir) * speedKmS;
        localVelocityDir = glm::normalize(velDir);

        // Apply thrust impulse, then refresh orbital elements
        if (thrustN > 0.0f) {
            stateVelKmS += thrustDeltaVel;
            localVelocityDir = (glm::length(stateVelKmS) > 1e-9)
                              ? glm::normalize(glm::vec3(stateVelKmS))
                              : localVelocityDir;
            recomputeFromState();
        }

        // SOI check then convert km -> scene units
        checkSOI(sim);
        float kmToU = sim.kmToSceneUnits();
        localPositionSceneUnits = glm::vec3(statePosKm) * kmToU;
        worldPosition = sim.body((size_t)parentBodyIdx).worldPos + localPositionSceneUnits;
    }

    // Build LVLH frame: forward = velocity, up = radial outward
    glm::vec3 radialOut = glm::normalize(localPositionSceneUnits);
    glm::vec3 fwd       = glm::normalize(localVelocityDir);
    glm::vec3 rightVec  = glm::normalize(glm::cross(radialOut, fwd));
    glm::vec3 upVec     = glm::normalize(glm::cross(fwd, rightVec));
    glm::mat3 Rlvlh;
    Rlvlh[0] =  rightVec;
    Rlvlh[1] =  upVec;
    Rlvlh[2] = -fwd;
    attitudeLvlh = glm::quat_cast(Rlvlh);

    // Integrate user attitude from angular velocity, then apply exponential damping
    float dampFactor = std::pow(0.985f, std::max(0.0f, realDtSec * 60.0f));
    angularVelocity *= dampFactor;

    float rotRate = glm::length(angularVelocity);
    if (rotRate > 1e-8f && realDtSec > 0.0f) {
        glm::vec3 rotAxis  = angularVelocity / rotRate;
        float     rotAngle = rotRate * realDtSec;
        glm::quat deltaRot = glm::angleAxis(rotAngle, rotAxis);
        // Compose on the right so torque is applied in body frame
        attitudeUser = glm::normalize(attitudeUser * deltaRot);
    }

    // Compose final world attitude from LVLH base plus user-input offset
    attitudeWorld = glm::normalize(attitudeLvlh * attitudeUser);
}

// getEulerAnglesLvlhDeg
// Purpose: Extract pitch, yaw, and roll in degrees relative to the LVLH orbital frame.
// Actions: Decomposes attitudeUser quaternion into ZYX Euler angles.
// Returns: vec3 where .x = pitch, .y = yaw, .z = roll (all in degrees).
glm::vec3 Spacecraft::getEulerAnglesLvlhDeg() const {
    const glm::quat& q = attitudeUser;
    float qw = q.w, qx = q.x, qy = q.y, qz = q.z;
    glm::vec3 eulerDeg;
    float sinPitch = 2.0f * (qw * qy - qz * qx);
    sinPitch = std::clamp(sinPitch, -1.0f, 1.0f);
    eulerDeg.y = glm::degrees(std::asin(sinPitch));
    eulerDeg.x = glm::degrees(std::atan2(2.0f * (qw * qx + qy * qz),
                                         1.0f - 2.0f * (qx * qx + qy * qy)));
    eulerDeg.z = glm::degrees(std::atan2(2.0f * (qw * qz + qx * qy),
                                         1.0f - 2.0f * (qy * qy + qz * qz)));
    return eulerDeg;
}

// getOrbitalSpeedKmS
// Purpose: Return the current orbital speed as the magnitude of stateVelKmS.
// Returns: Speed in km/s.
float Spacecraft::getOrbitalSpeedKmS() const {
    return (float)glm::length(stateVelKmS);
}

// checkSOI
// Purpose: Detect if the spacecraft has crossed an SOI boundary and switch parents if needed.
// Inputs:  sim - solar system state providing planet positions and SOI radii
// Actions: Computes distance ratios to the current parent and all other planets. If the spacecraft
//          has left the current parent's SOI, it falls back to the Sun. If it has entered a
//          different planet's SOI more deeply than the current one, it switches to that planet.
void Spacecraft::checkSOI(const SolarSystem& sim) {
    glm::dvec3 heliocentricPos = sim.planetRealPosKm((size_t)parentBodyIdx) + statePosKm;

    // Ratio of distance-to-parent over parent's SOI radius (infinite for Sun)
    double currentRatio = 1e30;
    if (parentBodyIdx != 0) {
        float parentSOI = sim.body((size_t)parentBodyIdx).soiRadiusKm;
        double distToParent = glm::length(heliocentricPos - sim.planetRealPosKm((size_t)parentBodyIdx));
        if (parentSOI > 0.0f) currentRatio = distToParent / (double)parentSOI;
        // Escaped current parent's SOI - fall back to Sun
        if (distToParent > (double)parentSOI * 1.05) {
            switchParent(0, sim);
            return;
        }
    }

    // Check whether any other planet's SOI is closer (more deeply inside)
    int    closestBody  = parentBodyIdx;
    double closestRatio = currentRatio;
    for (int i = 1; i < (int)sim.bodyCount(); ++i) {
        if (i == parentBodyIdx) continue;
        float soiKm = sim.body((size_t)i).soiRadiusKm;
        if (soiKm <= 0.0f) continue;
        double distToBody = glm::length(heliocentricPos - sim.planetRealPosKm((size_t)i));
        double ratio      = distToBody / (double)soiKm;
        if (ratio < closestRatio * 0.95 && ratio < closestRatio) {
            closestRatio = ratio;
            closestBody  = i;
        }
    }

    if (closestBody != parentBodyIdx)
        switchParent(closestBody, sim);
}

// switchParent
// Purpose: Transfer the spacecraft to a new parent body, re-expressing state vectors in the new frame.
// Inputs:  newIdx - index of the new parent planet in SolarSystem::planets
//          sim    - solar system state for planet positions and velocities
// Actions: Converts current state to absolute (heliocentric), then subtracts the new parent's
//          position and velocity to get parent-relative state. Recomputes orbital elements.
void Spacecraft::switchParent(int newIdx, const SolarSystem& sim) {
    std::cout << "[SOI] " << sim.body((size_t)parentBodyIdx).name
              << " -> " << sim.body((size_t)newIdx).name << "\n";

    glm::dvec3 absoluteVel  = stateVelKmS + sim.planetVelKmS((size_t)parentBodyIdx);
    glm::dvec3 newStatePosKm = statePosKm + sim.planetRealPosKm((size_t)parentBodyIdx)
                                          - sim.planetRealPosKm((size_t)newIdx);
    glm::dvec3 newStateVelKmS = absoluteVel - sim.planetVelKmS((size_t)newIdx);

    parentBodyIdx  = newIdx;
    gravParamKm3S2 = sim.body((size_t)newIdx).muKm3PerS2;
    statePosKm     = newStatePosKm;
    stateVelKmS    = newStateVelKmS;
    recomputeFromState();
}

// computeOrbitalFrameFromState
// Purpose: Build a 3x3 orbital reference frame matrix from position and velocity vectors.
// Inputs:  posKm   - position in km (from parent body center)
//          velKmS  - velocity in km/s
//          mu      - gravitational parameter of parent body (km^3/s^2)
// Actions: Computes the eccentricity vector to find periapsis direction, then forms
//          the right-handed frame [periapsisDir, semiMinorDir, orbitNormalDir].
// Returns: 3x3 rotation matrix with columns [periapsisDir, semiMinorDir, orbitNormalDir].
glm::mat3 Spacecraft::computeOrbitalFrameFromState(glm::dvec3 posKm, glm::dvec3 velKmS, double mu) {
    glm::dvec3 angularMomentum = glm::cross(posKm, velKmS);
    glm::dvec3 eccentricityVec = glm::cross(velKmS, angularMomentum) / mu - glm::normalize(posKm);
    double eccMag = glm::length(eccentricityVec);
    glm::dvec3 periapsisDir  = (eccMag > 1e-9) ? glm::normalize(eccentricityVec) : glm::normalize(posKm);
    glm::dvec3 orbitNormalDir = glm::normalize(angularMomentum);
    glm::dvec3 semiMinorDir   = glm::normalize(glm::cross(orbitNormalDir, periapsisDir));
    return glm::mat3(glm::vec3(periapsisDir), glm::vec3(semiMinorDir), glm::vec3(orbitNormalDir));
}

// gravAccel
// Purpose: Compute total gravitational acceleration at a heliocentric position from all bodies.
// Inputs:  heliocentricPos - absolute position in km (heliocentric)
//          sim             - solar system providing planet GM values and positions
// Actions: Sums contributions from all bodies using the inverse-square law.
// Returns: Acceleration vector in km/s^2.
static glm::dvec3 gravAccel(glm::dvec3 heliocentricPos, const SolarSystem& sim) {
    glm::dvec3 totalAccel(0.0);
    for (size_t i = 0; i < sim.bodyCount(); ++i) {
        double mu = (double)sim.body(i).muKm3PerS2;
        if (mu <= 0.0) continue;
        glm::dvec3 offset = sim.planetRealPosKm(i) - heliocentricPos;
        double dist2 = glm::dot(offset, offset);
        if (dist2 < 1.0) continue;  // closer than 1 km - skip (inside body)
        totalAccel += offset * (mu / (dist2 * std::sqrt(dist2)));
    }
    return totalAccel;
}

// rk4Step
// Purpose: Advance the N-body heliocentric state by one RK4 integration step.
// Inputs:  dtSec         - integration step size in seconds
//          sim           - solar system for gravity calculations
//          thrustAccKmS2 - additional thrust acceleration in km/s^2 (zero if coasting)
// Actions: Evaluates four derivative estimates and combines them with RK4 weights to
//          update nBodyAbsolutePosKm and nBodyAbsoluteVelKmS.
void Spacecraft::rk4Step(double dtSec, const SolarSystem& sim, glm::dvec3 thrustAccKmS2) {
    glm::dvec3 pos = nBodyAbsolutePosKm, vel = nBodyAbsoluteVelKmS;
    glm::dvec3 k1p = vel,                   k1v = gravAccel(pos,                    sim) + thrustAccKmS2;
    glm::dvec3 k2p = vel + k1v*(dtSec*.5),  k2v = gravAccel(pos + k1p*(dtSec*.5),  sim) + thrustAccKmS2;
    glm::dvec3 k3p = vel + k2v*(dtSec*.5),  k3v = gravAccel(pos + k2p*(dtSec*.5),  sim) + thrustAccKmS2;
    glm::dvec3 k4p = vel + k3v*dtSec,       k4v = gravAccel(pos + k3p*dtSec,       sim) + thrustAccKmS2;
    nBodyAbsolutePosKm  = pos + (k1p + k2p*2.0 + k3p*2.0 + k4p) * (dtSec/6.0);
    nBodyAbsoluteVelKmS = vel + (k1v + k2v*2.0 + k3v*2.0 + k4v) * (dtSec/6.0);
}

// updateNBodyParentBody
// Purpose: Find which planet's SOI the spacecraft is currently inside and update parentBodyIdx.
// Inputs:  sim - solar system state with planet positions and SOI radii
// Actions: Loops over all planets, computes the spacecraft's distance-to-SOI ratio for each,
//          and sets parentBodyIdx to the planet with the smallest ratio below 1.0.
void Spacecraft::updateNBodyParentBody(const SolarSystem& sim) {
    int    bestBodyIdx  = 0;    // default to Sun
    double bestRatio    = 1e30;
    for (int i = 1; i < (int)sim.bodyCount(); ++i) {
        float  soiKm  = sim.body((size_t)i).soiRadiusKm;
        if (soiKm <= 0.0f) continue;
        double dist   = glm::length(nBodyAbsolutePosKm - sim.planetRealPosKm((size_t)i));
        double ratio  = dist / (double)soiKm;
        if (ratio < 1.0 && ratio < bestRatio) { bestRatio = ratio; bestBodyIdx = i; }
    }
    parentBodyIdx = bestBodyIdx;
}

// updateNBody
// Purpose: Propagate the spacecraft using full N-body RK4 integration for one simulation timestep.
// Inputs:  simDtSec      - simulated time in seconds
//          sim           - solar system for gravity and planet positions
//          thrustAccKmS2 - thrust acceleration to apply during integration (km/s^2)
// Actions: Initializes heliocentric state on first call, then sub-steps RK4 with a maximum
//          10-second step size, updates the parent body, and derives local/world positions.
void Spacecraft::updateNBody(float simDtSec, const SolarSystem& sim,
                             glm::dvec3 thrustAccKmS2) {
    if (!nBodyInitialized) {
        nBodyAbsolutePosKm  = sim.planetRealPosKm((size_t)parentBodyIdx) + statePosKm;
        nBodyAbsoluteVelKmS = sim.planetVelKmS((size_t)parentBodyIdx)    + stateVelKmS;
        nBodyInitialized = true;
    }

    // Sub-step: cap each integration step at 10 sim-seconds, max 1000 steps per frame
    const double maxStepSec = 10.0;
    int numSteps = std::min(1000, std::max(1, (int)std::ceil((double)simDtSec / maxStepSec)));
    double stepSec = (double)simDtSec / numSteps;
    for (int i = 0; i < numSteps; ++i)
        rk4Step(stepSec, sim, thrustAccKmS2);

    updateNBodyParentBody(sim);

    // Convert absolute state to parent-relative for display and LVLH computation
    statePosKm  = nBodyAbsolutePosKm  - sim.planetRealPosKm((size_t)parentBodyIdx);
    stateVelKmS = nBodyAbsoluteVelKmS - sim.planetVelKmS((size_t)parentBodyIdx);
    float kmToU = sim.kmToSceneUnits();
    localPositionSceneUnits = glm::vec3(statePosKm) * kmToU;
    localVelocityDir = (glm::length(stateVelKmS) > 1e-9)
                     ? glm::normalize(glm::vec3(stateVelKmS))
                     : glm::vec3(0.0f, 0.0f, 1.0f);
    worldPosition = sim.body((size_t)parentBodyIdx).worldPos + localPositionSceneUnits;
}

// recomputeFromState
// Purpose: Refresh all Keplerian orbit parameters from the current state vectors.
// Actions: Calls elementsFromState to get orbital elements, then rebuilds orbitalFrame
//          and finds the current mean anomaly from the spacecraft's position on the ellipse.
//          Sets semiMajorAxisKm = 0 if the trajectory is hyperbolic or escape.
void Spacecraft::recomputeFromState() {
    double mu = (double)gravParamKm3S2;
    OrbitalElements elements = elementsFromState(statePosKm, stateVelKmS, mu);

    if (elements.semiMajorAxisKm <= 0.0) {
        semiMajorAxisKm = 0.0f;   // signals hyperbolic/escape to update()
        return;
    }

    semiMajorAxisKm   = (float)elements.semiMajorAxisKm;
    semiMinorAxisKm   = (float)elements.semiMinorAxisKm;
    eccentricity      = (float)elements.eccentricity;
    orbitalPeriodDays = (float)elements.periodDays;
    orbitalFrame      = computeOrbitalFrameFromState(statePosKm, stateVelKmS, mu);

    // Find current mean anomaly from position on the new ellipse
    glm::vec3 posInOrbitalPlane = glm::transpose(orbitalFrame) * glm::vec3(statePosKm);
    float cosE = std::clamp(posInOrbitalPlane.x / semiMajorAxisKm + eccentricity, -1.0f, 1.0f);
    float sinE = (semiMinorAxisKm > 0.0f) ? posInOrbitalPlane.y / semiMinorAxisKm : 0.0f;
    float eccentricAnomaly = std::atan2(sinE, cosE);
    meanAnomaly = eccentricAnomaly - eccentricity * std::sin(eccentricAnomaly);
}

// elementsFromState
// Purpose: Derive classical Keplerian orbital elements from position/velocity state vectors.
// Inputs:  posKm   - position relative to parent body, km
//          velKmS  - velocity relative to parent body, km/s
//          mu      - gravitational parameter of parent body, km^3/s^2
// Actions: Uses the vis-viva equation, specific angular momentum, and eccentricity vector
//          to compute semi-major axis, eccentricity, inclination, and period.
// Returns: OrbitalElements struct; semiMajorAxisKm <= 0 indicates a hyperbolic trajectory.
OrbitalElements Spacecraft::elementsFromState(glm::dvec3 posKm, glm::dvec3 velKmS, double mu) {
    double dist  = glm::length(posKm);
    double speed = glm::length(velKmS);
    if (dist < 1.0 || speed < 1e-9 || mu <= 0.0) return {};

    double specificEnergy = speed * speed * 0.5 - mu / dist;
    double semiMajorAxis  = (specificEnergy < 0.0) ? -mu / (2.0 * specificEnergy) : 0.0;

    glm::dvec3 angMomentum = glm::cross(posKm, velKmS);
    double     angMomLen   = glm::length(angMomentum);

    glm::dvec3 eccVec = glm::cross(velKmS, angMomentum) / mu - posKm / dist;
    double     ecc    = glm::length(eccVec);

    double semiMinorAxis = (semiMajorAxis > 0.0 && ecc < 1.0)
                         ? semiMajorAxis * std::sqrt(1.0 - ecc * ecc)
                         : 0.0;

    double inclinationDeg = (angMomLen > 0.0)
                          ? glm::degrees(std::acos(std::clamp(angMomentum.y / angMomLen, -1.0, 1.0)))
                          : 0.0;

    // In Y-up right-handed coords, prograde (CCW from +Y) orbits have angMomentum.y <= 0
    bool isPrograde = (angMomentum.y <= 0.0);

    double periodSec = (semiMajorAxis > 0.0)
                     ? TWO_PId * std::sqrt(semiMajorAxis * semiMajorAxis * semiMajorAxis / mu)
                     : 0.0;

    return { semiMajorAxis, semiMinorAxis, ecc, inclinationDeg, isPrograde, periodSec / 86400.0 };
}
