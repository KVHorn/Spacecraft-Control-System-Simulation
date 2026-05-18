#include "Spacecraft.h"
#include "SolarSystem.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <algorithm>
#include <iostream>

static constexpr float  TWO_PI  = 6.28318530717958647692f;
static constexpr double TWO_PId = 6.28318530717958647692;

static float solveKepler(float M, float e) {
    while (M >  3.14159265f) M -= TWO_PI;
    while (M < -3.14159265f) M += TWO_PI;
    float E = M;
    for (int i = 0; i < 12; ++i) {
        float f  = E - e * std::sin(E) - M;
        float fp = 1.0f - e * std::cos(E);
        float d  = f / fp;
        E -= d;
        if (std::fabs(d) < 1e-7f) break;
    }
    return E;
}

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
        // SpaceX Crew Dragon: 16× Draco biprop thrusters for orbital maneuvering
        {"Dragon",          {"Draco x16",             6400.0f,  300.0f,   1388.0f,   9616.0f,  8228.0f}},
        // Apollo-Soyuz: Apollo CSM Service Propulsion System AJ10-137
        {"Apollo Soyuz",    {"AJ10-137 SPS",         91190.0f, 314.5f,  18410.0f,  28800.0f, 10390.0f}},
        // Ares I upper stage: J-2X (rocket stage, not a pilotable spacecraft)
        {"Ares 1",          {"J-2X",               1307000.0f, 448.0f, 135100.0f, 172000.0f, 36900.0f}},
        // Cassini orbiter with Huygens attached: R-4D biprop (490 N, one engine fires at a time)
        {"Cassini",         {"R-4D",                   490.0f, 312.0f,   3132.0f,   5655.0f,  2523.0f}},
        // Chandra: TR-308 biprop apogee thrusters (4×472 N, used only for orbit insertion)
        {"Chandra",         {"TR-308",                 472.0f, 322.0f,   1070.0f,   5860.0f,  4790.0f}},
        // CloudSat: 4× hydrazine monoprop thrusters, attitude/translation only
        {"CloudSat",        {"Hydrazine ×4",            17.8f, 220.0f,    147.0f,    995.0f,   848.0f}},
        // Deep Space 1: NSTAR ion engine — 92 mN at max, Isp 3100 s
        {"Deep Space 1",    {"NSTAR Ion",              0.092f,3100.0f,     82.0f,    486.0f,   373.0f}},
        // Hubble: no propulsion; reboosts from visiting Shuttle only
        {"Hubble",          {"(none)",                  0.0f,   0.0f,      0.0f,  11110.0f, 11110.0f}},
        // JWST: SCAT biprop thrusters (thrust estimate; exact value not public)
        {"James Webb",      {"SCAT biprop",            220.0f, 310.0f,    301.0f,   6200.0f,  5899.0f}},
        // Juno: LEROS-1b biprop main engine
        {"Juno",            {"LEROS-1b",               645.0f, 317.0f,   2032.0f,   3625.0f,  1593.0f}},
        // Magellan post-VOI: 16× 445 N hydrazine (only a few fire at once; use single-thruster value)
        {"Magellan",        {"Hydrazine MRM",          445.0f, 220.0f,    133.0f,   1168.0f,  1035.0f}},
        // Mars Global Surveyor: LEROS-1b (slightly derated to 596 N as-flown)
        {"Mars Global",     {"LEROS-1b",               596.0f, 318.0f,    361.0f,   1030.0f,   669.0f}},
        // MRO: 6× MR-106 hydrazine monoprop (170 N each) for MOI; 1020 N combined
        {"Mars Recon",      {"MR-106 ×6",             1020.0f, 228.0f,   1149.0f,   2180.0f,  1031.0f}},
        // Nancy Grace Roman: 4× 22 N hydrazine at L2
        {"Nancy",           {"Hydrazine ×4",            88.0f, 220.0f,   1100.0f,   4166.0f,  3066.0f}},
        // Space Shuttle OMS: 2× AJ10-190 (53,400 N combined)
        {"Space Shuttle",   {"AJ10-190 OMS ×2",      53400.0f, 316.0f,  10830.0f, 110000.0f, 78000.0f}},
        // Voyager 1 & 2: 16× MR-103 hydrazine thrusters (~1.1 N each; ~4 active at once)
        {"Voyager",         {"MR-103 ×16",              4.48f, 227.0f,    100.0f,    815.0f,   715.0f}},
    };

    for (const auto& e : table)
        if (stem.find(e.key) != std::string::npos) return e.data;

    // Unknown model: provide a modest generic thruster so the spacecraft isn't dead
    return {"Generic thruster", 100.0f, 220.0f, 50.0f, 1000.0f, 950.0f};
}

