# corner 1.2.2

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
