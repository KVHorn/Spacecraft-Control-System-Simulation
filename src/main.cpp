#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <vector>
#include <string>
#include <ctime>

#include "Shader.h"
#include "Mesh.h"
#include "Camera.h"
#include "SolarSystem.h"
#include "Spacecraft.h"
#include "Model.h"
#include "HorizonsApi.h"
#include "Texture.h"
#include <array>
#include <unordered_map>

// ----------------------------------------------------------------------------
// App state
// ----------------------------------------------------------------------------
enum class ViewMode { SolarSystem, Planet, Spacecraft };

static ViewMode  g_view = ViewMode::SolarSystem;

static GLFWwindow* g_window = nullptr;
static int g_windowWidth = 1600, g_windowHeight = 900;

// Sidebars
static const float kLeftSidebarWidth  = 330.0f;
static const float kRightSidebarWidth = 280.0f;

// Viewport region (3D scene area between sidebars)
static int g_viewportX = 0, g_viewportY = 0, g_viewportWidth = 1, g_viewportHeight = 1;

static OrbitCamera g_camera;

static bool   g_isRightMouseDragging = false;
static double g_lastMouseX = 0, g_lastMouseY = 0;

// Display toggles
static bool g_showPlanetOrbits = true;
static bool g_showPlanetFrames = true;
static bool g_showSpacecraftOrbit     = true;
static bool g_showSpacecraftFrame     = true;

// Display colors (ImVec4 for color pickers; converted to IM_COL32 / glm at use site)
static ImVec4 g_planetOrbitColor = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
static ImVec4 g_planetFrameColor = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);
static ImVec4 g_spacecraftOrbitColor     = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
static ImVec4 g_spacecraftFrameColor     = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

// Skybox / visual
static float g_skyboxBrightness = 0.85f;
static float g_skyboxYaw        = 0.0f;   // Y-axis rotation
static float g_skyboxPitch      = 0.0f;   // X-axis rotation
static float g_skyboxRoll       = 0.0f;   // Z-axis rotation

// Settings / Help popups
static bool g_openSettingsPopup = false;
static bool g_openHelpPopup     = false;

// ----------------------------------------------------------------------------
// Hohmann transfer plan
// ----------------------------------------------------------------------------
struct HohmannPlan {
    bool active   = false;
    bool sameBody = true;
    int  targetBodyIdx = -1;
    double targetPeriKm = 0.0, targetApoKm = 0.0;

    struct Burn {
        double dv_kms     = 0.0;
        double duration_s = 0.0;
        double elapsed_s  = 0.0;
        bool   prograde   = true;
        bool   done       = false;
        glm::dvec3 posRelParent{0.0};  // km, relative to parent body center
    } burns[2];

    int activeBurn = 0;
};
static HohmannPlan g_hohmann;
static bool  g_openHohmannPopup = false;
static int   g_hohmannTargetBodyIdx  = 3;       // Earth default
static float g_hohmannPeriapsisKm         = 6771.0f; // 400 km altitude
static float g_hohmannApoapsisKm          = 42164.0f;// GEO

// colFromVec4
// Purpose: Convert an ImVec4 floating-point color (each channel 0.0-1.0) to a
//          packed IM_COL32 RGBA integer suitable for ImGui draw calls.
// Inputs:  c - RGBA color with each component in [0.0, 1.0]
// Returns: Packed 32-bit RGBA color as ImU32
static ImU32 colFromVec4(const ImVec4& c) {
    return IM_COL32(
        (int)(c.x * 255.0f + 0.5f),
        (int)(c.y * 255.0f + 0.5f),
        (int)(c.z * 255.0f + 0.5f),
        (int)(c.w * 255.0f + 0.5f));
}

static float g_timeScale = 1.0f;  // simulated seconds per real second (1.0 = real time)
static bool  g_paused = false;
static bool  g_showAxes = true;   // debug: draw world XYZ gizmo at origin
static bool  g_showPins = false;  // debug: draw surface pins on each planet
static bool  g_showSOI  = false;  // debug: draw SOI wireframe spheres

static MissionConfig   g_config;
static Spacecraft      g_spacecraft;
static bool            g_missionActive = false;

static std::vector<std::string> g_availableModels;
static int                      g_selectedModelIdx = 0;
static std::unique_ptr<Model>   g_currentModel;

// ----------------------------------------------------------------------------
// Callbacks
// ----------------------------------------------------------------------------
// framebufferSizeCallback
// Purpose: GLFW callback invoked when the OS resizes the window framebuffer.
// Inputs:  w, h - new framebuffer width and height in pixels
// Actions: Updates g_windowWidth/g_windowHeight so the rest of the frame uses the correct size.
static void framebufferSizeCallback(GLFWwindow*, int w, int h) {
    g_windowWidth = w; g_windowHeight = h;
}

// mouseInCenterViewport
// Purpose: Test whether a mouse position falls inside the 3D viewport (the region between the two sidebars).
// Inputs:  mx, my - mouse cursor position in window pixel coordinates
// Returns: true if the cursor is inside the viewport rectangle.
static bool mouseInCenterViewport(double mx, double my) {
    return mx >= g_viewportX && mx < g_viewportX + g_viewportWidth &&
           my >= g_viewportY && my < g_viewportY + g_viewportHeight;
}

// mouseButtonCallback
// Purpose: GLFW callback for mouse button presses; starts or stops right-click camera drag.
// Inputs:  w      - GLFW window handle
//          button - which mouse button was pressed/released
//          action - GLFW_PRESS or GLFW_RELEASE
// Actions: On right-button press inside the viewport, begins tracking the cursor for camera
//          rotation. On release, stops tracking.
static void mouseButtonCallback(GLFWwindow* w, int button, int action, int) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            double mx, my; glfwGetCursorPos(w, &mx, &my);
            if (mouseInCenterViewport(mx, my)) {
                g_isRightMouseDragging = true;
                g_lastMouseX = mx; g_lastMouseY = my;
            }
        } else {
            g_isRightMouseDragging = false;
        }
    }
}

// cursorPosCallback
// Purpose: GLFW callback for cursor movement; rotates the camera during a right-click drag.
// Inputs:  x, y - current cursor position in window pixels
// Actions: Computes pixel delta from last position and forwards it to OrbitCamera::rotate().
static void cursorPosCallback(GLFWwindow*, double x, double y) {
    if (!g_isRightMouseDragging) return;
    float dx = float(x - g_lastMouseX);
    float dy = float(y - g_lastMouseY);
    g_lastMouseX = x; g_lastMouseY = y;
    g_camera.rotate(dx, -dy);
}

// scrollCallback
// Purpose: GLFW callback for scroll wheel; zooms the camera in or out.
// Inputs:  yoff - vertical scroll amount (positive = wheel up = zoom in)
// Actions: Forwards scroll ticks to OrbitCamera::zoom().
static void scrollCallback(GLFWwindow*, double, double yoff) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    g_camera.zoom(float(yoff));
}

// ----------------------------------------------------------------------------
// Camera targeting
// ----------------------------------------------------------------------------
// snapCameraForView
// Purpose: Reposition and re-orient the camera to a sensible initial position for the current view mode.
// Inputs:  sim - solar system used to read planet positions and radii
// Actions: Sets the camera focus, distance, and angle limits appropriate for SolarSystem,
//          Planet, or Spacecraft view. Resets orbit angles to defaults.
static void snapCameraForView(SolarSystem& sim) {
    size_t parentIdx = (size_t)std::clamp(g_config.startingBody, 0,
                                          (int)sim.bodyCount() - 1);
    if (g_view == ViewMode::SolarSystem) {
        g_camera.setFocus(glm::vec3(0.0f), 3500.0f, true, 45.0f, 45.0f);
        g_camera.minDistance = 100.0f;
        g_camera.maxDistance = 2.0e7f;
    } else if (g_view == ViewMode::Planet) {
        float r = sim.planetSceneRadius(parentIdx);
        g_camera.setFocus(sim.body(parentIdx).worldPos, r * 5.0f, true, 0.0f, 45.0f);
        g_camera.minDistance = r * 1.2f;
        g_camera.maxDistance = r * 200.0f;
    } else {
        // Spacecraft view: allow zooming tight enough to see a real-scale craft.
        float s = g_missionActive ? g_spacecraft.renderScale : 5.0f;
        g_camera.setFocus(g_missionActive ? g_spacecraft.worldPosition : glm::vec3(0.0f),
                          s * 6.0f, true, 90.0f, 45.0f);
        g_camera.minDistance = std::max(1e-6f, s * 0.2f); // very close
        g_camera.maxDistance = s * 5000.0f;
    }
}

// trackCameraForView
// Purpose: Update the camera's focus point every frame so it follows the active target.
// Inputs:  sim - solar system for planet world positions
// Actions: In SolarSystem view focuses on the origin; in Planet view tracks the selected planet;
//          in Spacecraft view tracks the active spacecraft's worldPosition.
static void trackCameraForView(SolarSystem& sim) {
    size_t parentIdx = (size_t)std::clamp(g_config.startingBody, 0,
                                          (int)sim.bodyCount() - 1);
    if (g_view == ViewMode::SolarSystem) {
        g_camera.focus = glm::vec3(0.0f);
    } else if (g_view == ViewMode::Planet) {
        g_camera.focus = sim.body(parentIdx).worldPos;
    } else {
        if (g_missionActive) g_camera.focus = g_spacecraft.worldPosition;
    }
    g_camera.update();
}

// ----------------------------------------------------------------------------
// Models
// ----------------------------------------------------------------------------
// scanModelsDirectory
// Purpose: Populate g_availableModels with all .glb/.gltf files found in the model directories.
// Actions: Clears the list, then scans "models/" and "textures/3d SpaceCraft Assets/" for
//          supported 3D model files and appends their full relative paths.
static void scanModelsDirectory() {
    g_availableModels.clear();
    g_availableModels.push_back(""); // index 0 = default sphere

    namespace fs = std::filesystem;
    std::error_code ec;

    // Scan multiple directories; store the FULL relative path so launchMission
    // can open the file without needing to know which folder it came from.
    const char* dirs[] = { "models", "textures/3d SpaceCraft Assets" };
    for (const char* dir : dirs) {
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) continue;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            for (auto& c : ext) c = (char)tolower(c);
            if (ext == ".glb" || ext == ".gltf")
                g_availableModels.push_back(
                    entry.path().generic_string()); // full relative path
        }
    }
}

// ----------------------------------------------------------------------------
// Launch / end mission
// ----------------------------------------------------------------------------
// launchMission
// Purpose: Initialize and activate a new spacecraft mission using the current g_config settings.
// Inputs:  sim - solar system, used to seed planet positions before spacecraft init
// Actions: Ticks the simulation one micro-step to set planet positions, initializes
//          g_spacecraft, loads the 3D model if configured, computes render scale,
//          resets the Hohmann plan, and switches to SolarSystem view.
static void launchMission(SolarSystem& sim) {
    sim.update(0.001f, 1.0f);
    g_spacecraft.init(g_config, sim);
    g_spacecraft.update(0.0f, 0.0f, sim);

    g_currentModel.reset();
    if (!g_config.modelFile.empty()) {
        auto m = std::make_unique<Model>();
        if (m->loadFromFile(g_config.modelFile)) g_currentModel = std::move(m);
    }

    // Recompute renderScale now that the model radius is known
    float modelR = g_currentModel ? g_currentModel->modelRadius : 1.0f;
    if (g_config.realScale && g_currentModel) {
        // In real-scale mode, we want the model's natural extent, mapped to meters,
        // to equal lengthMeters in scene units.
        g_spacecraft.renderScale = g_config.lengthMeters * 1.0e-6f;
    } else {
        g_spacecraft.renderScale = g_spacecraft.computeRenderScale(modelR);
    }

    g_hohmann = HohmannPlan{};
    g_missionActive = true;
    g_view = ViewMode::SolarSystem;
    snapCameraForView(sim);
}

// endMission
// Purpose: Deactivate the current mission and free its 3D model resource.
// Actions: Sets g_missionActive to false, resets the loaded model pointer, and clears the Hohmann plan.
static void endMission() {
    g_missionActive = false;
    g_currentModel.reset();
    g_hohmann = HohmannPlan{};
}


// ----------------------------------------------------------------------------
// Coordinate helpers
// ----------------------------------------------------------------------------
// Real position of a planet in km (ignoring compressed scene units).
// planetRealPositionKm
// Purpose: Compute the real (uncompressed) heliocentric position of a planet in km.
// Inputs:  p       - planet whose orbit parameters to use
//          x, y, z - output coordinates in km
// Actions: Converts orbitRadiusGm to km and applies the orbit angle.
static void planetRealPositionKm(const Planet& p, float& x, float& y, float& z) {
    float rKm = p.orbitRadiusGm * 1.0e6f;
    float a = glm::radians(p.orbitAngle);
    x = std::cos(a) * rKm;
    y = 0.0f;
    z = std::sin(a) * rKm;
}