void Spacecraft::init(const MissionConfig& cfg, const SolarSystem& sim) {
    config = cfg;

    parentBodyIdx = std::clamp(cfg.startingBody, 0, (int)sim.bodyCount() - 1);
    size_t parentIdx = (size_t)parentBodyIdx;
    const Planet& parent = sim.body(parentIdx);

    muKm3PerS2 = (parent.muKm3PerS2 > 0.0f) ? parent.muKm3PerS2 : 3.986e5f;

    a = std::max(cfg.semiMajorKm, 100.0f);
    b = std::clamp(cfg.semiMinorKm, 100.0f, a);
    float ba = b / a;
    e = std::sqrt(std::max(0.0f, 1.0f - ba * ba));
    inclRad = glm::radians(cfg.inclinationDeg);
    dir = cfg.prograde ? 1.0f : -1.0f;

    float T_sec = TWO_PI * std::sqrt((a * a * a) / muKm3PerS2);
    periodDays = T_sec / 86400.0f;

    meanAnomaly = 0.0f;
    nbodyInitialized = false;
    attitudeUser = glm::quat(1, 0, 0, 0);
    angularVel   = glm::vec3(0.0f);

    engineData    = lookupEngineData(cfg.modelFile);
    propellantKg  = engineData.propellantMassKg;
    throttle      = 0.0f;
    engineOn      = false;

    // Build orbital frame: periapsis along +X, normal encodes prograde/retrograde.
    // normalDir = (0, -sin(i), cos(i)) for prograde; flipped for retrograde.
    float ci = std::cos(inclRad), si = std::sin(inclRad);
    glm::vec3 col2(0.0f, -si, ci);
    if (!cfg.prograde) col2 = -col2;
    glm::vec3 col0(1.0f, 0.0f, 0.0f);
    glm::vec3 col1 = glm::normalize(glm::cross(col2, col0));
    orbFrame = glm::mat3(col0, col1, col2);

    renderScale = computeRenderScale(1.0f); // fallback; gets updated when model loads
}

float Spacecraft::computeRenderScale(float modelRadius) const {
    // 1 scene unit = 1000 km = 1,000,000 m → 1 m = 1e-6 units
    if (config.realScale) {
        // In real-scale mode we treat "lengthMeters" as the spacecraft's
        // visible size in meters. Ignore modelRadius; caller handles model scale
        // independently via a separate factor.
        return std::max(1e-6f, config.lengthMeters * 1.0e-6f);
    }
    // Scale to ~1% of orbit semi-major axis so the ship looks small next to planets.
    return std::clamp(a * 8.0e-6f, 0.005f, 0.8f);
}

void Spacecraft::applyTorque(const glm::vec3& bodyTorque, float dtSec) {
    angularVel += bodyTorque * dtSec;
}

void Spacecraft::stopRotation() {
    angularVel = glm::vec3(0.0f);
}

