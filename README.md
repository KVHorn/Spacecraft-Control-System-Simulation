# Spacecraft-Control-System-Simulation

A real-time 3D Spacecraft Control System Simulation built with C++ and OpenGL 3.3. All eight planets orbit the Sun with accurate sizes, orbital periods, axial tilts, and real-world inclinations relative to the ecliptic plane. Planet positions are seeded from live JPL Horizons data on startup when a network connection is available. A full spacecraft mission system lets you place a crewed vehicle in orbit around any body and fly it with engine and attitude controls.

---

## Features

- All 8 planets + Sun with real radii, rotation periods, and axial tilts
- Real-world orbital inclinations and ascending nodes (J2000 ecliptic)
- Live planet positions fetched from the JPL Horizons API on startup
- Earth's Moon orbiting with the correct 27.32-day period
- Saturn's rings with transparency
- Milky Way equirectangular panorama skybox
- **Compact vs Realistic scale modes** — switch between compressed (easy viewing) and true proportional solar system scale
- **Spacecraft mission system**
  - Place a spacecraft in orbit around any body with configurable semi-major/minor axis and inclination
  - Load custom 3D models (`.gltf` / `.glb`) for the spacecraft
  - Patched Conics (Kepler) or full N-Body RK4 gravitational physics
  - Real engine data (thrust, Isp, propellant mass) for Apollo CSM, Soyuz, Dragon 2, and an uncrewed probe
  - Sphere-of-influence transitions between bodies (patched conics mode)
- **Navball (FDAI)** — shows spacecraft attitude relative to the LVLH orbital frame with prograde, retrograde, radial, anti-radial, and normal markers
- Throttle bar with mouse interaction
- In-app Settings panel — display colors, scale controls, sun/skybox brightness, skybox rotation, physics mode
- In-app Help panel — full reference for all controls and features

---

## Requirements

| Requirement | Version |
|---|---|
| Windows | 10 or 11 (64-bit) |
| Visual Studio | 2019 or later — **Desktop development with C++** workload |
| CMake | 3.16 or later — https://cmake.org/download/ |
| Git | Any recent version — https://git-scm.com/download/win |
| GPU | OpenGL 3.3 support (any GPU from ~2010 or later) |

You do **not** need to install any libraries manually. CMake's `FetchContent` downloads and compiles GLFW, GLM, GLAD, Dear ImGui, tinygltf, nlohmann/json, and libcurl automatically on first configure. An internet connection is required for the first build.

---

## Folder Structure

```
Spacecraft-Control-System-Simulation/
├── CMakeLists.txt
├── README.md
├── build.bat                         One-click Windows build script
├── src/                              C++ source files
├── shaders/                          GLSL vertex and fragment shaders
├── textures/                         Planet textures, skybox panorama
│   ├── Unreal Assets/                MS_Sun_BaseColor.png (sun texture)
│   └── 3d SpaceCraft Assets/         Spacecraft .gltf / .glb models
├── models/                           Optional high-quality planet glTF meshes
└── bin/
    └── Release/                      Built executable + copied assets (run from here)
```

---

## Texture Files