// Real position of the spacecraft in km.
// spacecraftRealPositionKm
// Purpose: Compute the real (uncompressed) heliocentric position of the spacecraft in km.
// Inputs:  sc      - spacecraft whose statePosKm and parentBodyIdx to use
//          sim     - solar system for the parent planet's orbit angle
//          x, y, z - output heliocentric coordinates in km
// Actions: Adds the spacecraft's parent-relative state position to the parent planet's real position.
static void spacecraftRealPositionKm(const Spacecraft& sc, const SolarSystem& sim,
                                     float& x, float& y, float& z) {
    float px, py, pz;
    planetRealPositionKm(sim.body((size_t)sc.parentBodyIdx), px, py, pz);
    x = px + (float)sc.statePosKm.x;
    y = py + (float)sc.statePosKm.y;
    z = pz + (float)sc.statePosKm.z;
}

// Formats a km triplet with auto-scaled units (km / Mm / Gm / Tm)
// formatCoordKm
// Purpose: Format a position in km as a string, auto-scaling to Mm, Gm, or Tm as needed.
// Inputs:  buf  - output character buffer
//          n    - buffer size in bytes
//          x, y, z - coordinate components in km
// Actions: Finds the largest component magnitude, picks the appropriate unit prefix, and
//          writes a formatted "(x, y, z) unit" string into buf.
static void formatCoordKm(char* buf, size_t n, float x, float y, float z) {
    float m = std::max({std::fabs(x), std::fabs(y), std::fabs(z)});
    const char* unit = "km";
    float s = 1.0f;
    if      (m >= 1.0e9f) { s = 1.0e-9f; unit = "Tm"; }  // teramet = 1e9 km
    else if (m >= 1.0e6f) { s = 1.0e-6f; unit = "Gm"; }
    else if (m >= 1.0e3f) { s = 1.0e-3f; unit = "Mm"; }
    std::snprintf(buf, n, "(%.3f, %.3f, %.3f) %s", x*s, y*s, z*s, unit);
}

// ----------------------------------------------------------------------------
// Create Simulation popup (modal)
// ----------------------------------------------------------------------------
static bool g_openCreateSimPopup = false;

// drawCreateSimulationPopup
// Purpose: Render the modal dialog for configuring and launching a new spacecraft mission.
// Inputs:  sim - solar system, used to populate the starting-body dropdown
// Actions: Shows controls for spacecraft type, starting body, orbit parameters, 3D model
//          selection, physics mode, and a Launch button that calls launchMission().
static void drawCreateSimulationPopup(SolarSystem& sim) {
    if (g_openCreateSimPopup) {
        ImGui::OpenPopup("Create Simulation");
        g_openCreateSimPopup = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520.0f, 620.0f), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("Create Simulation", nullptr,
                                ImGuiWindowFlags_NoResize)) return;

    ImGui::TextUnformatted("Spacecraft");
    ImGui::Combo("##sc", &g_config.spacecraftType, scLabels::kSpacecraft,
                 IM_ARRAYSIZE(scLabels::kSpacecraft));

    ImGui::Spacing();
    ImGui::TextUnformatted("Starting Body");
    std::vector<const char*> bodyNames;
    for (size_t i = 0; i < sim.bodyCount(); ++i)
        bodyNames.push_back(sim.body(i).name.c_str());
    ImGui::Combo("##body", &g_config.startingBody,
                 bodyNames.data(), (int)bodyNames.size());

    ImGui::Spacing();
    ImGui::TextUnformatted("Orbit (km from body center)");
    ImGui::InputFloat("a (semi-major)", &g_config.semiMajorKm, 100.0f, 1000.0f, "%.0f");
    ImGui::InputFloat("b (semi-minor)", &g_config.semiMinorKm, 100.0f, 1000.0f, "%.0f");
    if (g_config.semiMajorKm < 100.0f) g_config.semiMajorKm = 100.0f;
    if (g_config.semiMinorKm > g_config.semiMajorKm)
        g_config.semiMinorKm = g_config.semiMajorKm;
    float ba = g_config.semiMinorKm / g_config.semiMajorKm;
    float ecc = std::sqrt(std::max(0.0f, 1.0f - ba*ba));
    ImGui::TextDisabled("eccentricity = %.4f", ecc);
    ImGui::SliderFloat("Inclination", &g_config.inclinationDeg, 0.0f, 180.0f, "%.1f deg");
    ImGui::TextUnformatted("Direction");
    ImGui::SameLine();
    if (ImGui::RadioButton("Prograde",  g_config.prograde))  g_config.prograde = true;
    ImGui::SameLine();
    if (ImGui::RadioButton("Retrograde", !g_config.prograde)) g_config.prograde = false;

    ImGui::Spacing();
    ImGui::TextUnformatted("3D Model");
    // Build display labels: show only the filename stem, not the full path.
    std::vector<std::string> labelStrs;
    labelStrs.reserve(g_availableModels.size());
    labelStrs.push_back("(default sphere)");
    for (size_t i = 1; i < g_availableModels.size(); ++i) {
        namespace fs = std::filesystem;
        labelStrs.push_back(fs::path(g_availableModels[i]).filename().string());
    }
    std::vector<const char*> labels;
    labels.reserve(labelStrs.size());
    for (auto& s : labelStrs) labels.push_back(s.c_str());
    if (ImGui::Combo("##model", &g_selectedModelIdx, labels.data(), (int)labels.size())) {
        if (g_selectedModelIdx >= 0 && g_selectedModelIdx < (int)g_availableModels.size())
            g_config.modelFile = g_availableModels[g_selectedModelIdx];
    }
    ImGui::SameLine();
    if (ImGui::Button("Rescan")) scanModelsDirectory();

    if (g_selectedModelIdx > 0) {
        ImGui::Checkbox("Real scale", &g_config.realScale);
        if (g_config.realScale) {
            ImGui::InputFloat("Length (m)", &g_config.lengthMeters, 0.5f, 5.0f, "%.2f");
            if (g_config.lengthMeters < 0.1f) g_config.lengthMeters = 0.1f;
            ImGui::TextDisabled("1 m = 1e-6 scene units");
        } else {
            ImGui::SliderFloat("Scale", &g_config.modelScale, 0.001f, 100.0f,
                               "%.3f", ImGuiSliderFlags_Logarithmic);
        }
        ImGui::SliderFloat("Rot X", &g_config.modelRotX, -180.0f, 180.0f, "%.0f deg");
        ImGui::SliderFloat("Rot Y", &g_config.modelRotY, -180.0f, 180.0f, "%.0f deg");
        ImGui::SliderFloat("Rot Z", &g_config.modelRotZ, -180.0f, 180.0f, "%.0f deg");
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Physics");
    ImGui::SameLine();
    if (ImGui::RadioButton("Patched Conics", !g_config.nBody)) g_config.nBody = false;
    ImGui::SameLine();
    if (ImGui::RadioButton("N-Body RK4",      g_config.nBody)) g_config.nBody = true;
    ImGui::TextDisabled("  N-Body uses full gravity from all planets (slower at high timescale)");

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Launch Mission", ImVec2(200, 32))) {
        launchMission(sim);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 32))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
