# corner 1.3.0 — Game Mode + corners above Start

- **Game Mode:** a new Game entry directly below Settings in the tray menu. A blue dot means enabled; click again to disable. Your choice is saved.
- Game Mode keeps corners enabled in fullscreen and adds a layer-order check only while a fullscreen window is active.
- **Corners above Start:** setup automatically installs a locally signed UIAccess component so the taskbar no longer covers the lower corners while Start is open.
- Fixed event handling and short layer-order recovery during shell and fullscreen transitions.
- Updated the native supervisor, shortcuts and management commands for the protected renderer.
- The user confirmed that corners now work in games and with Start open on their Windows system. Borderless fullscreen is recommended; every game, anti-cheat system and true exclusive fullscreen mode is not guaranteed.

> [!IMPORTANT]
> **Why approve UAC during installation?** Administrator approval lets setup
> install corner's protected component in Program Files and configure a local
> trusted code-signing certificate. **This keeps corners visible above Start and
> the taskbar.** Setup does this automatically; no extra setting is needed.
> The private key stays on your PC. Uninstall removes the component, certificate
> and key. UIAccess expands access to other applications' UI; its use here is experimental.

The public downloads contain unsigned payloads, never a developer's local key
or pre-trusted certificate. Microsoft reserves UIAccess for accessibility apps;
see the README for installation details. Portable mode remains available without
administrator approval; Start coverage requires the installed component.

## corner 1.2.2

- Aligned the settings controls with the supplied SVG layout.
- Corrected the lower row spacing, logo position and Start Windows alignment.
- Kept slider tracks on a consistent vertical axis across Windows themes.
- Window dimensions, linking strategy and saved preferences are unchanged.

## corner 1.2.1

- Corners now remain visible in browser fullscreen, including YouTube, by default.
- Added **Fullscreen → Show corners / Hide corners** to the logo menu.
- Fullscreen preference participates in live preview, Apply and rollback.
- Foreground fullscreen windows are included in event-driven overlay ordering.
- Corrected the compiler path used by the Windows GitHub Actions build.

## Included from 1.2.0

The first public release of corner for Windows 10 / 11 x64.

- Independent upper and lower continuous-corner masks, 0–100%.
- Immediate preview, explicit Apply, and rollback when closing without saving.
- Smooth light/dark settings window, English tray menu and original corner icons.
- Black/white masks and system/light/dark appearance.
- Per-user sign-in task, event-driven rendering and renderer recovery.
- Click-through masks, best-effort capture exclusion and optional fullscreen hiding.
- Optional isolated desktop pinning with an ordinary-overlay fallback.

No Apple font files, personal preferences or machine diagnostics are included.
The corner contour is approximate. Capture exclusion and virtual-desktop
animation behavior depend on the Windows build and capture path.