The following textures are expected in the `textures/` folder. The [Solar System Scope texture pack](https://www.solarsystemscope.com/textures/) (CC-BY 4.0) provides all of these at the expected filenames. If your filenames differ, edit the `texturePath` strings in `src/SolarSystem.cpp`.

| File | Body | Notes |
|---|---|---|
| `Unreal Assets/MS_Sun_BaseColor.png` | Sun | High-resolution sun surface |
| `2k_mercury.jpg` | Mercury | |
| `2k_venus_atmosphere.jpg` | Venus | Cloud-top view |
| `2k_earth_daymap.jpg` | Earth | |
| `2k_moon.jpg` | Moon | |
| `2k_mars.jpg` | Mars | |
| `2k_jupiter.jpg` | Jupiter | |
| `2k_saturn.jpg` | Saturn | |
| `2k_saturn_ring_alpha.png` | Saturn rings | **PNG with alpha channel required** |
| `2k_uranus.jpg` | Uranus | |
| `2k_neptune.jpg` | Neptune | |
| `milkywayPanorama.png` | Skybox | Equirectangular Milky Way panorama |

Missing textures are silently skipped; the program still runs with a placeholder.

---

## Building

### Option 1 — One-click (recommended)

Double-click `build.bat` in File Explorer, or run it from a command prompt:

```
cd C:\Spacecraft-Control-System-Simulation
build.bat
```

The first build takes 5–10 minutes while CMake downloads and compiles all dependencies. Subsequent builds are fast. The executable is placed at:

```
bin\Release\SpacecraftControlSystemSimulation.exe
```

### Option 2 — Manual CMake

```
cmake -S . -B build
cmake --build build --config Release
```

### Option 3 — Visual Studio

Open the `SpacecraftControlSystemSimulation` folder using **File → Open → Folder**. Visual Studio detects the `CMakeLists.txt` automatically. Select `SpacecraftControlSystemSimulation.exe` as the startup item and press **F5** to build and run.

### Troubleshooting

| Error | Fix |
|---|---|
| `'git' is not recognized` | Install Git and choose "Git from the command line" during setup |
| CMake can't find a compiler | Run `build.bat` from a **Developer Command Prompt for Visual Studio** (Start Menu → Visual Studio 2022 → Developer Command Prompt) |
| Build fails on first run | Make sure you have an internet connection — FetchContent needs to download dependencies |

---

## Running

Double-click `bin\Release\SpacecraftControlSystemSimulation.exe`. The app launches maximized with the solar system visible and two control sidebars.

> **Note:** The app contacts the JPL Horizons API on startup to seed accurate planet positions for today's date. If you are offline it falls back to analytic positions automatically — no action required.

---

## Controls

### Camera

| Input | Action |
|---|---|
| Right-click + drag | Rotate camera around the focus point |
| Scroll wheel | Zoom in / out |
| Click a planet name (left panel) | Jump camera focus to that planet |

### Spacecraft Attitude (requires active mission)

| Key | Action |
|---|---|
| Up / Down arrows | Pitch nose up / down |
| Left / Right arrows | Yaw left / right |
| Q / E | Roll clockwise / counter-clockwise |
| X | Kill rotation (zero angular velocity instantly) |

### Propulsion (requires active mission)

| Key | Action |
|---|---|
| F | Toggle engine on / off |
| Z | Throttle up (hold) |
| C | Throttle down (hold) |
| Throttle bar (navball area) | Click or drag to set throttle directly |

### General

| Key | Action |
|---|---|
| Escape | Quit |

---

## Interface Overview

### Left Sidebar

- **Create Simulation** — Opens a configuration dialog to place a spacecraft in orbit. Set spacecraft type, parent body, orbit shape (semi-major/minor axis, inclination), 3D model, and physics mode, then click Launch.
- **Run Default Simulation** — Instantly launches a Dragon 2 capsule in a 420 km circular orbit around Earth at 51.6° inclination (similar to the ISS).
- **End Mission** — Removes the active spacecraft.
- **Settings** — Opens the Settings panel (see below).
- **Help** — Opens an in-app reference for all controls and features.
- **View radio buttons** — Switch between Solar System overview, Planet close-up, and Spacecraft chase camera.
- **Display checkboxes** — Toggle planet frames, planet orbit paths, spacecraft frame, and spacecraft orbit path.
- **Time controls** — Pause/Resume button, time scale slider (1× = real time), and quick-preset buttons (1×, 60×, 3600×, 86400×).
- **Planet list** — Shows each body with its current heliocentric position. Click to focus the camera.

### Right Sidebar

Shows live telemetry for the active spacecraft: position, orbital velocity, orbital elements (semi-major axis, eccentricity, inclination, period), engine state, throttle, fuel remaining, and attitude (Pitch/Yaw/Roll from LVLH frame).

### Navball

Displayed at the bottom-center of the viewport when a spacecraft is active. Shows spacecraft attitude relative to the orbital Local Vertical / Local Horizontal (LVLH) frame.

| Marker | Color | Meaning |
|---|---|---|
| PRO | Yellow | Prograde — direction of orbital velocity |
| RET | Yellow | Retrograde |
| RAD | Cyan | Radially away from orbited body |
| ARAD | Cyan | Radially toward orbited body |
| N / -N | Purple | Orbital normal (perpendicular to orbit plane) |
| Crosshair | White | Spacecraft nose direction |

Markers are dimmed when on the back hemisphere. The throttle bar is displayed to the right of the navball.

### Settings Panel

| Section | Option | Description |
|---|---|---|
| Display | Checkboxes + color pickers | Toggle and colorize planet/spacecraft frames and orbit paths |
| Scale | Compact / Realistic radio | Compact compresses orbits ~500× for easy viewing; Realistic uses true proportional distances |
| Scale | Orbit compression slider | Controls how compressed Compact mode is |
| Scale | Planet / Sun size multipliers | Scale visual body sizes independently of orbit scale |
| Visuals | Sun brightness | Emissive intensity of the sun surface |
| Visuals | Skybox brightness | 0 = black, 1 = neutral, 2 = overexposed |
| Visuals | Skybox Yaw / Pitch / Roll | Rotate the Milky Way panorama to your preferred orientation |
| Physics | Patched Conics | Fast Keplerian ellipses with instant sphere-of-influence transitions |
| Physics | N-Body RK4 | Full gravitational simulation from all planets (more accurate, slower at high time scales) |

---

## Architecture

| File | Role |
|---|---|
| `src/main.cpp` | Window setup, main loop, all ImGui UI, input handling, overlay rendering |
| `src/SolarSystem.cpp/.h` | Planet data, orbit simulation, scale modes, rendering |
| `src/Spacecraft.cpp/.h` | Orbital mechanics (Kepler + N-Body RK4), SOI transitions, propulsion, attitude |
| `src/Camera.cpp/.h` | Orbit camera with adjustable focus, azimuth, elevation, and distance |
| `src/Model.cpp/.h` | glTF 2.0 model loader (tinygltf) |
| `src/HorizonsApi.cpp/.h` | HTTP client fetching live planet positions from NASA JPL Horizons |
| `src/Shader.cpp/.h` | GLSL shader compilation and uniform setters |
| `src/Mesh.cpp/.h` | `SphereMesh`, `RingMesh`, `OrbitMesh`, `SkyboxMesh` geometry |
| `src/Texture.cpp/.h` | stb_image texture loader |
| `shaders/planet.*` | Diffuse-lit planet shader |
| `shaders/sun.*` | Emissive sun shader with limb darkening |
| `shaders/ring.*` | Alpha-blended Saturn ring shader |
| `shaders/orbit.*` | Colored line shader for orbit paths and SOI wireframes |
| `shaders/skybox.*` | Equirectangular panorama shader with brightness and rotation uniforms |
| `shaders/fdai.*` | Navball hemisphere shader (blue/orange based on radial direction) |

Render order per frame: **skybox → planet orbit paths → Sun → planets + moons → rings → spacecraft → ImGui overlays (navball, frames, labels)**.

---

## License

Code: MIT — do whatever you want with it.
Textures: check your texture pack's license. Solar System Scope textures are CC-BY 4.0 — credit them if you redistribute.