// Settings popup modal
// ----------------------------------------------------------------------------
// drawSettingsPopup
// Purpose: Render the modal settings dialog for display, scale, visual, and physics options.
// Inputs:  sim - solar system whose visual settings (planet size, orbit color, etc.) are read and written
// Actions: Presents checkboxes and sliders for overlays, orbit compression, size multipliers,
//          skybox controls, and physics mode. Changes take effect immediately.
static void drawSettingsPopup(SolarSystem& sim) {
    if (g_openSettingsPopup) {
        ImGui::OpenPopup("Settings");
        g_openSettingsPopup = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560.0f, 700.0f), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("Settings", nullptr,
                                ImGuiWindowFlags_NoResize)) return;

    // ---- DISPLAY ----
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "DISPLAY");
    ImGui::Separator();

    ImGui::Columns(2, "display_cols", false);
    ImGui::SetColumnWidth(0, 200.0f);

    ImGui::Checkbox("Planet frames",        &g_showPlanetFrames);
    ImGui::Checkbox("Planet orbital paths", &g_showPlanetOrbits);
    ImGui::Checkbox("Spacecraft frame",     &g_showSpacecraftFrame);
    ImGui::Checkbox("Spacecraft orbit",     &g_showSpacecraftOrbit);

    ImGui::NextColumn();

    ImGui::PushItemWidth(160.0f);
    ImGui::ColorEdit3("##pfcol",  (float*)&g_planetFrameColor, ImGuiColorEditFlags_NoLabel);
    ImGui::SameLine(); ImGui::TextDisabled("Planet frame color");
    ImGui::ColorEdit3("##pocol",  (float*)&g_planetOrbitColor, ImGuiColorEditFlags_NoLabel);
    ImGui::SameLine(); ImGui::TextDisabled("Planet orbit color");
    ImGui::ColorEdit3("##sfcol",  (float*)&g_spacecraftFrameColor,     ImGuiColorEditFlags_NoLabel);
    ImGui::SameLine(); ImGui::TextDisabled("SC frame color");
    ImGui::ColorEdit3("##socol",  (float*)&g_spacecraftOrbitColor,     ImGuiColorEditFlags_NoLabel);
    ImGui::SameLine(); ImGui::TextDisabled("SC orbit color");
    ImGui::PopItemWidth();

    ImGui::Columns(1);

    ImGui::Spacing();
    // ---- SCALE ----
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "SCALE");
    ImGui::Separator();

    {
        ScaleMode mode = sim.getScaleMode();
        bool compact    = (mode == ScaleMode::COMPACT);
        bool realistic  = (mode == ScaleMode::REALISTIC);
        ImGui::TextUnformatted("Orbit distances:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Compact (compressed)",  compact))   sim.setScaleMode(ScaleMode::COMPACT);
        ImGui::SameLine();
        if (ImGui::RadioButton("Realistic (true scale)", realistic)) sim.setScaleMode(ScaleMode::REALISTIC);
        ImGui::TextDisabled("  Realistic: planets become tiny specks far apart.");
        ImGui::TextDisabled("  Compact: orbits compressed ~500x for easy viewing.");

        if (mode == ScaleMode::COMPACT) {
            float cs = sim.getCompactOrbitScale();
            if (ImGui::SliderFloat("Orbit compression", &cs, 0.0002f, 0.010f, "%.4f",
                                   ImGuiSliderFlags_Logarithmic)) {
                sim.setCompactOrbitScale(cs);
            }
            ImGui::TextDisabled("  Lower = planets closer together.");
        }
    }

    ImGui::Spacing();
    {
        float ps = sim.getPlanetSizeMul();
        if (ImGui::SliderFloat("Planet size multiplier", &ps, 0.1f, 10.0f, "%.2fx",
                               ImGuiSliderFlags_Logarithmic))
            sim.setPlanetSizeMul(ps);
        ImGui::TextDisabled("  1.0 = real proportional size.");

        float ss = sim.getSunSizeMul();
        if (ImGui::SliderFloat("Sun size multiplier",    &ss, 0.1f, 10.0f, "%.2fx",
                               ImGuiSliderFlags_Logarithmic))
            sim.setSunSizeMul(ss);
    }

    ImGui::Spacing();
    // ---- VISUALS ----
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "VISUALS");
    ImGui::Separator();

    {
        float sb = sim.getSunBrightness();
        if (ImGui::SliderFloat("Sun brightness",     &sb, 0.2f, 5.0f, "%.2f"))
            sim.setSunBrightness(sb);

        ImGui::SliderFloat("Skybox brightness", &g_skyboxBrightness, 0.0f, 2.0f, "%.2f");
        ImGui::TextDisabled("  0 = black sky,  1 = neutral,  2 = overexposed.");
        ImGui::Spacing();
        ImGui::TextUnformatted("Skybox orientation:");
        ImGui::SliderFloat("Yaw  (Y)",  &g_skyboxYaw,   -180.0f, 180.0f, "%.1f deg");
        ImGui::SliderFloat("Pitch (X)", &g_skyboxPitch, -180.0f, 180.0f, "%.1f deg");
        ImGui::SliderFloat("Roll  (Z)", &g_skyboxRoll,  -180.0f, 180.0f, "%.1f deg");
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset##sky")) {
            g_skyboxYaw = g_skyboxPitch = g_skyboxRoll = 0.0f;
        }
    }

    ImGui::Spacing();
    // ---- PHYSICS ----
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "PHYSICS");
    ImGui::Separator();

    ImGui::TextUnformatted("Default orbit calculation (new missions):");
    if (ImGui::RadioButton("Patched Conics (fast)", !g_config.nBody)) g_config.nBody = false;
    ImGui::SameLine();
    if (ImGui::RadioButton("N-Body RK4 (accurate)", g_config.nBody))  g_config.nBody = true;
    ImGui::TextDisabled("  Patched Conics: Kepler ellipses, instant SOI switches.");
    ImGui::TextDisabled("  N-Body RK4: full gravitational simulation, CPU intensive.");

    if (g_missionActive) {
        ImGui::Spacing();
        ImGui::TextDisabled("Active mission uses: %s",
            g_spacecraft.config.nBody ? "N-Body RK4" : "Patched Conics");
        ImGui::TextDisabled("Launch a new mission to apply physics changes.");
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Close", ImVec2(120, 28))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
// Help popup modal
// ----------------------------------------------------------------------------
// drawHelpPopup
// Purpose: Render the modal help reference dialog listing all controls and features.
// Actions: Opens and draws a scrollable ImGui popup window with navigation, view, simulation,
//          time, display, spacecraft control, navball, and settings documentation.
static void drawHelpPopup() {
    if (g_openHelpPopup) {
        ImGui::OpenPopup("Help");
        g_openHelpPopup = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600.0f, 700.0f), ImGuiCond_Appearing);

    if (!ImGui::BeginPopupModal("Help", nullptr, ImGuiWindowFlags_NoResize)) return;

    ImGui::BeginChild("##helpcontent", ImVec2(0, -40), false);

    auto section = [](const char* title) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "%s", title);
        ImGui::Separator();
    };

    // ---- NAVIGATION ----
    section("NAVIGATION");
    ImGui::BulletText("Right-click + drag: Rotate camera around focus point");
    ImGui::BulletText("Scroll wheel: Zoom in / out");
    ImGui::BulletText("Click a planet name (left panel): Focus camera on that planet");

    // ---- VIEWS ----
    section("VIEWS");
    ImGui::BulletText("Solar System: Overview of all planets orbiting the Sun");
    ImGui::BulletText("Planet: Close-up of the selected starting body");
    ImGui::BulletText("Spacecraft: Chase camera locked onto the active spacecraft");

    // ---- SIMULATION ----
    section("SIMULATION");
    ImGui::BulletText("Create Simulation: Configure spacecraft, orbit parameters,\n"
                      "  3D model, and physics mode, then launch");
    ImGui::BulletText("Run Default Simulation: Launches a Dragon 2 capsule in an\n"
                      "  ISS-like orbit (420 km altitude, 51.6 deg inclination) around Earth");
    ImGui::BulletText("End Mission: Removes the active spacecraft");
    ImGui::TextDisabled("  Orbit parameters:");
    ImGui::TextDisabled("    a = semi-major axis (km from body center)");
    ImGui::TextDisabled("    b = semi-minor axis  (b < a gives an elliptical orbit)");
    ImGui::TextDisabled("    Inclination = tilt of orbit relative to equatorial plane");
    ImGui::TextDisabled("    Prograde = orbits in the same direction as the planet's rotation");

    // ---- TIME CONTROL ----
    section("TIME CONTROL");
    ImGui::BulletText("Pause / Resume: Freeze or continue the simulation");
    ImGui::BulletText("Time scale slider: 1x = real time; drag right to speed up");
    ImGui::BulletText("Quick presets: 1x / 60x / 3600x / 86400x");

    // ---- DISPLAY TOGGLES ----
    section("DISPLAY TOGGLES  (left panel and Settings > Display)");
    ImGui::BulletText("Planet frames: Cyan bounding boxes with planet names");
    ImGui::BulletText("Planet orbital paths: Cyan orbit ellipses for all planets");
    ImGui::BulletText("Spacecraft frame: Green bounding box around active spacecraft");
    ImGui::BulletText("Spacecraft orbit: Green orbital ellipse for active spacecraft");
    ImGui::TextDisabled("  Colors for each overlay are customizable in Settings > Display.");

    // ---- SPACECRAFT CONTROLS ----
    section("SPACECRAFT CONTROLS  (requires active mission)");
    ImGui::BulletText("Up / Down arrows: Pitch nose up / down");
    ImGui::BulletText("Left / Right arrows: Yaw left / right");
    ImGui::BulletText("Q / E: Roll clockwise / counter-clockwise");
    ImGui::BulletText("X: Kill rotation (zeroes angular velocity instantly)");
    ImGui::BulletText("F: Toggle engine on / off");
    ImGui::BulletText("Z / C: Throttle up / down (hold key)");
    ImGui::BulletText("Throttle bar (right of navball): Click or drag to set throttle");

    // ---- NAVBALL ----
    section("NAVBALL  (bottom-center of viewport)");
    ImGui::TextWrapped("Shows spacecraft attitude relative to the Local Vertical / Local Horizontal (LVLH) orbital frame.");
    ImGui::Spacing();
    ImGui::BulletText("PRO  (yellow):    Prograde - direction of orbital velocity");
    ImGui::BulletText("RET  (yellow):    Retrograde - opposite of orbital velocity");
    ImGui::BulletText("RAD  (cyan):      Radially away from the orbited body");
    ImGui::BulletText("ARAD (cyan):      Radially toward the orbited body");
    ImGui::BulletText("N / -N (purple):  Orbital normal (perpendicular to orbit plane)");
    ImGui::BulletText("Crosshair:        Spacecraft nose direction");
    ImGui::BulletText("Blue hemisphere:  Facing away from orbited body");
    ImGui::BulletText("Orange hemisphere: Facing toward orbited body");
    ImGui::TextDisabled("  Markers are dimmed when on the back hemisphere.");

    // ---- IN FLIGHT PANEL ----
    section("IN FLIGHT PANEL  (right sidebar)");
    ImGui::BulletText("Position: Heliocentric coordinates in km / Mm / Gm / Tm");
    ImGui::BulletText("Orbital velocity: Current speed in km/s");
    ImGui::BulletText("Orbital elements: semi-major axis (a), eccentricity (e),\n"
                      "  inclination (i), and orbital period");
    ImGui::BulletText("Propulsion: Engine toggle, throttle slider, fuel remaining");
    ImGui::BulletText("Attitude: Pitch / Yaw / Roll relative to LVLH frame");

    // ---- SETTINGS ----
    section("SETTINGS  (Settings... button)");
    ImGui::BulletText("Display: Toggle overlays and pick colors for each");
    ImGui::BulletText("Scale > Compact: Orbits compressed ~500x for easy viewing");
    ImGui::BulletText("Scale > Realistic: True proportional solar system scale");
    ImGui::BulletText("Orbit compression slider: Controls how compressed Compact mode is");
    ImGui::BulletText("Planet / Sun size multipliers: Scale visual sizes independently");
    ImGui::BulletText("Sun brightness: Emissive intensity of the sun surface");
    ImGui::BulletText("Skybox brightness: 0 = black, 1 = neutral, 2 = overexposed");
    ImGui::BulletText("Skybox Yaw / Pitch / Roll: Rotate the Milky Way panorama");
    ImGui::BulletText("Physics - Patched Conics: Fast Keplerian ellipses with\n"
                      "  instant sphere-of-influence transitions");
    ImGui::BulletText("Physics - N-Body RK4: Full gravitational simulation from\n"
                      "  all planets (more accurate, CPU-intensive at high timescale)");

    // ---- DEBUG OPTIONS ----
    section("DEBUG OPTIONS  (left panel)");
    ImGui::BulletText("Show world axes: RGB X/Y/Z gizmo at the origin");
    ImGui::BulletText("Show surface pins: Marks 0 deg N / 0 deg E on each planet\n"
                      "  (verifies textures are aligned to the mesh)");
    ImGui::BulletText("Show SOI spheres: Wireframe spheres showing each planet's\n"
                      "  sphere of influence boundary");

    ImGui::Spacing();
    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::Button("Close", ImVec2(120, 28))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
// Hohmann transfer planner
// ----------------------------------------------------------------------------
// computeHohmannPlan
// Purpose: Calculate the delta-V and burn durations for a two-burn Hohmann transfer.
// Inputs:  sim       - solar system for planet GM values and heliocentric positions
//          sc        - current spacecraft state (position, velocity, mass, engine data)
//          targetIdx - index of the target planet (or same as sc.parentBodyIdx for an orbit change)
//          periKm    - desired periapsis radius of the target orbit, km from body center
//          apoKm     - desired apoapsis radius of the target orbit, km from body center
// Actions: Computes vis-viva speeds and hyperbolic excess velocities for both burns.
//          Uses the Tsiolkovsky rocket equation to estimate burn durations.
// Returns: Populated HohmannPlan struct with burn directions, dV, duration, and 3D positions.
static HohmannPlan computeHohmannPlan(const SolarSystem& sim, const Spacecraft& sc,
                                       int targetIdx, double periKm, double apoKm) {
    HohmannPlan plan;
    plan.sameBody      = (targetIdx == sc.parentBodyIdx);
    plan.targetBodyIdx = targetIdx;
    plan.targetPeriKm  = periKm;
    plan.targetApoKm   = apoKm;

    const double MU_SUN   = 1.32712440018e11; // km³/s²
    const double g0_kms   = 9.80665e-3;       // standard gravity in km/s²

    // Current (approx circular) orbit radius around parent body
    double r1  = glm::length(sc.statePosKm);
    if (r1 < 10.0) r1 = 10.0;
    double mu1 = (double)sc.gravParamKm3S2;
    double v1c = std::sqrt(mu1 / r1);  // circular speed km/s

    // Burn duration from Tsiolkovsky rocket equation (result in real seconds)
    double mass_kg  = sc.currentMassKg();
    double thrust_N = sc.engineData.maxThrustN;
    double isp_s    = sc.engineData.ispSec;
    auto burnDuration = [&](double dv_kms) -> double {
        if (isp_s <= 0.0 || thrust_N <= 0.0 || mass_kg <= 0.0 || dv_kms <= 0.0)
            return 0.0;
        double ve   = isp_s * g0_kms;             // exhaust velocity km/s
        double mf   = mass_kg * std::exp(-dv_kms / ve);
        double mdot = thrust_N / (ve * 1000.0);    // kg/s  (F[N]/ve[m/s])
        return (mass_kg - mf) / mdot;
    };

    if (plan.sameBody) {
        // ---- Same-body 2-burn Hohmann ----
        // Burn 1 at r1: set apoapsis to apoKm  (transfer orbit: peri=r1, apo=apoKm)
        double ra    = std::max(periKm, apoKm);
        double rp    = std::min(periKm, apoKm);
        double a_t   = (r1 + ra) * 0.5;
        double vtp   = std::sqrt(mu1 * (2.0/r1  - 1.0/a_t)); // transfer speed at r1
        double dv1   = vtp - v1c;

        // Burn 2 at ra: set periapsis to periKm
        double vta   = std::sqrt(mu1 * (2.0/ra  - 1.0/a_t)); // transfer speed at ra
        double a_f   = (rp + ra) * 0.5;
        double vfa   = std::sqrt(mu1 * (2.0/ra  - 1.0/a_f)); // target orbit speed at ra
        double dv2   = vfa - vta;

        plan.burns[0].dv_kms        = std::abs(dv1);
        plan.burns[0].duration_s    = burnDuration(std::abs(dv1));
        plan.burns[0].prograde      = (dv1 >= 0.0);
        plan.burns[0].posRelParent  = sc.statePosKm;

        plan.burns[1].dv_kms        = std::abs(dv2);
        plan.burns[1].duration_s    = burnDuration(std::abs(dv2));
        plan.burns[1].prograde      = (dv2 >= 0.0);
        // Burn 2 at apoapsis of transfer orbit = opposite side from burn 1
        plan.burns[1].posRelParent  = -glm::normalize(sc.statePosKm) * ra;

    } else {
        // ---- Interplanetary patched-conics Hohmann ----
        double R1 = glm::length(sim.planetRealPosKm((size_t)sc.parentBodyIdx));  // km
        double R2 = glm::length(sim.planetRealPosKm((size_t)targetIdx));
        if (R1 < 1.0 || R2 < 1.0) { return plan; }  // parent or target is Sun

        double a_t    = (R1 + R2) * 0.5;
        double vH1    = std::sqrt(MU_SUN * (2.0/R1 - 1.0/a_t)); // transfer speed at R1
        double vH2    = std::sqrt(MU_SUN * (2.0/R2 - 1.0/a_t)); // transfer speed at R2
        double vC1    = std::sqrt(MU_SUN / R1);                  // parent planet speed
        double vC2    = std::sqrt(MU_SUN / R2);                  // target planet speed

        double vInf1  = std::abs(vH1 - vC1);  // hyperbolic excess at departure
        double vInf2  = std::abs(vH2 - vC2);  // hyperbolic excess at arrival

        // Departure: escape from parking orbit
        double vHyp1  = std::sqrt(vInf1*vInf1 + 2.0*mu1/r1);
        double dv1    = vHyp1 - v1c;  // positive = prograde for outer planet

        // Arrival: capture into target orbit at periapsis
        double mu2    = (double)sim.body((size_t)targetIdx).muKm3PerS2;
        double rCap   = std::min(periKm, apoKm);
        double rApo   = std::max(periKm, apoKm);
        double vHyp2  = std::sqrt(vInf2*vInf2 + 2.0*mu2/rCap);
        double aF     = (rCap + rApo) * 0.5;
        double vOrb2  = std::sqrt(mu2 * (2.0/rCap - 1.0/aF));
        double dv2    = std::abs(vHyp2 - vOrb2);

        bool toOuter  = (R2 > R1);
        plan.burns[0].dv_kms        = std::abs(dv1);
        plan.burns[0].duration_s    = burnDuration(std::abs(dv1));
        plan.burns[0].prograde      = toOuter;    // prograde = outer, retrograde = inner
        plan.burns[0].posRelParent  = sc.statePosKm;

        plan.burns[1].dv_kms        = dv2;
        plan.burns[1].duration_s    = burnDuration(dv2);
        plan.burns[1].prograde      = !toOuter;   // arrival: decelerate to capture
        plan.burns[1].posRelParent  = glm::dvec3(0.0); // at target planet, no 3D marker here
    }

    plan.active     = true;
    plan.activeBurn = 0;
    return plan;
}

