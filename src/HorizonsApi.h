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

// Call once at app startup to initialize libcurl global state.
void initGlobal();
// Call once at app shutdown to clean up libcurl global state.
void shutdownGlobal();

// Fetch the heliocentric state vector for one planet identified by its Horizons
// object ID (e.g. "399" for Earth) at the given UTC timestamp ("YYYY-MM-DD HH:MM").
// Writes results to out and returns true on success, false on network or parse failure.
bool fetchOne(const std::string& planetId, const std::string& timestamp,
              PlanetState& out);

// Fetch state vectors for all 8 planets at the given UTC timestamp.
// Keys in the returned map are planet names ("Mercury", "Venus", ..., "Neptune").
// Planets that fail to fetch are absent from the map; callers can fall back to analytic positions.
std::unordered_map<std::string, PlanetState> fetchAllPlanets(
    const std::string& timestamp);

} // namespace HorizonsApi
