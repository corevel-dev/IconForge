# Icon Forge v3 (UE 5.x Editor plugin)

Studio for rendering item icons from Static Meshes: orbit camera, 3-point lighting presets,
transparent background, PNG + UI Texture asset output, batch rendering.

## Install
1. Copy the `IconForge` folder into `<Project>/Plugins/`.
2. Delete `Plugins/IconForge/Binaries` and `Plugins/IconForge/Intermediate` if they exist.
3. Regenerate project files and build the editor (C++ project required).

## Open
* **Tools > Icon Forge**, or
* right-click one or several Static Meshes in the Content Browser > **Icon Forge: Make Icon(s)**.

## Layout
| Panel | What it does |
|---|---|
| Header | Status (mesh, size, outputs), Presets, Reset, **Batch N**, **Shot!** |
| MESHES | Asset list. Click = on stage, double-click = instant shot, Ctrl/Shift+click = batch selection. Chips: Project only / hide LODs |
| STAGE | Viewport with exactly the icon aspect ratio. Camera presets 1-7, Frame, Reset, lighting presets, Guides |
| SETTINGS | Quick chips (size, supersampling, background, outputs) + full details panel |
| RESULT | Preview on checkerboard, paths, Open Folder / Find in Content / Copy Path, history |

## Controls (stage)
LMB orbit, MMB or Shift+LMB pan, RMB / wheel zoom, Ctrl = precise, F or double-click = frame,
R = reset to 3/4, 1-7 camera presets, G = guides, Space / Enter = Shot!

## Files
* Session (autosave): `Saved/IconForge/Session.json`
* Presets: `Saved/IconForge/Presets/*.json`
* PNG default: `Saved/Icons`, texture assets default: `/Game/Icons`
