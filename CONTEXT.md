# CONTEXT.md — Solar System Simulator Project Handoff

**Purpose of this file:** handoff from a long conversation with Claude in the chat interface to a fresh Claude Code session. Reader should be able to continue development without re-explaining the whole project. Treat this as authoritative; if it conflicts with what code comments say, trust the code and update this file.

---

## 1. Project summary

C++/OpenGL desktop simulator for interactive exploration of the solar system and spacecraft maneuvers. Started as Unreal Engine 5 project, ported to raw OpenGL/GLFW/ImGui for better control and to avoid UE's scale/rendering quirks. Builds on Windows via `build.bat` (CMake + MSVC) and on Linux via standard CMake.

**Not a game; not a precision mission planner.** A visualization / sandbox somewhere between Kerbal Space Program and Orbiter, with Kepler orbital mechanics and an FDAI attitude ball.

## 2. Current state (what works right now)

- **Real-time clock.** Default time scale is 1 sim-second per real-second. Slider ranges from 0.1 to 10,000,000 (sim sec/real sec), logarithmic. Preset buttons: 1x / 1 min/s / 1 hr/s / 1 d/s. Internally still converts to days/sec for legacy consumers (`daysPerRealSec = g_timeScale / 86400`).
- **JPL Horizons API integration.** At startup, `HorizonsApi::fetchAllPlanets()` blocks for ~5–20s fetching today's planet positions from `ssd.jpl.nasa.gov/api/horizons.api`, parses the `$$SOE…$$EOE` CSV block, extracts J2000 heliocentric state vectors (X/Y/Z + VX/VY/VZ in KM-S), and `SolarSystem::seedFromRealCoords()` sets each planet's `orbitAngle = atan2(z, x)`. Graceful fallback to analytic positions if network fails. Uses libcurl with Windows Schannel (no OpenSSL dependency on Windows).
- **glTF planets.** Unreal Engine 5 exports from `models/M_<Name>.gltf` are loaded at init via `Model::loadFromFile`. Each planet's `std::unique_ptr<Model>` is null if no file found — falls back to procedural UV sphere. Same logic for moons.
- **Kepler spacecraft physics.** Newton's-method Kepler solver (12 iterations max), orbital elements (a, e derived from b/a, i, prograde/retrograde). Period from `T = 2π√(a³/μ)`. Orbital speed from vis-viva. Spacecraft orbits its starting parent forever — NO SOI transitions yet.
- **FDAI attitude.** LVLH-relative attitude ball in bottom-center sub-viewport. Controls: arrow keys = pitch/yaw, Q/E = roll, X = stop rotation. Damped at 0.985^(dt*60). Decomposition: `attitudeWorld = attitudeLvlh × attitudeUser`; FDAI shows `attitudeUser` so in stable orbit the ball is still.
- **Milky Way skybox.** `textures/milkyway.jpg` (4096×2048 equirectangular, downsampled from ESO gigapixel eso0932a.tif) sampled via `dirToEquirectangularUV` in `skybox.frag`. 0.85× dim so foreground isn't washed out.
- **Debug overlays.** World-axis gizmo (red/green/blue at origin, toggleable) and live camera readout (`az=X° el=Y° d=Z`) in left sidebar. Surface pins defaulted OFF now — they were for the glTF inside-out-sphere debugging and are no longer needed.

## 3. Directory layout

```
SolarSystemSim/
├── CMakeLists.txt          # FetchContent for everything — no system deps except compiler
├── build.bat               # Windows one-click build
├── README.md
├── CONTEXT.md              # this file
├── src/
│   ├── main.cpp            # entry point, UI, render loop, input
│   ├── Shader.{h,cpp}      # GLSL program wrapper
│   ├── Texture.{h,cpp}     # loadTexture() via stb_image
│   ├── Mesh.{h,cpp}        # SphereMesh, RingMesh, OrbitMesh, SkyboxMesh
│   ├── Camera.{h,cpp}      # OrbitCamera (focus + distance + az/el, lookAt-based)
│   ├── SolarSystem.{h,cpp} # Planet/Moon structs, sim.init/update/render
│   ├── Spacecraft.{h,cpp}  # Kepler orbit + attitude; KEY REFACTOR TARGET
│   ├── Model.{h,cpp}       # tinygltf-based glTF loader (planets + Dragon)
│   ├── HorizonsApi.{h,cpp} # libcurl + nlohmann/json HTTP client
│   ├── tinygltf_impl.cpp   # single-TU impl
│   └── stb_image_impl.cpp  # single-TU impl
├── shaders/
│   ├── planet.{vert,frag}  # diffuse w/ point-light at Sun
│   ├── sun.{vert,frag}     # emissive boost
│   ├── ring.{vert,frag}    # alpha-blended Saturn rings
│   ├── orbit.{vert,frag}   # solid-color line shader for orbit traces
│   ├── skybox.{vert,frag}  # equirectangular Milky Way sampler
│   └── fdai.{vert,frag}    # procedural attitude ball
├── textures/               # 2K planet maps, ring alpha, Milky Way JPG
└── models/                 # glTF + bin + PNG/JPG for planets, Dragon spacecraft
```

