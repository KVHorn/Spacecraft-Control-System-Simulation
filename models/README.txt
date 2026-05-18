Put your .glb or .gltf spacecraft models here.

The app scans this folder (relative to SolarSystemSim.exe) at startup, so the
files at build time need to end up next to the exe. CMake is set up to copy
this folder into bin/Release/ automatically.

Supported formats:
  - .glb  (preferred: binary packed, includes embedded textures)
  - .gltf (JSON + separate .bin + external textures; bring the whole set)

Typical workflow:
  1. Drop your model files (e.g. apollo_csm.glb, soyuz.glb) into this folder.
  2. Rebuild OR copy them directly into bin/Release/models/
  3. Launch SolarSystemSim.exe -> the files appear in the menu's Model dropdown.
  4. Click "Rescan" in the menu to pick up newly added files without restarting.

Tips:
  - Scale varies wildly between sources. Use the Scale slider in the menu
    (log-scaled 0.001 - 100) to dial in the right size.
  - Orientation varies too. Use the Rot X/Y/Z sliders to align the model.
    The spacecraft's "forward" direction is the prograde tangent; the model's
    natural +Z is treated as forward before your rotation is applied.
  - If a model is huge (millions of triangles), decimate it in Blender:
    Mesh -> Decimate modifier -> ratio ~0.1.
  - Free NASA models: https://nasa3d.arc.nasa.gov
    Most are in OBJ but many are on Sketchfab as glTF already.