void Spacecraft::update(float simDtDays, float realDtSec, const SolarSystem& sim) {
    float simDtSec = simDtDays * 86400.0f;

    // --- Thrust: using last frame's world attitude (computed end of previous frame) ---
    float thrustN = currentThrustN();
    glm::dvec3 thrustDv(0.0);
    glm::dvec3 thrustAccKmS2(0.0);
    if (thrustN > 0.0f) {
        float mass = currentMassKg();
        if (mass > 0.0f) {
            // Body -Z is the prograde / "forward" direction in LVLH convention
            glm::vec3 thrustDir = -(glm::mat3_cast(attitudeWorld)[2]);
            float accKmS2 = thrustN / mass / 1000.0f;
            thrustDv      = glm::dvec3(thrustDir) * (double)(accKmS2 * simDtSec);
            thrustAccKmS2 = glm::dvec3(thrustDir) * (double)accKmS2;
        }
        float dm = thrustN / (engineData.ispSec * 9.80665f) * simDtSec;
        propellantKg = std::max(0.0f, propellantKg - dm);
    }

    // N-body mode: bypass Kepler entirely; worldPos/localPos set inside updateNBody
    if (config.nBody) {
        updateNBody(simDtSec, sim, thrustAccKmS2);
    } else if (a <= 0.0f) {
        // Escape/hyperbolic: linear propagation
        statePos += stateVel * (double)simDtSec;
        stateVel += thrustDv;
        checkSOI(sim);
        float kmToU = sim.kmToSceneUnits();
        localPos = glm::vec3(statePos) * kmToU;
        localVel = (glm::length(stateVel) > 1e-9)
                 ? glm::normalize(glm::vec3(stateVel))
                 : glm::vec3(0.0f, 0.0f, 1.0f);
        worldPos = sim.body((size_t)parentBodyIdx).worldPos + localPos;
        if (thrustN > 0.0f) recomputeFromState();
    } else {
        // --- Advance mean anomaly (retrograde encoded in orbFrame, so always +) ---
        meanAnomaly += (TWO_PI / periodDays) * simDtDays;

        // --- Kepler position/velocity in orbital plane ---
        float E = solveKepler(meanAnomaly, e);
        float xp = a * (std::cos(E) - e);
        float yp = b * std::sin(E);
        float denom = 1.0f - e * std::cos(E);
        float vxN = -std::sin(E) / denom;
        float vyN = (b / a) * std::cos(E) / denom;
        float vlen = std::sqrt(vxN*vxN + vyN*vyN);
        if (vlen > 0.0f) { vxN /= vlen; vyN /= vlen; }

        statePos = glm::dvec3(orbFrame * glm::vec3(xp, yp, 0.0f));
        double r_km = glm::length(statePos);
        double v2   = (r_km > 0.0) ? (double)muKm3PerS2 * (2.0/r_km - 1.0/a) : 0.0;
        double vKmS = (v2 > 0.0) ? std::sqrt(v2) : 0.0;
        glm::vec3 velDir = orbFrame * glm::vec3(vxN, vyN, 0.0f);
        stateVel  = glm::dvec3(velDir) * vKmS;
        localVel  = glm::normalize(velDir);

        // Apply thrust impulse
        if (thrustN > 0.0f) {
            stateVel += thrustDv;
            localVel  = (glm::length(stateVel) > 1e-9)
                      ? glm::normalize(glm::vec3(stateVel))
                      : localVel;
            recomputeFromState();
        }

        // SOI check + derive scene positions
        checkSOI(sim);
        float kmToU = sim.kmToSceneUnits();
        localPos = glm::vec3(statePos) * kmToU;
        worldPos = sim.body((size_t)parentBodyIdx).worldPos + localPos;
    }

    // --- LVLH frame (body-to-world quaternion) ---
    // Forward axis (+Z in body frame when attitudeUser is identity) = velocity.
    // Up axis (+Y) = radial outward.
    glm::vec3 radialOut = glm::normalize(localPos);
    glm::vec3 fwd       = glm::normalize(localVel);
    glm::vec3 rightVec  = glm::normalize(glm::cross(radialOut, fwd));
    glm::vec3 upVec     = glm::normalize(glm::cross(fwd, rightVec));
    glm::mat3 Rlvlh;
    Rlvlh[0] =  rightVec;
    Rlvlh[1] =  upVec;
    Rlvlh[2] = -fwd;
    attitudeLvlh = glm::quat_cast(Rlvlh);

    // --- Integrate user attitude from angular velocity ---
    // Apply damping so inputs fade away naturally (RCS isn't perfect).
    float damp = std::pow(0.985f, std::max(0.0f, realDtSec * 60.0f));
    angularVel *= damp;

    float w = glm::length(angularVel);
    if (w > 1e-8f && realDtSec > 0.0f) {
        glm::vec3 axis = angularVel / w;
        float angle = w * realDtSec;
        glm::quat dq = glm::angleAxis(angle, axis);
        // Compose on the right so torque is applied in body frame
        attitudeUser = glm::normalize(attitudeUser * dq);
    }

    // --- Compose world attitude ---
    attitudeWorld = glm::normalize(attitudeLvlh * attitudeUser);
}

glm::vec3 Spacecraft::getEulerDegreesLvlh() const {
    // Extract pitch/yaw/roll from attitudeUser (relative to LVLH)
    const glm::quat& q = attitudeUser;
    float qw = q.w, qx = q.x, qy = q.y, qz = q.z;
    glm::vec3 out;
    float sinp = 2.0f * (qw * qy - qz * qx);
    sinp = std::clamp(sinp, -1.0f, 1.0f);
    out.y = glm::degrees(std::asin(sinp));
    out.x = glm::degrees(std::atan2(2.0f * (qw * qx + qy * qz),
                                    1.0f - 2.0f * (qx * qx + qy * qy)));
    out.z = glm::degrees(std::atan2(2.0f * (qw * qz + qx * qy),
                                    1.0f - 2.0f * (qy * qy + qz * qz)));
    return out;
}

float Spacecraft::getOrbitalSpeedKmS() const {
    return (float)glm::length(stateVel);
}

