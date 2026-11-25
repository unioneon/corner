# Development

corner is a C++17 / Win32 executable. GDI+ handles masks and image export; GDI
draws text; common controls provide the keyboard-accessible sliders and buttons.
The background renderer and its supervisor wait on events and process handles.

## Build and test

```powershell
.\build.ps1 -Compiler C:\w64devkit\bin\g++.exe
.\build.ps1 -UIAccess
.\tests\smoke.ps1
.\bin\corner.exe --self-test --report result.json
.\bin\corner.exe --export-ui C:\captures --report capture.json
```

The self-test covers alpha reflection, black/white premultiplication, Windows
argument quoting, settings round-trip/replacement, independent percentages,
zero disabling, icon resources, and the selected font. Native UI export creates
light/dark captures and reports the number of partly transparent outline pixels.

The settings window has a supersampled, per-pixel-alpha backdrop. Its input
region covers only opaque pixels, so the outer edge isn't a binary HRGN clip.
The backdrop is click-through, follows the main window, and is destroyed with it.
Slider changes invalidate only the changed row/percentage and use buffered
painting. Overlay preview is passed through WM_COPYDATA; it is persisted only
by Apply. The renderer waits for the preview owner's process and rolls back
if that process terminates unexpectedly.

The private desktop COM helper is isolated from the renderer and bounded to
three seconds. HRESULT failure, absence, or a helper crash leaves standard
topmost rendering in place. Pinning acceptance is not proof of stationary
composition during desktop animations.

## Fonts and resources

Release builds embed the supplied corner icons, not Apple font files. The app
can load user-supplied SF Pro files from `fonts/` beside the executable, or use
an already available family; otherwise it selects Segoe UI. Local `fonts/`,
Apple source files, settings, captures of personal diagnostics and build output
are excluded from Git and release staging.

## Release

```powershell
.\scripts\release.ps1 -InstallerCompiler C:\InnoSetup7\ISCC.exe
```

Artifacts are staged from an explicit allowlist. Personal preferences and
fonts are never copied into downloads. The source archive uses `git archive`
so it matches the committed release. Check `SHA256SUMS.txt` before uploading.

Public downloads are unsigned. Windows may identify the installer publisher as unknown. The protected payload is signed locally during setup; never stage a locally signed binary or export its private key. See [Enhanced renderer](ENHANCED.md).
