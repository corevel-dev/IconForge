# Changelog

## 3.0.0
### New interface (Causality Engine design language)
* Own style set `IconForgeStyle`: dark rounded panels, accent colour, chips, captions, pills.
* Header with logo, live status (mesh / size / outputs), Presets, Reset, Batch N, big **Shot!** button.
* Panels MESHES / STAGE / SETTINGS / RESULT; camera and lighting presets as buttons above the stage.
* Quick settings chips: size 64-2048, supersampling 1-4x, transparent / solid, PNG / texture.
* Stage overlays: mesh info, output size, shortcuts, empty-state hint.
* Result: preview with correct proportions on checkerboard, SAVED / PROBLEM status, Copy Path, Clear history,
  red marker on failed shots in the history.
* Plugin icon `Resources/Icon128.png`.

### Fixes
* Space / R / W / E were eaten by the editor viewport commands (gizmo cycle, scale mode): hotkeys now go first.
* Typing "1".."7" or Space in the search box triggered camera presets / shots.
* Holding Space fired a shot on every key repeat.
* Editing a sub-field (e.g. MeshRotation.Yaw) did not re-frame (member property name not used).
* After Batch the user's own pan / zoom was lost (only the angle was restored).
* Forced LOD0 stayed on after a failed render (scope guard).
* Supersampling could create render targets up to 16384 px: now clamped to 8192.
* Lumen GI / reflections made single captures noisy and different from the viewport: disabled for the studio.
* Textures still compiling (UE5 async) produced blurry icons: wait for texture & shader compilation.
* Creating a texture asset over an existing asset of another class crashed: now reported as an error.
* Invalid characters from mesh names / patterns broke asset names; package path is validated.
* "Preset saved" was reported even when writing failed; loading a preset no longer resets list filters / camera.
* Session JSON was written on every slider change: now autosaved once per second.
* Content Browser menu used only the first of several selected meshes: all are now selected for Batch
  (and the Project filter is turned off automatically for engine / plugin meshes).
* Mouse drag could stay stuck after losing capture (Alt+Tab).
* Stale FOV after changing FOV / size: framing now uses the new value immediately.
* Batch saved every package separately: now one save at the end; render targets reused within a batch.
* Missing include for FSlateApplication in the module.

### Other
* `{BaseName}` token (mesh name without SM_ / S_), `Overwrite Existing` option (adds _2, _3...),
  Ambient Occlusion setting, last camera angle restored on reopen, axis gizmo hidden.
