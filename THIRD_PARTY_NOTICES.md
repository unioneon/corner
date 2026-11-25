# Third-party notices

## Fonts

No Apple font files are distributed in this repository or in release packages.
corner uses SF Pro Display if it is available locally, with Segoe UI as the
Windows fallback. Local font files, when supplied by the user, are loaded for
the application process only. They remain subject to their original licence.
See [Apple's font licence](https://developer.apple.com/fonts/).

## Toolchain

Release executables are built with MinGW-w64 GCC. They statically link GCC
support libraries under the [GCC Runtime Library Exception](https://www.gnu.org/licenses/gcc-exception-3.1.html)
and the MinGW-w64 runtime under its applicable permissive notices.
Windows APIs and GDI+ are supplied by Windows, not bundled with corner.

## Visual assets

The corner icon and UI design were supplied by unioneon. The poster is
AI-assisted promotional artwork based on the actual app UI; the separate
light/dark settings images are captured from the compiled native controls.

## Experimental desktop integration

The optional `IVirtualDesktopPinnedApps` ABI declaration was checked against
the [Windhawk virtual desktop helper](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/virtual-desktop-helper.wh.cpp).
No Windhawk binary or implementation is bundled. Private API calls are
isolated in a disposable helper process, with a timeout and a normal overlay
fallback.