## 4. Dependencies (all CMake FetchContent, no system installs needed)

- GLFW 3.4 — windowing
- GLM 1.0.1 — math
- GLAD v2.0.6 — GL loader, core 3.3
- stb_image — texture decode
- Dear ImGui v1.91.6 — UI
- tinygltf v2.9.3 — glTF loader (`TINYGLTF_HEADER_ONLY`)
- nlohmann_json v3.11.3 — JSON parsing for Horizons
- libcurl 8.10.1 — HTTPS, uses **Schannel on Windows** (no OpenSSL!), OpenSSL on Linux

**First configure is slow** (~5 min) because libcurl is huge. Subsequent incremental builds are ~30s. If rebuilding from scratch is needed, expect 5–10 min.

## 5. Scene scale conventions

**1 scene unit = 1000 km.** So Earth's radius (6,371 km) = 6.371 units. Earth's orbit (149.6 Gm) = 149,600 units in REALISTIC mode. Defined at top of `SolarSystem.cpp`.

Two display modes controlled by `g_realisticScale` (default: COMPACT):
- **COMPACT:** orbits compressed 500×, Sun shrunk to 60 units (real 696), moons enlarged 3× with orbits 15×. Good for visual understanding.
- **REALISTIC:** real distances. Needs adaptive near/far planes (`nearP = max(0.0001, cameraDist × 0.001)`, `farP = max(1e6, cameraDist × 1e6)`). Planets become sub-pixel — this is a known UX issue, don't "fix" it without discussion.

**"Real-scale" Spacecraft toggle** in Create Simulation popup is separate — it uses `lengthMeters × 1e-6` as render scale for very small craft.

## 6. Coordinate system