void Spacecraft::checkSOI(const SolarSystem& sim) {
    glm::dvec3 absPos = sim.planetRealPosKm((size_t)parentBodyIdx) + statePos;

    // Current parent ratio (distance / SOI). Sun's SOI is infinite → ratio = 1e30.
    double currentRatio = 1e30;
    if (parentBodyIdx != 0) {
        float parentSOI = sim.body((size_t)parentBodyIdx).soiRadiusKm;
        double rParent  = glm::length(absPos - sim.planetRealPosKm((size_t)parentBodyIdx));
        if (parentSOI > 0.0f) currentRatio = rParent / (double)parentSOI;
        // Escaped current parent's SOI → fall back to Sun
        if (rParent > (double)parentSOI * 1.05) {
            switchParent(0, sim);
            return;
        }
    }

    // Find any planet whose SOI we're entering more deeply
    int    bestBody  = parentBodyIdx;
    double bestRatio = currentRatio;
    for (int i = 1; i < (int)sim.bodyCount(); ++i) {
        if (i == parentBodyIdx) continue;
        float soiKm = sim.body((size_t)i).soiRadiusKm;
        if (soiKm <= 0.0f) continue;
        double r     = glm::length(absPos - sim.planetRealPosKm((size_t)i));
        double ratio = r / (double)soiKm;
        if (ratio < bestRatio * 0.95 && ratio < bestRatio) {
            bestRatio = ratio;
            bestBody  = i;
        }
    }

    if (bestBody != parentBodyIdx)
        switchParent(bestBody, sim);
}

void Spacecraft::switchParent(int newIdx, const SolarSystem& sim) {
    std::cout << "[SOI] " << sim.body((size_t)parentBodyIdx).name
              << " -> " << sim.body((size_t)newIdx).name << "\n";

    glm::dvec3 absVel      = stateVel + sim.planetVelKmS((size_t)parentBodyIdx);
    glm::dvec3 newStatePos = statePos + sim.planetRealPosKm((size_t)parentBodyIdx)
                                      - sim.planetRealPosKm((size_t)newIdx);
    glm::dvec3 newStateVel = absVel   - sim.planetVelKmS((size_t)newIdx);

    parentBodyIdx  = newIdx;
    muKm3PerS2     = sim.body((size_t)newIdx).muKm3PerS2;
    statePos       = newStatePos;
    stateVel       = newStateVel;
    recomputeFromState();
}

glm::mat3 Spacecraft::orbFrameFromState(glm::dvec3 posKm, glm::dvec3 velKmS, double mu) {
    glm::dvec3 h  = glm::cross(posKm, velKmS);
    glm::dvec3 ev = glm::cross(velKmS, h) / mu - glm::normalize(posKm);
    double eMag   = glm::length(ev);
    glm::dvec3 col0 = (eMag > 1e-9) ? glm::normalize(ev) : glm::normalize(posKm);
    glm::dvec3 col2 = glm::normalize(h);
    glm::dvec3 col1 = glm::normalize(glm::cross(col2, col0));
    return glm::mat3(glm::vec3(col0), glm::vec3(col1), glm::vec3(col2));
}

// Gravitational acceleration at absolute heliocentric position pos (km/s²).
static glm::dvec3 gravAccel(glm::dvec3 absPos, const SolarSystem& sim) {
    glm::dvec3 acc(0.0);
    for (size_t i = 0; i < sim.bodyCount(); ++i) {
        double mu = (double)sim.body(i).muKm3PerS2;
        if (mu <= 0.0) continue;
        glm::dvec3 r = sim.planetRealPosKm(i) - absPos;
        double dist2 = glm::dot(r, r);
        if (dist2 < 1.0) continue;  // closer than 1 km — skip (inside body)
        acc += r * (mu / (dist2 * std::sqrt(dist2)));
    }
    return acc;
}

void Spacecraft::rk4Step(double dtSec, const SolarSystem& sim, glm::dvec3 thrustAccKmS2) {
    glm::dvec3 p = nbodyAbsPos, v = nbodyAbsVel;
    glm::dvec3 k1p = v,               k1v = gravAccel(p,               sim) + thrustAccKmS2;
    glm::dvec3 k2p = v+k1v*(dtSec*.5),k2v = gravAccel(p+k1p*(dtSec*.5),sim) + thrustAccKmS2;
    glm::dvec3 k3p = v+k2v*(dtSec*.5),k3v = gravAccel(p+k2p*(dtSec*.5),sim) + thrustAccKmS2;
    glm::dvec3 k4p = v+k3v*dtSec,     k4v = gravAccel(p+k3p*dtSec,     sim) + thrustAccKmS2;
    nbodyAbsPos = p + (k1p + k2p*2.0 + k3p*2.0 + k4p) * (dtSec/6.0);
    nbodyAbsVel = v + (k1v + k2v*2.0 + k3v*2.0 + k4v) * (dtSec/6.0);
}