// drawHohmannPopup
// Purpose: Render the modal dialog for planning a Hohmann transfer maneuver.
// Inputs:  sim - solar system for body names and positions
//          sc  - current spacecraft (used for live delta-V preview)
// Actions: Shows target body selector, periapsis/apoapsis inputs, and a live preview of
//          both burn delta-Vs and durations. Confirms plan on "Plan Transfer" click.
static void drawHohmannPopup(const SolarSystem& sim, const Spacecraft& sc) {
    if (g_openHohmannPopup) {
        ImGui::OpenPopup("Set Target Orbit");
        g_openHohmannPopup = false;
    }
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 440.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Set Target Orbit", nullptr,
                                ImGuiWindowFlags_NoResize)) return;

    ImGui::TextColored(ImVec4(0.8f, 0.5f, 1.0f, 1.0f), "HOHMANN TRANSFER PLANNER");
    ImGui::Separator();
    ImGui::Spacing();

    std::vector<const char*> bodyNames;
    for (size_t i = 0; i < sim.bodyCount(); ++i)
        bodyNames.push_back(sim.body(i).name.c_str());
    ImGui::Text("Target Body:");
    ImGui::Combo("##hmtgt", &g_hohmannTargetBodyIdx, bodyNames.data(), (int)bodyNames.size());

    bool sameBdy = (g_hohmannTargetBodyIdx == sc.parentBodyIdx);
    if (sameBdy)
        ImGui::TextDisabled("  Orbit change around %s", sim.body(sc.parentBodyIdx).name.c_str());
    else
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "  Interplanetary: %s → %s",
            sim.body(sc.parentBodyIdx).name.c_str(),
            sim.body(g_hohmannTargetBodyIdx).name.c_str());

    ImGui::Spacing();
    float tgtR = sameBdy ? (float)sim.body(sc.parentBodyIdx).radiusKm
                         : (float)sim.body(g_hohmannTargetBodyIdx).radiusKm;
    ImGui::Text("Target Orbit (km from body center):");
    ImGui::InputFloat("Periapsis##hmp", &g_hohmannPeriapsisKm, 10.0f, 100.0f, "%.0f");
    ImGui::InputFloat("Apoapsis##hma",  &g_hohmannApoapsisKm,  10.0f, 100.0f, "%.0f");
    g_hohmannPeriapsisKm = std::max(g_hohmannPeriapsisKm, tgtR + 10.0f);
    g_hohmannApoapsisKm  = std::max(g_hohmannApoapsisKm,  g_hohmannPeriapsisKm);
    ImGui::TextDisabled("  Periapsis altitude: %.0f km   Apoapsis altitude: %.0f km",
        g_hohmannPeriapsisKm - tgtR, g_hohmannApoapsisKm - tgtR);

    ImGui::Spacing();
    ImGui::Separator();

    // Live preview
    HohmannPlan preview = computeHohmannPlan(sim, sc,
        g_hohmannTargetBodyIdx, (double)g_hohmannPeriapsisKm, (double)g_hohmannApoapsisKm);
    if (preview.active) {
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "CALCULATED BURNS");
        ImGui::Separator();
        for (int i = 0; i < 2; ++i) {
            ImGui::Text("Burn %d:", i+1);
            ImGui::TextDisabled("  ΔV = %.4f km/s  (%s)",
                preview.burns[i].dv_kms,
                preview.burns[i].prograde ? "Prograde" : "Retrograde");
            double d = preview.burns[i].duration_s;
            if (d <= 0.0)
                ImGui::TextDisabled("  Duration: N/A (no propulsion data)");
            else {
                int m = (int)(d/60.0); double s = d - m*60.0;
                ImGui::TextDisabled("  Duration: %d:%05.2f  (engine-on time)", m, s);
            }
            ImGui::Spacing();
        }
        double totalDv = preview.burns[0].dv_kms + preview.burns[1].dv_kms;
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.7f, 1.0f), "Total ΔV: %.4f km/s", totalDv);
        if (!sameBdy)
            ImGui::TextDisabled("Point spacecraft at PRO (outer planet) or RET (inner planet)\n"
                                "during Burn 1. Point at RET during Burn 2 (arrival capture).");
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Plan Transfer", ImVec2(160, 28))) {
        g_hohmann = computeHohmannPlan(sim, sc,
            g_hohmannTargetBodyIdx, (double)g_hohmannPeriapsisKm, (double)g_hohmannApoapsisKm);
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 28))) ImGui::CloseCurrentPopup();
    ImGui::SameLine();
    if (g_hohmann.active && ImGui::Button("Clear Plan", ImVec2(110, 28))) {
        g_hohmann = HohmannPlan{};
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// ----------------------------------------------------------------------------
// Left sidebar: Create Simulation button, view/time, planet list with coords
// ----------------------------------------------------------------------------
// drawLeftSidebar
// Purpose: Render the left UI panel with simulation controls, view selection, and the planet list.
// Inputs:  sim - solar system for planet names and coordinates
// Actions: Draws buttons (Create Simulation, End Mission, Settings, Help), view mode radio
//          buttons, display toggles, time controls, and a scrollable list of all bodies.
static void drawLeftSidebar(SolarSystem& sim) {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kLeftSidebarWidth, (float)g_windowHeight), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("##left", nullptr, flags)) {
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "SOLAR SYSTEM SIM");
        ImGui::Separator();

        // Create Simulation button
        if (ImGui::Button(g_missionActive ? "New Simulation..." : "Create Simulation...",
                          ImVec2(-1, 32))) {
            g_openCreateSimPopup = true;
        }
        if (g_missionActive) {
            if (ImGui::Button("End Mission", ImVec2(-1, 0))) endMission();
        }
        if (ImGui::Button("Settings...", ImVec2(-1, 0))) g_openSettingsPopup = true;
        if (ImGui::Button("Help...",     ImVec2(-1, 0))) g_openHelpPopup     = true;
        if (ImGui::Button("Run Default Simulation", ImVec2(-1, 28))) {
            // Dragon 2 in ISS-like orbit around Earth (420 km altitude, 51.6° inc)
            g_config = MissionConfig{};
            g_config.spacecraftType = 3;           // Dragon 2
            g_config.startingBody   = 3;           // Earth
            g_config.semiMajorKm    = 6791.0f;     // 6371 + 420 km
            g_config.semiMinorKm    = 6791.0f;     // circular
            g_config.inclinationDeg = 51.6f;       // ISS inclination
            g_config.prograde       = true;
            g_config.modelFile      = "textures/3d SpaceCraft Assets/Dragon_Spacecraft.glb";
            g_config.modelScale     = 1.0f;
            g_config.nBody          = false;
            launchMission(sim);
            g_view = ViewMode::Planet;
            snapCameraForView(sim);
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("View");
        int v = (int)g_view;
        if (ImGui::RadioButton("Solar System", v == 0)) {
            g_view = ViewMode::SolarSystem; snapCameraForView(sim);
        }
        if (ImGui::RadioButton("Planet",       v == 1)) {
            g_view = ViewMode::Planet; snapCameraForView(sim);
        }
        if (ImGui::RadioButton("Spacecraft",   v == 2)) {
            g_view = ViewMode::Spacecraft; snapCameraForView(sim);
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "DISPLAY");
        ImGui::Checkbox("Planet frames",        &g_showPlanetFrames);
        ImGui::Checkbox("Planet orbital paths", &g_showPlanetOrbits);
        ImGui::Checkbox("Spacecraft frame",     &g_showSpacecraftFrame);
        ImGui::Checkbox("Spacecraft orbit",     &g_showSpacecraftOrbit);

        ImGui::Spacing();
        ImGui::Checkbox("Show world axes (debug)", &g_showAxes);
        ImGui::Checkbox("Show surface pins (debug)", &g_showPins);
        ImGui::Checkbox("Show SOI spheres (debug)", &g_showSOI);

        // Live camera state - definitive way to tell if camera is moving
        {
            float azDeg = glm::degrees(g_camera.azimuth);
            float elDeg = glm::degrees(g_camera.elevation);
            // Keep azimuth in [-180, 180] for readability
            while (azDeg > 180.0f)  azDeg -= 360.0f;
            while (azDeg < -180.0f) azDeg += 360.0f;
            ImGui::Text("Cam az=%+.1f° el=%+.1f° d=%.0f",
                        azDeg, elDeg, g_camera.distance);
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Time");
        if (ImGui::Button(g_paused ? "Resume" : "Pause", ImVec2(100, 0)))
            g_paused = !g_paused;
        // g_timeScale = speed multiplier: 1 = real time, 2 = 2× real time, etc.
        {
            char lbl[32];
            if      (g_timeScale >= 86400.0f) std::snprintf(lbl, sizeof(lbl), "%.0fx", g_timeScale);
            else if (g_timeScale >=    60.0f) std::snprintf(lbl, sizeof(lbl), "%.1fx", g_timeScale);
            else                              std::snprintf(lbl, sizeof(lbl), "%.2fx", g_timeScale);
            ImGui::SliderFloat("##timescale", &g_timeScale, 0.1f, 10000000.0f, lbl,
                               ImGuiSliderFlags_Logarithmic);
        }
        ImGui::TextDisabled("Speed multiplier (1x = real time)");
        // Quick-preset buttons
        if (ImGui::SmallButton("1x"))      { g_timeScale = 1.0f;      } ImGui::SameLine();
        if (ImGui::SmallButton("60x"))     { g_timeScale = 60.0f;     } ImGui::SameLine();
        if (ImGui::SmallButton("3600x"))   { g_timeScale = 3600.0f;   } ImGui::SameLine();
        if (ImGui::SmallButton("86400x"))  { g_timeScale = 86400.0f;  }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "PLANETS");
        ImGui::Separator();

        // Scrollable list in case window is short
        ImGui::BeginChild("##planetlist", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar);
        for (size_t i = 0; i < sim.bodyCount(); ++i) {
            const Planet& p = sim.body(i);
            float x, y, z;
            planetRealPositionKm(p, x, y, z);
            char coords[96];
            formatCoordKm(coords, sizeof(coords), x, y, z);

            if (ImGui::Selectable(p.name.c_str(), false,
                                  ImGuiSelectableFlags_AllowDoubleClick)) {
                float r = sim.planetSceneRadius(i);
                if (p.isSun) {
                    g_view = ViewMode::SolarSystem;
                    g_camera.setFocus(glm::vec3(0.0f), 3500.0f, true, 45.0f, 45.0f);
                    g_camera.minDistance = 100.0f;
                    g_camera.maxDistance = 2.0e7f;
                } else {
                    g_view = ViewMode::Planet;
                    g_config.startingBody = (int)i;
                    g_camera.setFocus(p.worldPos, r * 5.0f, true, 0.0f, 45.0f);
                    g_camera.minDistance = r * 1.2f;
                    g_camera.maxDistance = r * 200.0f;
                }
            }
            ImGui::TextDisabled("  %s", coords);
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

// ----------------------------------------------------------------------------
// Right sidebar: spacecraft in flight with coords, velocity, parent, attitude
// ----------------------------------------------------------------------------
// drawRightSidebar
// Purpose: Render the right UI panel showing in-flight telemetry, propulsion, attitude, and transfer planner.
// Inputs:  sim - solar system for the parent body name
// Actions: If a mission is active, displays position, orbital velocity, orbital elements,
//          engine controls, attitude readout, and the Hohmann burn timer.
static void drawRightSidebar(SolarSystem& sim) {
    ImGui::SetNextWindowPos(ImVec2(g_windowWidth - kRightSidebarWidth, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kRightSidebarWidth, (float)g_windowHeight), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("##right", nullptr, flags)) {
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "IN FLIGHT");
        ImGui::Separator();

        if (g_missionActive) {
            const Planet& parent = sim.body((size_t)g_spacecraft.parentBodyIdx);
            const char* scName = scLabels::kSpacecraft[
                std::clamp(g_spacecraft.config.spacecraftType, 0, 3)];

            if (ImGui::Selectable(scName)) {
                g_view = ViewMode::Spacecraft;
                snapCameraForView(sim);
            }
            ImGui::TextDisabled("Orbiting: %s  [%s]", parent.name.c_str(),
                g_spacecraft.config.nBody ? "N-Body" : "Kepler");

            float x, y, z;
            spacecraftRealPositionKm(g_spacecraft, sim, x, y, z);
            char posStr[96];
            formatCoordKm(posStr, sizeof(posStr), x, y, z);
            ImGui::TextUnformatted("Position:");
            ImGui::TextDisabled("  %s", posStr);

            float v_kms = g_spacecraft.getOrbitalSpeedKmS();
            ImGui::Text("Orbital velocity: %.3f km/s", v_kms);

            ImGui::Spacing();
            // Live orbital elements derived from state vectors
            OrbitalElements el = Spacecraft::elementsFromState(
                g_spacecraft.statePosKm, g_spacecraft.stateVelKmS, g_spacecraft.gravParamKm3S2);
            if (el.semiMajorAxisKm > 0.0) {
                ImGui::TextDisabled("a = %.0f km", (float)el.semiMajorAxisKm);
                ImGui::TextDisabled("e = %.4f",    (float)el.eccentricity);
                ImGui::TextDisabled("i = %.1f deg",(float)el.inclinationDeg);
                ImGui::TextDisabled("dir: %s",     el.prograde ? "prograde" : "retrograde");
                ImGui::TextDisabled("T = %.3f days",(float)el.periodDays);
            } else {
                ImGui::TextDisabled("(escape trajectory)");
            }
        } else {
            ImGui::TextDisabled("(no active spacecraft)");
            ImGui::Spacing();
            ImGui::TextWrapped("Click \"Create Simulation\" in the left panel to launch.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "PROPULSION");
        ImGui::Separator();
        if (g_missionActive) {
            auto& sc = g_spacecraft;
            if (sc.engineData.maxThrustN <= 0.0f) {
                ImGui::TextDisabled("%s", sc.engineData.engineName.c_str());
                ImGui::TextDisabled("(no propulsion system)");
            } else {
                // Engine on/off toggle
                ImVec4 btnCol = sc.engineOn
                    ? ImVec4(0.15f, 0.65f, 0.15f, 1.0f)
                    : ImVec4(0.40f, 0.40f, 0.40f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
                const char* btnLabel = sc.engineOn ? "Engine: ON  [F]" : "Engine: OFF [F]";
                if (ImGui::Button(btnLabel, ImVec2(-1.0f, 0.0f)))
                    sc.engineOn = !sc.engineOn;
                ImGui::PopStyleColor();

                // Throttle slider (integer %)
                int throttlePct = (int)(sc.throttle * 100.0f + 0.5f);
                if (ImGui::SliderInt("Throttle", &throttlePct, 0, 100, "%d%%"))
                    sc.throttle = throttlePct / 100.0f;
                ImGui::TextDisabled("  Z = +throttle   C = -throttle");

                // Current thrust + engine name
                float thrustCur = sc.currentThrustN();
                ImGui::Text("Thrust: %.1f N", thrustCur);
                ImGui::TextDisabled("  %s", sc.engineData.engineName.c_str());

                // Fuel remaining
                if (sc.engineData.propellantMassKg > 0.0f) {
                    float pct = sc.propellantKg / sc.engineData.propellantMassKg * 100.0f;
                    ImGui::Text("Fuel:  %.0f kg (%.1f%%)", sc.propellantKg, pct);
                } else {
                    ImGui::TextDisabled("Fuel: N/A");
                }
            }
        } else {
            ImGui::TextDisabled("(no spacecraft)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 1.0f, 1.0f), "ATTITUDE (LVLH)");
        ImGui::Separator();
        if (g_missionActive) {
            glm::vec3 eul = g_spacecraft.getEulerAnglesLvlhDeg();
            ImGui::Text("Pitch: %+7.2f deg", eul.x);
            ImGui::Text("Yaw:   %+7.2f deg", eul.y);
            ImGui::Text("Roll:  %+7.2f deg", eul.z);
            ImGui::Text("Rate:  %.4f rad/s", glm::length(g_spacecraft.angularVelocity));
        } else {
            ImGui::TextDisabled("(no spacecraft)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.5f, 1.0f, 1.0f), "TRANSFER PLANNER");
        ImGui::Separator();
        if (g_missionActive) {
            if (ImGui::Button("Set Target Orbit...", ImVec2(-1, 0)))
                g_openHohmannPopup = true;
        }
        // Burn timer - always displayed; shows "---:--:--" when idle
        {
            int bi = g_hohmann.activeBurn;
            bool planActive = g_hohmann.active && bi < 2 && !g_hohmann.burns[bi].done;
            if (planActive) {
                const auto& burn = g_hohmann.burns[bi];
                double rem = std::max(0.0, burn.duration_s - burn.elapsed_s);
                int    h   = (int)(rem / 3600.0);
                int    m   = (int)((rem - h*3600.0) / 60.0);
                double s   = rem - h*3600.0 - m*60.0;
                char tbuf[24]; std::snprintf(tbuf,sizeof(tbuf),"%03d:%02d:%05.2f", h, m, s);
                ImGui::TextColored(ImVec4(0.8f,0.5f,1.0f,1.0f), "BURN %d TIMER", bi+1);
                ImGui::Text("  %s", tbuf);
                ImGui::TextDisabled("  ΔV = %.4f km/s", burn.dv_kms);
                ImGui::TextDisabled("  Dir: %s", burn.prograde ? "PROGRADE" : "RETROGRADE");
                if (g_missionActive) {
                    bool on = g_spacecraft.engineOn && g_spacecraft.throttle > 0.0f;
                    ImGui::TextColored(on ? ImVec4(0,1,0.3f,1) : ImVec4(1,0.8f,0,1),
                        on ? "  [ENGINE FIRING]" : "  [FIRE ENGINE TO BURN]");
                }
                if (g_hohmann.burns[0].done && bi == 1)
                    ImGui::TextColored(ImVec4(0.5f,1,0.5f,1), "  Burn 1 complete!");
            } else if (g_hohmann.active &&
                       g_hohmann.burns[0].done && g_hohmann.burns[1].done) {
                ImGui::TextColored(ImVec4(0.5f,1.0f,0.5f,1.0f), "ALL BURNS COMPLETE");
                ImGui::TextDisabled("  Transfer maneuvers executed.");
            } else {
                ImGui::TextDisabled("---:--:--");
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 1.0f), "CONTROLS");
        ImGui::BulletText("RMB + drag: rotate cam");
        ImGui::BulletText("Scroll: zoom");
        ImGui::BulletText("Arrows: pitch / yaw");
        ImGui::BulletText("Q / E: roll");
        ImGui::BulletText("X: stop rotation");
        ImGui::BulletText("F: engine on/off");
        ImGui::BulletText("Z / C: throttle up/down");
    }
    ImGui::End();
}
// ----------------------------------------------------------------------------
// drawPlanetFrames
// Purpose: Draw a 2D bounding box and name label over each planet in the viewport.
// Inputs:  sim  - solar system for planet world positions and names
//          view - camera view matrix
//          proj - camera projection matrix
// Actions: Projects each planet's world position to screen space and draws a colored
//          rectangle and text label using ImGui's foreground draw list.
static void drawPlanetFrames(SolarSystem& sim, const glm::mat4& view,
                             const glm::mat4& proj) {
    if (!g_showPlanetFrames) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImU32 color = colFromVec4(g_planetFrameColor);

    for (size_t i = 0; i < sim.bodyCount(); ++i) {
        const Planet& p = sim.body(i);
        if (p.isSun) continue;
        glm::vec4 clip = proj * view * glm::vec4(p.worldPos, 1.0f);
        if (clip.w <= 0.0f) continue;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (std::abs(ndc.x) > 1.5f || std::abs(ndc.y) > 1.5f) continue;

        float sx = g_viewportX + (ndc.x * 0.5f + 0.5f) * g_viewportWidth;
        float sy = g_viewportY + (1.0f - (ndc.y * 0.5f + 0.5f)) * g_viewportHeight;
        float box = 18.0f;
        dl->AddRect(ImVec2(sx - box, sy - box), ImVec2(sx + box, sy + box),
                    color, 0.0f, 0, 1.5f);
        const char* name = p.name.c_str();
        ImVec2 ts = ImGui::CalcTextSize(name);
        dl->AddText(ImVec2(sx - ts.x * 0.5f, sy + box + 2.0f), color, name);
    }
}

// drawSpacecraftFrame
// Purpose: Draw a 2D bounding box and name label over the active spacecraft in the viewport.
// Inputs:  view - camera view matrix
//          proj - camera projection matrix
// Actions: Projects g_spacecraft.worldPosition to screen space and draws a colored rectangle
//          and spacecraft name using ImGui's foreground draw list.
static void drawSpacecraftFrame(const glm::mat4& view, const glm::mat4& proj) {
    if (!g_missionActive || !g_showSpacecraftFrame) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImU32 color = colFromVec4(g_spacecraftFrameColor);

    glm::vec4 clip = proj * view * glm::vec4(g_spacecraft.worldPosition, 1.0f);
    if (clip.w <= 0.0f) return;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (std::abs(ndc.x) > 1.5f || std::abs(ndc.y) > 1.5f) return;

    float sx = g_viewportX + (ndc.x * 0.5f + 0.5f) * g_viewportWidth;
    float sy = g_viewportY + (1.0f - (ndc.y * 0.5f + 0.5f)) * g_viewportHeight;
    float box = 14.0f;
    dl->AddRect(ImVec2(sx - box, sy - box), ImVec2(sx + box, sy + box),
                color, 0.0f, 0, 1.5f);
    const char* name = scLabels::kSpacecraft[
        std::clamp(g_spacecraft.config.spacecraftType, 0, 3)];
    ImVec2 ts = ImGui::CalcTextSize(name);
    dl->AddText(ImVec2(sx - ts.x * 0.5f, sy + box + 2.0f), color, name);
}

// drawSpacecraftOrbit
// Purpose: Render the spacecraft's current Keplerian orbital ellipse as a 3D line loop.
// Inputs:  orbitShader - shader program used to draw the orbit line
//          view        - camera view matrix
//          proj        - camera projection matrix
// Actions: Builds a model matrix from the spacecraft's semi-major/minor axes, orbital
//          frame, and eccentricity, then draws a unit-circle orbit mesh scaled to match.
static void drawSpacecraftOrbit(Shader& orbitShader,
                                const glm::mat4& view, const glm::mat4& proj) {
    if (!g_missionActive || !g_showSpacecraftOrbit) return;
    if (g_view == ViewMode::Spacecraft) return;

    static OrbitMesh scOrbit(1.0f, 128);

    const float kmToU = 0.001f;
    float semiMajorScene = g_spacecraft.semiMajorAxisKm * kmToU;
    float semiMinorScene = g_spacecraft.semiMinorAxisKm * kmToU;
    float focusOffset    = semiMajorScene * g_spacecraft.eccentricity;

    glm::vec3 periapsisDir  = g_spacecraft.orbitalFrame[0];
    glm::vec3 orbitNormDir  = g_spacecraft.orbitalFrame[2];
    glm::vec3 semiMinorDir  = g_spacecraft.orbitalFrame[1];

    // Parent body world position = spacecraft world position minus local offset
    glm::vec3 parentBodyPos = g_spacecraft.worldPosition - g_spacecraft.localPositionSceneUnits;

    glm::mat4 M(1.0f);
    M[0] = glm::vec4(periapsisDir  * semiMajorScene, 0.0f);
    M[1] = glm::vec4(orbitNormDir,                   0.0f);
    M[2] = glm::vec4(semiMinorDir  * semiMinorScene, 0.0f);
    M[3] = glm::vec4(parentBodyPos - focusOffset * periapsisDir, 1.0f);

    orbitShader.use();
    orbitShader.setMat4("uView",  view);
    orbitShader.setMat4("uProj",  proj);
    orbitShader.setVec4("uColor", glm::vec4(g_spacecraftOrbitColor.x, g_spacecraftOrbitColor.y,
                                            g_spacecraftOrbitColor.z, g_spacecraftOrbitColor.w));
    orbitShader.setMat4("uModel", M);
    scOrbit.draw();
}

// drawWorldAxes
// Purpose: Draw colored X/Y/Z axis lines at world origin as a debug gizmo, so the
//          user can verify world space is not rotating as the camera moves.
// Inputs:  view - camera view matrix
//          proj - camera projection matrix
// Actions: Projects axis endpoints to screen space and draws labeled colored lines
//          via ImGui's foreground draw list. Axis length scales with camera distance.
static void drawWorldAxes(const glm::mat4& view, const glm::mat4& proj) {
    if (!g_showAxes) return;

    // Axis length scales with camera distance so it's always visible but not silly.
    float axisLen = std::max(50.0f, g_camera.distance * 0.15f);

    auto project = [&](glm::vec3 wp, ImVec2& out) -> bool {
        glm::vec4 clip = proj * view * glm::vec4(wp, 1.0f);
        if (clip.w <= 0.0f) return false;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        out.x = g_viewportX + (ndc.x * 0.5f + 0.5f) * g_viewportWidth;
        out.y = g_viewportY + (1.0f - (ndc.y * 0.5f + 0.5f)) * g_viewportHeight;
        return true;
    };

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 o, xe, ye, ze;
    if (!project(glm::vec3(0.0f), o))                  return;
    bool hx = project(glm::vec3(axisLen, 0, 0), xe);
    bool hy = project(glm::vec3(0, axisLen, 0), ye);
    bool hz = project(glm::vec3(0, 0, axisLen), ze);

    if (hx) {
        dl->AddLine(o, xe, IM_COL32(255, 60, 60, 255), 2.0f);
        dl->AddText(xe, IM_COL32(255, 60, 60, 255), "+X");
    }
    if (hy) {
        dl->AddLine(o, ye, IM_COL32(60, 220, 60, 255), 2.0f);
        dl->AddText(ye, IM_COL32(60, 220, 60, 255), "+Y");
    }
    if (hz) {
        dl->AddLine(o, ze, IM_COL32(80, 120, 255, 255), 2.0f);
        dl->AddText(ze, IM_COL32(80, 120, 255, 255), "+Z");
    }
}

// drawSurfacePins
// Purpose: Draw a bright debug dot at the 0N/0E surface point of each planet to verify
//          that planet textures rotate with the mesh (not independently).
// Inputs:  sim  - solar system providing planet data and scene scale
//          view - camera view matrix
//          proj - camera projection matrix
// Actions: Reconstructs each planet's full model matrix (translate + tilt + spin + scale),
//          transforms the local point (radius, 0, 0) to screen space, and draws a cyan
//          dot via ImGui's foreground draw list.
static void drawSurfacePins(SolarSystem& sim, const glm::mat4& view,
                            const glm::mat4& proj) {
    if (!g_showPins) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    for (size_t i = 0; i < sim.bodyCount(); ++i) {
        const Planet& p = sim.body(i);

        // Reconstruct the same model matrix used when the planet was drawn.
        // Sc * (1,0,0) = (radius, 0, 0) - a point on the equator at prime meridian.
        float r = sim.planetSceneRadius(i);
        glm::mat4 T  = glm::translate(glm::mat4(1.0f), p.worldPos);
        glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), glm::radians(p.axialTiltDeg),
                                   glm::vec3(0, 0, 1));
        glm::mat4 Sp = glm::rotate(glm::mat4(1.0f), glm::radians(p.rotationAngle),
                                   glm::vec3(0, 1, 0));
        glm::mat4 Sc = glm::scale(glm::mat4(1.0f), glm::vec3(r));
        glm::mat4 M  = T * Rx * Sp * Sc;

        glm::vec4 surfaceLocal(1.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 surfaceWorld = M * surfaceLocal;
        glm::vec4 clip = proj * view * surfaceWorld;
        if (clip.w <= 0.0f) continue;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (std::abs(ndc.x) > 1.0f || std::abs(ndc.y) > 1.0f) continue;

        float sx = g_viewportX + (ndc.x * 0.5f + 0.5f) * g_viewportWidth;
        float sy = g_viewportY + (1.0f - (ndc.y * 0.5f + 0.5f)) * g_viewportHeight;

        // Cyan dot + planet name - big enough to spot clearly.
        ImU32 col = IM_COL32(80, 240, 240, 255);
        dl->AddCircleFilled(ImVec2(sx, sy), 5.0f, col);
        dl->AddCircle(ImVec2(sx, sy), 7.0f, IM_COL32(0, 0, 0, 200), 0, 1.5f);
    }
}

// drawHohmannMarkers
// Purpose: Draw a purple X marker and delta-v label at each upcoming Hohmann burn point
//          projected onto the viewport.
// Inputs:  sim  - solar system providing the scene unit conversion factor
//          view - camera view matrix
//          proj - camera projection matrix
// Actions: For each incomplete burn, projects its position (relative to parent body)
//          to screen space and draws an X, delta-v label, and burn duration hint via ImGui.
static void drawHohmannMarkers(const SolarSystem& sim,
                                const glm::mat4& view, const glm::mat4& proj) {
    if (!g_hohmann.active || !g_missionActive) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImU32 col = IM_COL32(180, 80, 255, 230);

    // Parent body is at worldPos − localPos in scene units
    glm::vec3 parentScenePos = g_spacecraft.worldPosition - g_spacecraft.localPositionSceneUnits;
    const float kmToU = sim.kmToSceneUnits();

    // Interplanetary: only burn 1 can be shown (burn 2 is at the target planet)
    int numBurns = g_hohmann.sameBody ? 2 : 1;

    for (int i = 0; i < numBurns; ++i) {
        if (g_hohmann.burns[i].done) continue;
        glm::dvec3 posKm = g_hohmann.burns[i].posRelParent;
        if (glm::length(posKm) < 1.0) continue;

        glm::vec3 worldPt = parentScenePos + glm::vec3(posKm) * kmToU;
        glm::vec4 clip    = proj * view * glm::vec4(worldPt, 1.0f);
        if (clip.w <= 0.0f) continue;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (std::abs(ndc.x) > 1.0f || std::abs(ndc.y) > 1.0f) continue;

        float sx = g_viewportX + (ndc.x * 0.5f + 0.5f) * g_viewportWidth;
        float sy = g_viewportY + (1.0f - (ndc.y * 0.5f + 0.5f)) * g_viewportHeight;

        // Purple X
        const float xr = 8.0f;
        dl->AddLine(ImVec2(sx-xr,sy-xr), ImVec2(sx+xr,sy+xr), col, 2.5f);
        dl->AddLine(ImVec2(sx+xr,sy-xr), ImVec2(sx-xr,sy+xr), col, 2.5f);

        // Label + delta-v
        char lbl[80];
        std::snprintf(lbl, sizeof(lbl), "Burn %d  Δv=%.3f km/s  (%s)",
            i+1, g_hohmann.burns[i].dv_kms,
            g_hohmann.burns[i].prograde ? "PRO" : "RET");
        ImVec2 ts = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(sx - ts.x*0.5f, sy + xr + 3.0f), col, lbl);

        // Burn duration hint
        double dur = g_hohmann.burns[i].duration_s;
        if (dur > 0.0) {
            int m = (int)(dur/60.0); double s = dur - m*60.0;
            char dur_s[32]; std::snprintf(dur_s, sizeof(dur_s), "%d:%05.2f burn", m, s);
            ImVec2 ds = ImGui::CalcTextSize(dur_s);
            dl->AddText(ImVec2(sx - ds.x*0.5f, sy + xr + 16.0f),
                        IM_COL32(200,150,255,200), dur_s);
        }
    }
}

// renderSOISphereWireframes
// Purpose: Render sphere-of-influence boundaries as three orthogonal wireframe circles
//          around each solar system body.
// Inputs:  sim         - solar system providing planet positions and SOI radii
//          orbitShader - shader used to draw the circle lines
//          view        - camera view matrix
//          proj        - camera projection matrix
// Actions: For each body with a nonzero SOI radius, draws three unit-circle meshes
//          scaled and rotated to approximate a sphere wireframe in the XY, XZ, and YZ planes.
static void renderSOISphereWireframes(SolarSystem& sim, Shader& orbitShader,
                                const glm::mat4& view, const glm::mat4& proj) {
    if (!g_showSOI) return;

    static OrbitMesh soiCircle(1.0f, 128);

    orbitShader.use();
    orbitShader.setMat4("uView", view);
    orbitShader.setMat4("uProj", proj);
    orbitShader.setVec4("uColor", glm::vec4(0.2f, 0.9f, 0.4f, 0.6f));

    const glm::mat4 Rx90 = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1,0,0));
    const glm::mat4 Rz90 = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0,0,1));

    for (size_t i = 1; i < sim.bodyCount(); ++i) {
        const Planet& p = sim.body(i);
        if (p.soiRadiusKm <= 0.0f) continue;

        float r = p.soiRadiusKm * sim.kmToSceneUnits();
        glm::mat4 T  = glm::translate(glm::mat4(1.0f), p.worldPos);
        glm::mat4 Sc = glm::scale(glm::mat4(1.0f), glm::vec3(r));

        orbitShader.setMat4("uModel", T * Sc);       soiCircle.draw(); // XZ plane
        orbitShader.setMat4("uModel", T * Rx90 * Sc); soiCircle.draw(); // XY plane
        orbitShader.setMat4("uModel", T * Rz90 * Sc); soiCircle.draw(); // YZ plane
    }
}

// renderSpacecraft
// Purpose: Render the active spacecraft at its current world position and attitude.
// Inputs:  sphere       - fallback sphere mesh used when no 3D model is loaded
//          planetShader - phong-lit shader used for both model and sphere rendering
//          view         - camera view matrix
//          proj         - camera projection matrix
// Actions: If a glTF model is loaded, draws it scaled to either real-world or display
//          size and oriented by the spacecraft's attitude quaternion. Otherwise draws
//          a gray fallback sphere at the spacecraft's world position.
static void renderSpacecraft(SphereMesh& sphere, Shader& planetShader,
                             const glm::mat4& view, const glm::mat4& proj) {
    if (!g_missionActive) return;

    planetShader.use();
    planetShader.setMat4("uView", view);
    planetShader.setMat4("uProj", proj);
    planetShader.setVec3("uLightPos", glm::vec3(0.0f));
    planetShader.setVec3("uLightColor", glm::vec3(1.0f));
    planetShader.setFloat("uAmbient", 0.18f);
    planetShader.setInt("uTexture", 0);

    glm::mat4 T = glm::translate(glm::mat4(1.0f), g_spacecraft.worldPosition);
    glm::mat4 Rattitude = glm::mat4_cast(g_spacecraft.attitudeWorld);

    if (g_currentModel && g_currentModel->loaded) {
        float s;
        if (g_config.realScale) {
            // lengthMeters is the model's natural extent in meters.
            // 1 m = 1e-6 units. Normalize by model radius so the longest
            // dimension of the model equals lengthMeters.
            s = g_config.lengthMeters * 1.0e-6f
              / std::max(g_currentModel->modelRadius, 1e-3f);
        } else {
            float baseFit = g_spacecraft.renderScale
                          / std::max(g_currentModel->modelRadius, 1e-3f);
            s = baseFit * g_config.modelScale;
        }
        glm::mat4 Ruser =
              glm::rotate(glm::mat4(1.0f), glm::radians(g_config.modelRotX), glm::vec3(1,0,0))
            * glm::rotate(glm::mat4(1.0f), glm::radians(g_config.modelRotY), glm::vec3(0,1,0))
            * glm::rotate(glm::mat4(1.0f), glm::radians(g_config.modelRotZ), glm::vec3(0,0,1));
        glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(s));
        glm::mat4 parentM = T * Rattitude * Ruser * S;
        g_currentModel->draw(planetShader, parentM);
    } else {
        // Bind a neutral gray texture so the sphere doesn't inherit a planet texture
        static GLuint scFallbackTex = 0;
        if (!scFallbackTex) scFallbackTex = makeSolidTexture(160, 165, 175);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, scFallbackTex);

        float s = g_spacecraft.renderScale;
        glm::mat4 Sm = glm::scale(glm::mat4(1.0f), glm::vec3(s));
        glm::mat4 M = T * Rattitude * Sm;
        planetShader.setMat4("uModel", M);
        glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(M)));
        planetShader.setMat3("uNormalMat", nm);
        sphere.draw();
    }
}

