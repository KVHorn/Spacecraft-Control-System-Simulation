#include "HorizonsApi.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using nlohmann::json;

namespace HorizonsApi {

static bool g_curlInited = false;

void initGlobal() {
    if (g_curlInited) return;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    g_curlInited = true;
}

void shutdownGlobal() {
    if (!g_curlInited) return;
    curl_global_cleanup();
    g_curlInited = false;
}

static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::string* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), total);
    return total;
}

static std::string urlEncode(CURL* curl, const std::string& value) {
    char* enc = curl_easy_escape(curl, value.c_str(), (int)value.length());
    if (!enc) throw std::runtime_error("curl_easy_escape failed");
    std::string r(enc);
    curl_free(enc);
    return r;
}

static std::string httpGet(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init failed");

    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "SpacecraftControlSystemSimulation/1.0");

    CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        std::string msg = curl_easy_strerror(code);
        curl_easy_cleanup(curl);
        throw std::runtime_error("HTTP GET failed: " + msg);
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (status != 200)
        throw std::runtime_error("HTTP status " + std::to_string(status));
    return body;
}

static std::string buildUrl(const std::string& planetId,
                            const std::string& startTime) {
    const std::string base = "https://ssd.jpl.nasa.gov/api/horizons.api";
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init failed");

    // Build +1d stop time so Horizons returns a row
    auto plusOneDay = [](const std::string& t) {
        // quick-and-dirty: only touches the date part
        if (t.size() < 10) return t;
        int y, m, d;
        if (std::sscanf(t.c_str(), "%4d-%2d-%2d", &y, &m, &d) != 3) return t;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %s",
                      y, m, d + 1, t.substr(11).c_str());
        return std::string(buf);
    };

    std::vector<std::pair<std::string, std::string>> params = {
        {"format",      "json"},
        {"COMMAND",     "'" + planetId + "'"},
        {"MAKE_EPHEM",  "'YES'"},
        {"EPHEM_TYPE",  "'VECTORS'"},
        {"CENTER",      "'500@10'"},       // Sun barycenter
        {"START_TIME",  "'" + startTime + "'"},
        {"STOP_TIME",   "'" + plusOneDay(startTime) + "'"},
        {"STEP_SIZE",   "'1 d'"},
        {"OUT_UNITS",   "'KM-S'"},
        {"REF_SYSTEM",  "'J2000'"},
        {"VEC_TABLE",   "'3'"},            // position + velocity
        {"VEC_LABELS",  "'NO'"},
        {"CSV_FORMAT",  "'YES'"},
        {"OBJ_DATA",    "'NO'"}
    };

    std::ostringstream url;
    url << base << "?";
    for (size_t i = 0; i < params.size(); ++i) {
        url << params[i].first << "=" << urlEncode(curl, params[i].second);
        if (i + 1 < params.size()) url << "&";
    }
    curl_easy_cleanup(curl);
    return url.str();
}

// Parse a line like "2460387.500, A.D. 2026-Mar-17 00:00, X, Y, Z, VX, VY, VZ, LT, RG, RR"
// With VEC_TABLE=3 we get: JD, calendar, X, Y, Z, VX, VY, VZ (+ extras)
static bool parseResult(const std::string& text, PlanetState& st) {
    std::regex blockRe(R"(\$\$SOE([\s\S]*?)\$\$EOE)");
    std::smatch m;
    if (!std::regex_search(text, m, blockRe)) return false;

    std::string block = m[1].str();
    std::istringstream ss(block);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.find_first_not_of(" \t\r\n") != std::string::npos) break;
    }
    if (line.empty()) return false;

    std::vector<std::string> parts;
    std::stringstream ls(line);
    std::string item;
    while (std::getline(ls, item, ',')) {
        size_t a = item.find_first_not_of(" \t\r\n");
        size_t b = item.find_last_not_of(" \t\r\n");
        parts.push_back(a == std::string::npos ? "" : item.substr(a, b - a + 1));
    }
    if (parts.size() < 8) return false;

    try {
        st.xKm   = std::stod(parts[2]);
        st.yKm   = std::stod(parts[3]);
        st.zKm   = std::stod(parts[4]);
        st.vxKmS = std::stod(parts[5]);
        st.vyKmS = std::stod(parts[6]);
        st.vzKmS = std::stod(parts[7]);
    } catch (...) { return false; }
    return true;
}

bool fetchOne(const std::string& planetId, const std::string& timestamp,
              PlanetState& out) {
    try {
        std::string url  = buildUrl(planetId, timestamp);
        std::string resp = httpGet(url);
        json parsed = json::parse(resp);
        if (!parsed.contains("result")) return false;
        std::string result = parsed["result"].get<std::string>();
        return parseResult(result, out);
    } catch (const std::exception& ex) {
        std::cerr << "[Horizons] fetch " << planetId << " failed: " << ex.what() << "\n";
        return false;
    }
}

std::unordered_map<std::string, PlanetState> fetchAllPlanets(
    const std::string& timestamp) {
    std::unordered_map<std::string, PlanetState> result;
    const std::vector<std::pair<std::string, std::string>> planets = {
        {"Mercury", "199"}, {"Venus",   "299"},
        {"Earth",   "399"}, {"Mars",    "499"},
        {"Jupiter", "599"}, {"Saturn",  "699"},
        {"Uranus",  "799"}, {"Neptune", "899"}
    };
    std::cout << "[Horizons] Fetching state vectors at " << timestamp << "...\n";
    for (const auto& [name, id] : planets) {
        PlanetState st;
        if (fetchOne(id, timestamp, st)) {
            result[name] = st;
            std::cout << "  " << name << ": ("
                      << st.xKm << ", " << st.yKm << ", " << st.zKm << ") km\n";
        } else {
            std::cout << "  " << name << ": FAILED\n";
        }
    }
    return result;
}

} // namespace HorizonsApi