void Spacecraft::updateNBodyParent(const SolarSystem& sim) {
    int    best      = 0;           // default to Sun
    double bestRatio = 1e30;
    for (int i = 1; i < (int)sim.bodyCount(); ++i) {
        float  soiKm = sim.body((size_t)i).soiRadiusKm;
        if (soiKm <= 0.0f) continue;
        double r     = glm::length(nbodyAbsPos - sim.planetRealPosKm((size_t)i));
        double ratio = r / (double)soiKm;
        if (ratio < 1.0 && ratio < bestRatio) { bestRatio = ratio; best = i; }
    }
    parentBodyIdx = best;
}

void Spacecraft::updateNBody(float simDtSec, const SolarSystem& sim,
                             glm::dvec3 thrustAccKmS2) {
    if (!nbodyInitialized) {
        nbodyAbsPos = sim.planetRealPosKm((size_t)parentBodyIdx) + statePos;
        nbodyAbsVel = sim.planetVelKmS((size_t)parentBodyIdx)    + stateVel;
        nbodyInitialized = true;
    }

    // Sub-step: cap each integration step at 10 sim-seconds, max 1000 steps/frame
    const double maxStep = 10.0;
    int nSteps = std::min(1000, std::max(1, (int)std::ceil((double)simDtSec / maxStep)));
    double step = (double)simDtSec / nSteps;
    for (int i = 0; i < nSteps; ++i)
        rk4Step(step, sim, thrustAccKmS2);

    updateNBodyParent(sim);

    // Convert absolute state → parent-relative for display / LVLH
    statePos = nbodyAbsPos - sim.planetRealPosKm((size_t)parentBodyIdx);
    stateVel = nbodyAbsVel - sim.planetVelKmS((size_t)parentBodyIdx);
    float kmToU = sim.kmToSceneUnits();
    localPos = glm::vec3(statePos) * kmToU;
    localVel = (glm::length(stateVel) > 1e-9)
             ? glm::normalize(glm::vec3(stateVel))
             : glm::vec3(0.0f, 0.0f, 1.0f);
    worldPos = sim.body((size_t)parentBodyIdx).worldPos + localPos;
}

void Spacecraft::recomputeFromState() {
    double mu = (double)muKm3PerS2;
    OrbElements el = elementsFromState(statePos, stateVel, mu);

    if (el.a <= 0.0) {
        a = 0.0f;   // signal hyperbolic/escape to update()
        return;
    }

    a          = (float)el.a;
    b          = (float)el.b;
    e          = (float)el.e;
    periodDays = (float)el.periodDays;
    orbFrame   = orbFrameFromState(statePos, stateVel, mu);

    // Find current mean anomaly from position on the new ellipse
    glm::vec3 posOrb = glm::transpose(orbFrame) * glm::vec3(statePos);
    float cosE = std::clamp(posOrb.x / a + e, -1.0f, 1.0f);
    float sinE = (b > 0.0f) ? posOrb.y / b : 0.0f;
    float E    = std::atan2(sinE, cosE);
    meanAnomaly = E - e * std::sin(E);
}

OrbElements Spacecraft::elementsFromState(glm::dvec3 posKm, glm::dvec3 velKmS, double mu) {
    double r   = glm::length(posKm);
    double v   = glm::length(velKmS);
    if (r < 1.0 || v < 1e-9 || mu <= 0.0) return {};

    double eps = v * v * 0.5 - mu / r;           // specific orbital energy
    double a_d = (eps < 0.0) ? -mu / (2.0 * eps) : 0.0;

    glm::dvec3 h   = glm::cross(posKm, velKmS);  // specific angular momentum
    double     hLen = glm::length(h);

    glm::dvec3 ev = glm::cross(velKmS, h) / mu - posKm / r;
    double     e_d = glm::length(ev);

    double b_d = (a_d > 0.0 && e_d < 1.0) ? a_d * std::sqrt(1.0 - e_d * e_d) : 0.0;

    double incDeg = (hLen > 0.0) ? glm::degrees(std::acos(std::clamp(h.y / hLen, -1.0, 1.0))) : 0.0;

    // In Y-up right-handed coords, prograde (CCW from +Y) orbits have h.y <= 0
    bool prog = (h.y <= 0.0);

    double T_sec = (a_d > 0.0) ? TWO_PId * std::sqrt(a_d * a_d * a_d / mu) : 0.0;

    return { a_d, b_d, e_d, incDeg, prog, T_sec / 86400.0 };
}
