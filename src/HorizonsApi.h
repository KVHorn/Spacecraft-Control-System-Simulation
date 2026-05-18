#pragma once
#include <string>
#include <unordered_map>

// Queries JPL Horizons for heliocentric J2000 state vectors of the 8 planets.
// Values are returned in KM with the Sun at origin.
//
// Usage:
//   HorizonsApi::initGlobal();
//   auto data = HorizonsApi::fetchAllPlanets("2026-03-17 00:00");
//   HorizonsApi::shutdownGlobal();
//
// `fetchAllPlanets` returns an empty map if ALL planets failed (offline, etc.);
// otherwise it returns whatever it successfully retrieved, so missing planets
// can fall back to analytic positions.
namespace HorizonsApi {

struct PlanetState {
    double xKm = 0.0, yKm = 0.0, zKm = 0.0;   // position (J2000 sun-centered)
    double vxKmS = 0.0, vyKmS = 0.0, vzKmS = 0.0; // velocity km/s
};

// Call once at app startup / shutdown for libcurl global init.
void initGlobal();
void shutdownGlobal();

// Fetch state for a single planet (by Horizons object id, e.g. "399" for Earth)
bool fetchOne(const std::string& planetId, const std::string& timestamp,
              PlanetState& out);

// Fetch all 8 planets. Keys are "Mercury", "Venus", ..., "Neptune".
std::unordered_map<std::string, PlanetState> fetchAllPlanets(
    const std::string& timestamp);

} // namespace HorizonsApi