- **Right-handed, world +Y = ecliptic north** (Earth's orbit is in the XZ plane)
- **J2000 reference** from Horizons data — X along vernal equinox, Z perpendicular to Earth's equator
- Planet axial tilt applied around world +Z; spin around world +Y (pre-tilt, so tilt carries the spin axis along with it)

## 7. Camera

`OrbitCamera` in `Camera.h/cpp`:
- Orbits a `focus` point at `distance`, parameterized by `azimuth`/`elevation` (radians)
- `glm::lookAt(position, focus, world_up=(0,1,0))` — has classic gimbal-lock behavior near elevation ±90°
- `elevation` clamped to ±89°
- Right-click + drag rotates, scroll zooms
- **Free-flight camera was explicitly REMOVED** per user request. Don't re-add without asking.
- View modes: Solar System / Planet / Spacecraft. Each calls `snapCameraForView()` to jump the focus and distance appropriately.

## 8. UI structure

Single maximized GLFW window with three regions:
- **LEFT sidebar (330px):** Title, Create Simulation button, End Mission, view radio buttons, debug checkboxes, live camera readout, time controls, scrollable PLANETS list with auto-scaled coordinates (km/Mm/Gm/Tm via `formatCoordKm()`)
- **CENTER viewport:** 3D scene (`glViewport` set explicitly between sidebars)
- **RIGHT sidebar (280px):** IN FLIGHT section (spacecraft name, parent body, position, orbital velocity, a/e/i/dir/T), ATTITUDE (LVLH) pitch/yaw/roll + rate, Controls reference

Create Simulation opens a **modal popup**, not a separate screen. Main menu was removed in favor of always-visible sidebars.

**Engine type dropdown was removed** per user request. Don't re-add.

## 9. Planet rendering

Two code paths:
1. **glTF path (preferred):** if `Planet::model` is non-null, `Model::draw(planetShader, M)` handles it. Scales by `sceneRadius / model->modelRadius` because UE5 editor spheres export with ±1.6 bounds.
2. **Sphere fallback:** procedural `SphereMesh(48, 32)` with a single 2D equirectangular texture.

**Critical fix already applied:** `Model::draw` saves/disables/restores `GL_CULL_FACE`. UE5's glTF exports mark materials as `"doubleSided": true` and their winding doesn't match what back-face culling expects — without this fix, planets render inside-out (far hemisphere visible through the near one). **Don't re-enable culling inside `Model::draw` without accounting for this.**

Planet model matrix (both paths): `M = T × R_tilt × Spin × Scale`. Each factor is camera-independent. Rotation axis for `Spin` is (0,1,0) — local Y after tilt, so tilted planets spin on the tilted axis, which is correct.

## 10. Sun

- Disabled rotation (`rotationAngle` frozen at 0) per user request. At real-time scale the rotation would be invisible anyway, and it visually simplifies the scene.
- Rendered with `sun.frag` (emissive boost 1.6×), doesn't use the planet diffuse shader.
- Acts as the single point light for planet rendering at world origin.

## 11. Spacecraft physics (CURRENT STATE — BEFORE PATCHED CONICS)

Stored in `Spacecraft.h`:
- Orbital elements: `semiMajor` (km), `semiMinor` (km), `inclinationDeg`, prograde bool
- Mean anomaly `M` advances each frame by `n × dt` where `n = 2π/T`
- Newton's method for eccentric anomaly E: `E - e sinE = M`, 12 iterations, tolerance 1e-6
- Position in orbital plane: `(a(cosE - e), b sinE)`
- Rotated into world frame by inclination around X-axis
- Added to parent's `worldPos`

**Fixed parent.** `config.startingBody` determines orbital parent at launch, and spacecraft orbits that forever. This is the thing to change in the patched-conics refactor.

Vis-viva: `getOrbitalSpeedKmS()` returns `sqrt(μ × (2/r - 1/a))`.

## 12. FDAI (attitude indicator)

Three-part decomposition:
- `attitudeLvlh` — quaternion for local orbital frame (continuously changes with orbit)
- `attitudeUser` — pilot's delta from LVLH (identity = stable orbit)
- `attitudeWorld = attitudeLvlh × attitudeUser` — used for rendering spacecraft

Rendered as 200×200 sub-viewport at bottom-center. Procedural shader (`fdai.frag`) paints sky/ground split, lat/lon grid every 15°/10°, horizon band. Rotates with `attitudeUser` so the ball is still when the spacecraft is nose-prograde.

## 13. Horizons integration details

- Endpoint: `https://ssd.jpl.nasa.gov/api/horizons.api`
- Params: `CENTER='500@10'` (Sun barycenter), `VEC_TABLE='3'` (pos+vel), `OUT_UNITS='KM-S'`, `REF_SYSTEM='J2000'`, `CSV_FORMAT='YES'`
- Planet IDs: Mercury=199, Venus=299, Earth=399, Mars=499, Jupiter=599, Saturn=699, Uranus=799, Neptune=899
- Date: today's UTC via `strftime("%Y-%m-%d 00:00")`
- Timeouts: 15s per request, 5s connect. So worst case ~2 minutes for all 8 on a dead network before falling back.
- User has their own working reference at `PlanetCoordinates.cpp` (not in repo) that we mirrored. Don't deviate from its pattern without reason.

## 14. Roadmap — patched conics

Agreed sequence for physics work (in order):

### Stage 1 — State-vector refactor of Spacecraft (NEXT)
- Change ground truth from orbital elements to `worldPos` + `worldVel` (glm::vec3 or better, glm::dvec3)
- Orbital elements become derived-on-demand
- No behavior change yet; still orbits starting parent only
- Add state↔elements converter functions (~30 lines) — need both directions

### Stage 2 — SOI transitions
- Add SOI radii to `Planet` struct (or a lookup table). Values agreed:
  - Mercury 112,000 km / Venus 616,000 / Earth 924,000 / Mars 577,000
  - Jupiter 48.2M / Saturn 54.5M / Uranus 51.8M / Neptune 86.6M
  - Moon 66,100 (relative to Earth)
- Per-frame parent check: find body with smallest `r / r_SOI` ratio
- **Hysteresis:** only switch parent if new ratio < 0.95 × current. Prevents flicker at boundaries.
- On parent switch: recompute orbital elements from current state vectors relative to new parent

### Stage 3 — SOI wireframe visualization
- Debug toggle for wireframe spheres around each body at SOI radius
- Use the existing orbit shader pattern — line loops of circles in 3 planes
- Makes the transitions actually observable

### Stage 4 (optional, may be skipped per time) — N-body RK4 mode
- Numerical integrator using all bodies' gravity
- Use `glm::dvec3` for state to avoid float precision drift over long sims
- Sub-step the frame dt: cap integration step at ~10s of sim time
- UI toggle in Create Simulation popup: "Physics: Patched Conics / N-Body"
- RK4 code template is in the chat transcript if needed

## 15. Explicit user preferences (do NOT change without asking)

- **Time scale default 1:1 real-time.** Not 1 day/sec.
- **Sun rotation disabled.**
- **Free-flight camera removed.** Orbit camera only.
- **Engine type dropdown removed.**
- **Main menu screen removed.** Sidebars always visible, modal popup for new-mission setup.
- **Planets list shows coordinates under each entry** in left sidebar with auto-scaled units.
- **Surface pins disabled by default.** Leave the toggle and code, just default off.

## 16. Known gotchas and pitfalls

- **First libcurl build is 5+ minutes.** Don't assume a hang.
- **glTF back-face culling** — already documented above, but critical. If you add new glTF rendering paths, remember `doubleSided` materials.
- **Horizons CSV parsing** — the regex `\$\$SOE([\s\S]*?)\$\$EOE` is critical. Handles newlines. If Horizons changes output format, this is the first thing to break.
- **Texture paths in glTF are relative to the .gltf file.** Keep `M_<Name>.gltf`, `M_<Name>.bin`, and the PNG/JPG in the same folder.
- **Four of the 8K planet textures** (Mercury, Venus, Moon, Mars) are 4K JPGs in the shipped zip to keep download size reasonable. Originals were 50MB+ each. Their glTFs reference the .jpg filenames. User has the 8K originals if they want to swap back.
- **Moon glTF lookup** uses `models/M_Moon.gltf` but moon's `name` field is literally "Moon" — if another planet ever gets a moon named "Moon" (e.g., just "Earth's moon"), the naming collision would matter.
- **libcurl global init/shutdown** are called at main start/end. Don't double-init.
- **stb_image does NOT support TIFF.** The Milky Way original was 29MB LZW TIFF; it was pre-converted to JPG. Same goes for any future asset.

## 17. Ongoing unresolved thread

There was extended debate about "planets appearing to rotate as camera rotates." Final diagnosis: **inside-out sphere bug from glTF winding** (fix in `Model::draw`). Pre-fix visual was the far hemisphere showing through. Confirmed resolved. The world-axis gizmo and surface-pin debug features were built to diagnose this and are still in the code — feel free to remove the pin code entirely if it feels redundant now, or leave it for future surface-based features.

## 18. Communication conventions with the user

- User is technical, building this as a learning/exploration project, has their own Horizons API reference code
- Prefers **honest assessment** over reflexive agreement. Call out risk, say what I don't know, suggest when a request is bigger than fits in one turn
- Prefers **step-by-step progress** with verified builds over "throw everything at the wall"
- Drops screenshots when something looks wrong — read them carefully before concluding it's working as intended
- Console output (prefix `[Horizons]`, `[models]`, etc.) is the main runtime diagnostic channel since I can't see the OpenGL window
- When user says something is broken, **press for specifics before arguing the code is correct.** The "inside-out sphere" issue cost several turns because I interpreted "texture rotating" as camera parallax instead of asking what exactly looked wrong. Don't repeat that.

## 19. Useful pointers for quick orientation

- Entry point: `src/main.cpp`, `main()` around line 690-ish
- Planet data table: top of `SolarSystem::init()` in `SolarSystem.cpp` — all physical constants live here
- Render dispatch: `SolarSystem::render()` — Sun then planets-and-moons then rings then orbits
- Spacecraft update: `Spacecraft::update(simDtDays, realDtSec, sim)`
- Scan `TODO` or `XXX` comments in source before making assumptions about intent

---

*End of context. Ready to continue at Stage 1 of patched conics roadmap.*