// renderNavball
// Purpose: Render the spacecraft attitude navball (FDAI) widget in the bottom-center
//          of the viewport with LVLH orbital marker overlays and a throttle bar.
// Inputs:  sphere    - sphere mesh used to draw the navball globe
//          navShader - navball-specific shader that colors hemispheres by orbital orientation
// Actions: Renders the sphere in a fixed sub-viewport with the spacecraft's attitude
//          applied, then draws PRO/RET/RAD/ARAD/N/-N markers, a center crosshair,
//          attitude readout, and an interactive throttle slider via ImGui.
//
// Frame convention (matches LVLH):
//   navball +X (right on screen) = body +X
//   navball +Y (up on screen)    = body +Y (radial-out when unrotated)
//   navball +Z (toward viewer)   = body -Z (nose/prograde when unrotated)
// Hemisphere: blue = radially away from the orbited body, orange = radially toward it.
static void renderNavball(SphereMesh& sphere, Shader& navShader) {
    if (!g_missionActive) return;

    const int   size = 240;
    const float Rpx  = size * 0.425f;   // sphere radius in screen pixels

    // Navball center in screen space
    int   cx_vp = g_viewportX + g_viewportWidth / 2;
    int   vx    = cx_vp - size / 2;
    int   vy_gl = g_viewportY + 16;           // GL Y is bottom-up from viewport bottom
    float scx   = (float)cx_vp;
    float scy   = (float)(g_windowHeight - vy_gl - size / 2); // ImGui Y is top-down

    // ---- Spacecraft body axes in world space ----
    glm::mat3 Rw    = glm::mat3_cast(g_spacecraft.attitudeWorld);
    glm::vec3 bRight =  Rw[0];          // body +X  = navball screen-right
    glm::vec3 bUp    =  Rw[1];          // body +Y  = navball screen-up
    glm::vec3 bFwd   = -Rw[2];          // body -Z  = nose (toward navball viewer)

    // ---- Orbital frame vectors in world space ----
    glm::vec3 radWorld  = (glm::length(g_spacecraft.localPositionSceneUnits) > 1e-9f)
                        ? glm::normalize(g_spacecraft.localPositionSceneUnits)
                        : glm::vec3(0, 1, 0);
    glm::vec3 proWorld  = (glm::length(g_spacecraft.localVelocityDir) > 1e-9f)
                        ? g_spacecraft.localVelocityDir     // already normalised
                        : glm::vec3(1, 0, 0);
    glm::vec3 normWorld = glm::normalize(glm::cross(radWorld, proWorld));
    // Re-orthogonalize prograde against computed normal so the triad is clean
    proWorld = glm::normalize(glm::cross(normWorld, radWorld));

    // ---- Project world-space direction → navball screen coords ----
    // Returns (screen_x, screen_y, depth) where depth>0 = visible hemisphere.
    auto project = [&](glm::vec3 d)
        -> std::tuple<float, float, float>
    {
        float x =  glm::dot(d, bRight);
        float y =  glm::dot(d, bUp);
        float z =  glm::dot(d, bFwd);
        return { scx + x * Rpx, scy - y * Rpx, z };
    };

    // ---- Navball model matrix: identity ----
    // The sphere sits fixed in body frame.  The camera is at (0,0,2.5) looking
    // toward origin, so model +Z (body nose) always maps to the screen centre.
    // Orbital-frame vectors (uRadPole, uProDir) are passed in body-frame
    // coordinates and drive all colouring - exactly mirroring the Python
    // per-pixel approach where (lx,ly,lz) are already in body frame.
    glm::mat4 navModel(1.0f);

    // RAD pole and prograde expressed in navball (model) frame
    glm::vec3 radPoleNav(
        glm::dot(radWorld, bRight),
        glm::dot(radWorld, bUp),
        glm::dot(radWorld, bFwd)
    );
    glm::vec3 proDirNav(
        glm::dot(proWorld, bRight),
        glm::dot(proWorld, bUp),
        glm::dot(proWorld, bFwd)
    );

    // ---- Render sphere ----
    GLint oldVp[4];
    glGetIntegerv(GL_VIEWPORT, oldVp);
    glViewport(vx, vy_gl, size, size);

    glm::mat4 proj = glm::perspective(glm::radians(30.0f), 1.0f, 0.01f, 10.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 2.5f), glm::vec3(0), glm::vec3(0, 1, 0));

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    navShader.use();
    navShader.setMat4("uModel",   navModel);
    navShader.setMat4("uView",    view);
    navShader.setMat4("uProj",    proj);
    navShader.setVec3("uRadPole", radPoleNav);
    navShader.setVec3("uProDir",  proDirNav);
    sphere.draw();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glViewport(oldVp[0], oldVp[1], oldVp[2], oldVp[3]);

    // ---- ImGui overlay: bezel, markers, crosshair ----
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Bezel rings - thick annular ring that frames the sphere without covering it
    dl->AddCircle(ImVec2(scx, scy), Rpx + 7.0f, IM_COL32(20, 20, 20, 255), 80, 14.0f);
    dl->AddCircle(ImVec2(scx, scy), Rpx + 9.0f, IM_COL32(90, 90, 90, 255), 80, 3.0f);
    dl->AddCircle(ImVec2(scx, scy), Rpx + 1.5f, IM_COL32(50, 50, 50, 255), 80, 2.0f);

    // Clip markers to the sphere circle
    dl->PushClipRect(
        ImVec2(scx - Rpx - 1, scy - Rpx - 1),
        ImVec2(scx + Rpx + 1, scy + Rpx + 1),
        true);

    // ---- Marker definitions ----
    struct Marker {
        glm::vec3 dir;
        const char* label;
        ImU32 col;
        int   style;   // 0=prograde, 1=retrograde, 2=rad_out, 3=rad_in, 4=normal_up, 5=normal_dn
    };
    const ImU32 cYellow  = IM_COL32(255, 200,   0, 255);
    const ImU32 cCyan    = IM_COL32(  0, 215, 255, 255);
    const ImU32 cMagenta = IM_COL32(200,  50, 255, 255);

    Marker markers[] = {
        {  proWorld,  "PRO",  cYellow,  0 },
        { -proWorld,  "RET",  cYellow,  1 },
        {  radWorld,  "RAD",  cCyan,    2 },
        { -radWorld,  "ARAD", cCyan,    3 },
        {  normWorld, "N",    cMagenta, 4 },
        { -normWorld, "-N",   cMagenta, 5 },
    };
    const int kNumMarkers = 6;

    // Sort back → front so front markers draw on top
    // Simple insertion sort on z
    int order[kNumMarkers] = {0,1,2,3,4,5};
    for (int i = 1; i < kNumMarkers; ++i) {
        int key = order[i]; int j = i - 1;
        float kz = std::get<2>(project(markers[key].dir));
        while (j >= 0 && std::get<2>(project(markers[order[j]].dir)) > kz) {
            order[j+1] = order[j]; --j;
        }
        order[j+1] = key;
    }

    auto drawMarker = [&](const Marker& m) {
        auto [px, py, pz] = project(m.dir);
        float alpha = (pz >= 0.0f) ? 1.0f : 0.30f;
        // Apply alpha to the stored RGBA
        ImU32 col = (m.col & 0x00FFFFFFu)
                  | ((ImU32)(((m.col >> 24) & 0xFF) * alpha + 0.5f) << 24);

        const float r0 = 9.0f;
        switch (m.style) {
        case 0: // PRO - circle + 3 tick marks (up, left, right)
            dl->AddCircle(ImVec2(px, py), r0, col, 32, 2.0f);
            dl->AddLine(ImVec2(px, py - r0),        ImVec2(px,        py - r0 - 7), col, 2.0f);
            dl->AddLine(ImVec2(px - r0, py),        ImVec2(px - r0 - 7, py),        col, 2.0f);
            dl->AddLine(ImVec2(px + r0, py),        ImVec2(px + r0 + 7, py),        col, 2.0f);
            break;
        case 1: // RET - circle + X + two tail lines
            dl->AddCircle(ImVec2(px, py), r0, col, 32, 2.0f);
            { float d = r0 * 0.68f;
              dl->AddLine(ImVec2(px-d, py-d), ImVec2(px+d, py+d), col, 2.0f);
              dl->AddLine(ImVec2(px-d, py+d), ImVec2(px+d, py-d), col, 2.0f); }
            dl->AddLine(ImVec2(px,      py + r0),     ImVec2(px + 7, py + r0 + 10), col, 2.0f);
            dl->AddLine(ImVec2(px,      py + r0),     ImVec2(px - 7, py + r0 + 10), col, 2.0f);
            break;
        case 2: // RAD OUT - circle + center dot + 4 diagonal ticks
            dl->AddCircle(ImVec2(px, py), r0, col, 32, 2.0f);
            dl->AddCircleFilled(ImVec2(px, py), 2.5f, col);
            for (float deg : {45.0f, 135.0f, 225.0f, 315.0f}) {
                float rad = glm::radians(deg);
                float co = std::cos(rad), si = -std::sin(rad);
                dl->AddLine(ImVec2(px + co*(r0+2), py + si*(r0+2)),
                            ImVec2(px + co*(r0+8), py + si*(r0+8)), col, 2.0f);
            }
            break;
        case 3: // ARAD - circle + X only
            dl->AddCircle(ImVec2(px, py), r0, col, 32, 2.0f);
            { float d = r0 * 0.68f;
              dl->AddLine(ImVec2(px-d, py-d), ImVec2(px+d, py+d), col, 2.0f);
              dl->AddLine(ImVec2(px-d, py+d), ImVec2(px+d, py-d), col, 2.0f); }
            break;
        case 4: // NORMAL - triangle pointing up
            dl->AddTriangle(ImVec2(px,      py - r0 - 2),
                            ImVec2(px - r0, py + 6),
                            ImVec2(px + r0, py + 6), col, 2.0f);
            break;
        case 5: // ANTI-NORMAL - triangle pointing down
            dl->AddTriangle(ImVec2(px,      py + r0 + 2),
                            ImVec2(px - r0, py - 6),
                            ImVec2(px + r0, py - 6), col, 2.0f);
            break;
        }
        // Label below the marker (skipped if it would fall outside the sphere)
        ImVec2 ts = ImGui::CalcTextSize(m.label);
        float lx = px - ts.x * 0.5f;
        float ly = py + r0 + 2.0f;
        dl->AddText(ImVec2(lx, ly), col, m.label);
    };

    for (int i = 0; i < kNumMarkers; ++i)
        drawMarker(markers[order[i]]);

    // Hohmann burn-direction cue: purple X at PRO or RET
    if (g_hohmann.active) {
        int bi = g_hohmann.activeBurn;
        if (bi < 2 && !g_hohmann.burns[bi].done) {
            glm::vec3 burnDir = g_hohmann.burns[bi].prograde ? proWorld : -proWorld;
            auto [bx, by, bz] = project(burnDir);
            float alpha = (bz >= 0.0f) ? 1.0f : 0.35f;
            ImU32 pc = (IM_COL32(200, 80, 255, 0))
                     | ((ImU32)((int)(255 * alpha)) << 24);
            const float xr = 10.0f;
            dl->AddLine(ImVec2(bx-xr,by-xr),ImVec2(bx+xr,by+xr), pc, 2.5f);
            dl->AddLine(ImVec2(bx+xr,by-xr),ImVec2(bx-xr,by+xr), pc, 2.5f);
            ImVec2 bt = ImGui::CalcTextSize("BURN");
            dl->AddText(ImVec2(bx - bt.x*0.5f, by + xr + 2.0f), pc, "BURN");
        }
    }

    dl->PopClipRect();

    // Center crosshair - fixed, always on top, represents spacecraft nose
    const float cr = 8.0f;
    const ImU32 wh = IM_COL32(240, 240, 240, 255);
    dl->AddLine(ImVec2(scx - cr*2.2f, scy), ImVec2(scx - cr, scy), wh, 2.0f);
    dl->AddLine(ImVec2(scx + cr,      scy), ImVec2(scx + cr*2.2f, scy), wh, 2.0f);
    dl->AddLine(ImVec2(scx, scy - cr*2.2f), ImVec2(scx, scy - cr), wh, 2.0f);
    dl->AddLine(ImVec2(scx, scy + cr),      ImVec2(scx, scy + cr*2.2f), wh, 2.0f);
    dl->AddCircle(ImVec2(scx, scy), cr, wh, 20, 2.0f);

    // Attitude readout above the navball
    glm::vec3 eul = g_spacecraft.getEulerAnglesLvlhDeg();
    char buf[80];
    std::snprintf(buf, sizeof(buf), "P%+5.1f  Y%+5.1f  R%+5.1f", eul.x, eul.y, eul.z);
    ImVec2 ts = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(scx - ts.x * 0.5f, scy - Rpx - 18.0f),
                IM_COL32(255, 255, 255, 210), buf);

    // Label below
    dl->AddText(ImVec2(scx - 24.0f, scy + Rpx + 11.0f),
                IM_COL32(180, 180, 180, 200), "NAVBALL");

    // ---- Throttle bar (right of navball, same height as navball diameter) ----
    {
        const float barH     = Rpx * 2.0f;                 // matches navball diameter
        const float barW     = 18.0f;
        const float barLeft  = scx + Rpx + 14.0f;
        const float barRight = barLeft + barW;
        const float barTop   = scy - Rpx;                  // 100 % is at the top
        const float barBot   = scy + Rpx;                  // 0 %  is at the bottom

        // --- Background track ---
        dl->AddRectFilled(ImVec2(barLeft,  barTop),
                          ImVec2(barRight, barBot),
                          IM_COL32(25, 25, 25, 230), 3.0f);
        bool engOn = g_spacecraft.engineOn;
        ImU32 borderCol = engOn ? IM_COL32(120, 200, 80, 220)
                                : IM_COL32(70,  70,  70, 220);
        dl->AddRect(ImVec2(barLeft - 1,  barTop - 1),
                    ImVec2(barRight + 1, barBot + 1),
                    borderCol, 3.0f, 0, 1.5f);

        // --- Fill (0 % at bottom → current throttle) ---
        float thr = g_spacecraft.throttle;
        if (thr > 0.001f) {
            float fillTop = barBot - barH * thr;
            // Color: green → yellow (0–50 %) → orange-red (50–100 %)
            float r = (thr < 0.5f) ? (thr * 2.0f * 200.0f + 20.0f) : 220.0f;
            float g = (thr < 0.5f) ? 200.0f
                                   : std::max(0.0f, 200.0f - (thr - 0.5f) * 2.0f * 200.0f);
            ImU32 fillCol = engOn
                ? IM_COL32((int)r, (int)g, 0, 210)
                : IM_COL32((int)(r * 0.5f), (int)(g * 0.5f), 0, 130); // dim when off
            dl->AddRectFilled(ImVec2(barLeft,  fillTop),
                              ImVec2(barRight, barBot), fillCol, 2.0f);
        }

        // --- Tick marks at 25 %, 50 %, 75 % ---
        for (float pct : {0.25f, 0.50f, 0.75f}) {
            float ty = barBot - barH * pct;
            dl->AddLine(ImVec2(barLeft  - 4, ty), ImVec2(barLeft,      ty),
                        IM_COL32(110, 110, 110, 180), 1.0f);
            dl->AddLine(ImVec2(barRight, ty), ImVec2(barRight + 4, ty),
                        IM_COL32(110, 110, 110, 180), 1.0f);
        }

        // --- Handle: horizontal bar + small side arrows at throttle position ---
        float hy = barBot - barH * thr;
        dl->AddLine(ImVec2(barLeft  - 4, hy), ImVec2(barRight + 4, hy),
                    IM_COL32(240, 240, 240, 230), 2.0f);
        // Left arrow
        dl->AddTriangleFilled(ImVec2(barLeft  - 5, hy),
                              ImVec2(barLeft  - 11, hy - 5),
                              ImVec2(barLeft  - 11, hy + 5),
                              IM_COL32(220, 220, 220, 220));
        // Right arrow
        dl->AddTriangleFilled(ImVec2(barRight + 5, hy),
                              ImVec2(barRight + 11, hy - 5),
                              ImVec2(barRight + 11, hy + 5),
                              IM_COL32(220, 220, 220, 220));

        // --- Axis labels ---
        ImGui::PushFont(nullptr); // default font
        dl->AddText(ImVec2(barLeft - 1.0f, barTop - 14.0f),
                    IM_COL32(160, 160, 160, 200), "THR");
        dl->AddText(ImVec2(barRight + 6, barTop - 4),
                    IM_COL32(130, 130, 130, 180), "100");
        dl->AddText(ImVec2(barRight + 6, barBot - 8),
                    IM_COL32(130, 130, 130, 180), "0");
        ImGui::PopFont();

        // --- Current percentage next to the handle ---
        char pctBuf[8];
        std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(thr * 100.0f + 0.5f));
        ImVec2 pts = ImGui::CalcTextSize(pctBuf);
        // Clamp label so it doesn't go above/below the bar
        float labelY = std::clamp(hy - pts.y * 0.5f, barTop, barBot - pts.y);
        dl->AddText(ImVec2(barLeft - pts.x - 6.0f, labelY),
                    IM_COL32(255, 255, 255, 220), pctBuf);

        // --- Mouse interaction: click-drag to set throttle ---
        ImVec2 mouse = ImGui::GetMousePos();
        bool overBar = mouse.x >= barLeft  - 10.0f && mouse.x <= barRight + 10.0f
                    && mouse.y >= barTop   -  4.0f  && mouse.y <= barBot   +  4.0f;
        if (overBar && ImGui::IsMouseDown(ImGuiMouseButton_Left)
                    && !ImGui::GetIO().WantCaptureKeyboard) {
            float newT = (barBot - mouse.y) / barH;
            g_spacecraft.throttle = std::clamp(newT, 0.0f, 1.0f);
        }
    }
}

