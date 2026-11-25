# Protected renderer and local signing

Setup automatically configures the protected renderer. The Windows UAC prompt
is needed to write to Program Files and configure its local certificate. No
extra mode selection or custom confirmation dialog is shown during setup.
The standalone `enhanced.ps1 Enable` maintenance command shows an explicit
consent dialog before requesting UAC; setup passes its already disclosed consent.

The renderer lives in `%ProgramFiles%\corner`. Preferences stay in
`%LOCALAPPDATA%\corner\settings.ini`; the small native supervisor and ordinary
executable also remain in the per-user folder. There is no resident PowerShell
launcher. Windows Task Scheduler starts the supervisor, which activates the
protected renderer through ShellExecuteEx and waits on its process handle.

Windows requires a trusted Authenticode signature and a protected location for
UIAccess. Setup creates a nonexportable RSA code-signing key in the local-machine
My store and trusts that certificate in the local-machine Trusted Root
Certification Authorities and Trusted Publishers stores. It signs the installed
renderer on the destination PC. Downloads contain unsigned payloads and no
private key, developer certificate, machine settings or Apple fonts.

UIAccess broadens the renderer's access to other applications' UI. Microsoft
reserves this mechanism for accessibility applications and advises against using
it only to appear above modern Windows UI. Its use for cosmetic screen corners
is therefore experimental. No signature/location policy is disabled, and the
application does not inject code into Windows or games or borrow process tokens.
An administrator prompt is still required for protected updates and removal.

The local signing certificate currently expires one year after creation. A
machine-local certificate is not a public publisher identity and does not make
the downloaded installer a trusted publisher on other PCs. The installer itself
is unsigned and Windows may display Unknown Publisher. Only one Windows user
can own the protected installation on a machine; another user's setup is refused
rather than replacing their certificate or runtime.

The elevated worker is `enhanced-admin.ps1`; it checks the installation and
protected ownership markers and records only public certificate metadata. The
private key is never exported. A staged binary is signed and verified before
replacing the protected executable. Installation reports stay in the per-user
folder and are not published.

To return to ordinary rendering, run `enhanced.ps1 Disable` from the installed
corner folder. It requests UAC to remove the protected executable, that exact
local certificate and its private key, and preserves current preferences. The
regular uninstaller performs this cleanup automatically and may request UAC.
Portable corner works without administrator approval; corners above Start
require the installed protected component. Advanced portable users can run
Install.cmd followed by enhanced.ps1 Enable.

Cancelling UAC does not grant UIAccess. Setup reports an incomplete protected
installation and retains ordinary rendering or the previous valid renderer.
Game Mode still works as an ordinary overlay, subject to the target game's
composition path. True exclusive fullscreen and every anti-cheat combination
are not guaranteed.

References:
- https://learn.microsoft.com/windows/win32/winauto/uiauto-securityoverview
- https://github.com/microsoft/PowerToys/issues/14718