# Shell layer validation

Ordinary corner windows use desktop band 1. Live inspection on the test Windows
system confirmed that opening Start promotes Shell_TrayWnd from band 1 to band
6. The previous repeated SetWindowPos recovery could restore corners after
closing Start, but could not establish a persistent cross-band ordering.

The installed locally signed UIAccess renderer reports TokenUIAccess=1 and
creates topmost corners in band 2, which is above immersive shell bands despite
the numeric band identifiers. Twenty consecutive measurements while Start was
open all retained four visible UIAccess corner windows. This verifies native
window ordering, not a captured compositor-frame sequence. The user separately
confirmed that corners stayed visible in games and with Start open.

Direct Task Scheduler activation of the protected executable failed with error
740. The ordinary native supervisor instead uses ShellExecuteEx for activation
and retains its event-driven process wait and crash recovery. The scheduled task
still owns the supervisor and keeps its restart-on-failure configuration.

`build.ps1 -UIAccess` emits a separate unsigned payload in bin/uiaccess. The
public installer explains UAC and automatically performs the local signing
step. Per-machine keys, certificates, preferences, diagnostic reports and task
backups must never be included in public downloads. See ../ENHANCED.md for the
security implications and supported maintenance commands.