// handleKeys
// Purpose: Process keyboard input each frame for spacecraft attitude control, engine
//          toggle, throttle adjustment, and application exit.
// Inputs:  w     - GLFW window handle used to query key states
//          dtSec - real elapsed time in seconds since last frame
// Actions: Arrow keys apply pitch/yaw torque; Q/E apply roll torque; X kills all
//          rotation; F toggles the engine; Z/C ramp throttle up/down. Escape closes
//          the window. Skips spacecraft controls if ImGui has keyboard focus.
static bool g_escPrev = false;
static void handleKeys(GLFWwindow* w, float dtSec) {
    bool escNow = glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escNow && !g_escPrev) glfwSetWindowShouldClose(w, true);
    g_escPrev = escNow;

    if (!g_missionActive || ImGui::GetIO().WantCaptureKeyboard) return;

    const float torque = 0.8f; // rad/s^2 applied while key held
    glm::vec3 bodyT(0.0f);
    // Pitch: arrow up = nose down? Convention: nose up = positive pitch (+X body axis)
    if (glfwGetKey(w, GLFW_KEY_UP)    == GLFW_PRESS) bodyT.x += torque;
    if (glfwGetKey(w, GLFW_KEY_DOWN)  == GLFW_PRESS) bodyT.x -= torque;
    // Yaw around +Y
    if (glfwGetKey(w, GLFW_KEY_LEFT)  == GLFW_PRESS) bodyT.y += torque;
    if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) bodyT.y -= torque;
    // Roll around +Z
    if (glfwGetKey(w, GLFW_KEY_Q)     == GLFW_PRESS) bodyT.z += torque;
    if (glfwGetKey(w, GLFW_KEY_E)     == GLFW_PRESS) bodyT.z -= torque;

    if (bodyT != glm::vec3(0.0f))
        g_spacecraft.applyTorque(bodyT, dtSec);

    // X = kill rotation
    static bool xPrev = false;
    bool xNow = glfwGetKey(w, GLFW_KEY_X) == GLFW_PRESS;
    if (xNow && !xPrev) g_spacecraft.stopRotation();
    xPrev = xNow;

    // F = engine on/off toggle
    static bool fPrev = false;
    bool fNow = glfwGetKey(w, GLFW_KEY_F) == GLFW_PRESS;
    if (fNow && !fPrev) g_spacecraft.engineOn = !g_spacecraft.engineOn;
    fPrev = fNow;

    // Z / C = throttle up / down (continuous while held)
    const float throttleRate = 0.5f; // full range in 2 s
    if (glfwGetKey(w, GLFW_KEY_Z) == GLFW_PRESS)
        g_spacecraft.throttle = std::min(1.0f, g_spacecraft.throttle + throttleRate * dtSec);
    if (glfwGetKey(w, GLFW_KEY_C) == GLFW_PRESS)
        g_spacecraft.throttle = std::max(0.0f, g_spacecraft.throttle - throttleRate * dtSec);
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    g_window = glfwCreateWindow(g_windowWidth, g_windowHeight, "Spacecraft Control System Simulation", nullptr, nullptr);
    if (!g_window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(g_window);
    glfwGetFramebufferSize(g_window, &g_windowWidth, &g_windowHeight);

    glfwSetFramebufferSizeCallback(g_window, framebufferSizeCallback);
    glfwSetCursorPosCallback   (g_window, cursorPosCallback);
    glfwSetMouseButtonCallback (g_window, mouseButtonCallback);
    glfwSetScrollCallback      (g_window, scrollCallback);
    glfwSwapInterval(1);

    if (!gladLoadGL(glfwGetProcAddress)) return -1;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glClearColor(0, 0, 0, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(1.10f);
    io.FontGlobalScale = 1.05f;
    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    Shader planetShader("shaders/planet.vert", "shaders/planet.frag");
    Shader sunShader   ("shaders/sun.vert",    "shaders/sun.frag");
    Shader ringShader  ("shaders/ring.vert",   "shaders/ring.frag");
    Shader orbitShader ("shaders/orbit.vert",  "shaders/orbit.frag");
    Shader skyShader   ("shaders/skybox.vert", "shaders/skybox.frag");
    Shader fdaiShader  ("shaders/fdai.vert",   "shaders/fdai.frag");

    SolarSystem sim; sim.init();
    SphereMesh  spacecraftMesh(48, 32);
    SkyboxMesh  skybox;
    GLuint      milkywayTex = loadTexture("textures/milkywayPanorama.png", true);
    if (!milkywayTex)
        std::cerr << "[skybox] Failed to load textures/milkywayPanorama.png\n";
    scanModelsDirectory();

    // ---- Fetch real planet positions from JPL Horizons (one-time, blocking) ----
    // If the network is down or any call fails, we keep the default analytic
    // positions for those bodies - the sim still runs.
    HorizonsApi::initGlobal();
    {
        // Use today's date (UTC). You could expose this as a user setting.
        std::time_t t = std::time(nullptr);
        std::tm utc{};
#ifdef _WIN32
        gmtime_s(&utc, &t);
#else
        gmtime_r(&t, &utc);
#endif
        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d 00:00", &utc);
        auto real = HorizonsApi::fetchAllPlanets(ts);
        if (!real.empty()) {
            std::unordered_map<std::string, std::array<double, 3>> posMap;
            for (auto& [name, st] : real)
                posMap[name] = { st.xKm, st.yKm, st.zKm };
            size_t seeded = sim.seedFromRealCoords(posMap);
            std::cout << "[Horizons] Seeded " << seeded << " planet(s) with real positions.\n";
        } else {
            std::cout << "[Horizons] Offline / all fetches failed - using analytic positions.\n";
        }
    }

    g_camera.setFocus(glm::vec3(0.0f), 3500.0f, true, 45.0f, 45.0f);

    float last = (float)glfwGetTime();

    while (!glfwWindowShouldClose(g_window)) {
        float now = (float)glfwGetTime();
        float dt = std::min(now - last, 0.1f);
        last = now;

        glfwPollEvents();
        handleKeys(g_window, dt);

        // Compute viewport region (center of window, between sidebars)
        g_viewportX = (int)kLeftSidebarWidth;
        g_viewportY = 0;
        g_viewportWidth = std::max(1, g_windowWidth - (int)kLeftSidebarWidth - (int)kRightSidebarWidth);
        g_viewportHeight = g_windowHeight;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // g_timeScale is sim-seconds per real-second. Legacy update APIs still
        // want days-per-real-second, so convert:  days = sec / 86400.
        const float daysPerRealSec = g_timeScale / 86400.0f;
        if (!g_paused) sim.update(dt, daysPerRealSec);
        if (g_missionActive) {
            float simDtDays = g_paused ? 0.0f : dt * daysPerRealSec;
            g_spacecraft.update(simDtDays, dt, sim);

            // Advance Hohmann burn timer while engine is firing
            if (g_hohmann.active && !g_paused &&
                g_spacecraft.engineOn && g_spacecraft.throttle > 0.0f) {
                int bi = g_hohmann.activeBurn;
                if (bi < 2 && !g_hohmann.burns[bi].done) {
                    g_hohmann.burns[bi].elapsed_s += (double)dt;
                    if (g_hohmann.burns[bi].elapsed_s >= g_hohmann.burns[bi].duration_s) {
                        g_hohmann.burns[bi].done = true;
                        if (bi + 1 < 2) g_hohmann.activeBurn = bi + 1;
                        else            g_hohmann.active = false;
                    }
                }
            }
        }
        trackCameraForView(sim);

        // Clear whole window
        glViewport(0, 0, g_windowWidth, g_windowHeight);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 3D scene in center viewport
        glViewport(g_viewportX, g_viewportY, g_viewportWidth, g_viewportHeight);
        float aspect = (g_viewportHeight > 0) ? float(g_viewportWidth) / float(g_viewportHeight) : 1.0f;

        // Adaptive near/far clip planes based on focus distance (needed for real-scale close-ups)
        float nearPlane = std::max(0.0001f, g_camera.distance * 0.001f);
        float farPlane  = std::max(1.0e6f,  g_camera.distance * 1.0e6f);
        glm::mat4 view = g_camera.getView();
        glm::mat4 proj = g_camera.getProj(aspect, nearPlane, farPlane);

        // Skybox (Milky Way equirectangular panorama)
        glDepthFunc(GL_LEQUAL); glDepthMask(GL_FALSE);
        skyShader.use();
        skyShader.setMat4("uView", view);
        skyShader.setMat4("uProj", proj);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, milkywayTex);
        skyShader.setInt("uMilkyway", 0);
        skyShader.setFloat("uBrightness", g_skyboxBrightness);
        skyShader.setFloat("uYawDeg",     g_skyboxYaw);
        skyShader.setFloat("uPitchDeg",   g_skyboxPitch);
        skyShader.setFloat("uRollDeg",    g_skyboxRoll);
        skybox.draw();
        glDepthMask(GL_TRUE); glDepthFunc(GL_LESS);

        const bool settingsOpen = ImGui::IsPopupOpen("Settings") || ImGui::IsPopupOpen("Help")
                               || ImGui::IsPopupOpen("Set Target Orbit");

        sim.setShowOrbits(g_showPlanetOrbits && !settingsOpen);
        sim.setPlanetOrbitColor(glm::vec4(g_planetOrbitColor.x, g_planetOrbitColor.y,
                                          g_planetOrbitColor.z, g_planetOrbitColor.w));
        sim.render(planetShader, sunShader, ringShader, orbitShader, view, proj);
        if (!settingsOpen) {
            renderSOISphereWireframes(sim, orbitShader, view, proj);
            drawSpacecraftOrbit(orbitShader, view, proj);
        }
        renderSpacecraft(spacecraftMesh, planetShader, view, proj);

        // Restore full-window viewport for overlays and UI
        glViewport(0, 0, g_windowWidth, g_windowHeight);

        if (!settingsOpen) {
            drawPlanetFrames(sim, view, proj);
            drawSpacecraftFrame(view, proj);
            drawWorldAxes(view, proj);
            drawSurfacePins(sim, view, proj);
            drawHohmannMarkers(sim, view, proj);
            renderNavball(spacecraftMesh, fdaiShader);
        }
        drawLeftSidebar(sim);
        drawRightSidebar(sim);
        drawCreateSimulationPopup(sim);
        drawSettingsPopup(sim);
        drawHelpPopup();
        if (g_missionActive) drawHohmannPopup(sim, g_spacecraft);

        glDisable(GL_FRAMEBUFFER_SRGB);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glEnable(GL_FRAMEBUFFER_SRGB);

        glfwSwapBuffers(g_window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(g_window);
    glfwTerminate();
    return 0;
}
