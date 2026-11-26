# Multi-monitor preview validation

Rounded mode covers all active logical displays. Each has four independent
click-through windows, sharing the current Up/Down percentages, color, enabled
state and Game preference. Mask dimensions are physical pixels and are not
multiplied by a monitor's text/UI scaling. A mirrored desktop gets one logical
set of windows, which Windows mirrors to its outputs.

Display enumeration uses active CCD paths and EnumDisplayMonitors desktop
rectangles. Extended desktops can have negative X/Y positions. Explicit
`--panel` selection still limits the renderer to one target; exact PNG panel
masks retain their original single-display resolution requirement.

WM_DISPLAYCHANGE / DPI notifications trigger immediate reconciliation and two
bounded follow-up checks. Unchanged layouts retain their windows. New layouts
are prepared with capture exclusion before replacing the old windows. Removing
a display destroys its corner windows. There is no idle topology polling.
Fullscreen visibility is calculated per display instead of hiding the entire
set when a fullscreen window covers only one screen.

## Automated checks

The normal `--self-test` / `tests/smoke.ps1` sequence now checks:

- All-display selection, external-only output and explicit target selection.
- Separate positions for eight actual hidden HWNDs on a two-display layout.
- Negative X and Y positions, unequal resolutions and portrait orientation.
- Logical display deduplication for mirrored output paths.
- Replacement sequences: two displays, one display, three displays, vertical
  arrangement, mirrored output, no outputs, then an external-only output.
- Old HWNDs are destroyed and each new window's native rectangle matches its
  target mask position; synthetic test windows remain hidden throughout.
- Fullscreen on one rectangle leaves the other display's policy visible;
  Game Mode overrides hiding.
- Legacy exact-mask restrictions and the existing UI/DPI smoothing checks.

Reports explicitly contain `physicalMultiMonitorTest: not_tested`. These tests
exercise application logic and real window placement, not a virtual display
driver, physical hotplug, mixed-DPI hardware, game presentation or a recording
pipeline spanning two monitors. No display driver is installed for testing.

## Hardware confirmation still needed

On a PC with two screens, install the preview, confirm its normal UAC prompt,
and check that status reports `monitors: 2`, `corners: 8` and (when enabled)
`visibleCorners: 8` with both Up and Down enabled in Extend mode. Mirrored screens normally report one logical
monitor. Move the secondary display to the left and above the primary in
Windows, try portrait orientation and unequal DPI scales, and disconnect then
reconnect it. Check fullscreen video or a game on each display, including the
hide-fullscreen preference and Game Mode. Verify Start, clicks and capture
exclusion on each output using the recorder actually used by the tester.

Do not report those hardware checks as passed until they have been performed.

API references:
- https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-enumdisplaymonitors
- https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-querydisplayconfig
- https://learn.microsoft.com/windows/win32/gdi/wm-displaychange
