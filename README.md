# AudioControl

A native Windows 10/11 audio-output cycling utility. No managed runtime, browser,
audio processing, network traffic, or idle timers.

## Use

Press **Left Alt+S** to play/pause Windows' current media session without finding
the player window. The overlay says **Pause** or **Play** after the player accepts
the corresponding command. Playback state is queried on demand, so changes made
in Spotify or the browser are respected. Right Alt+S stays available for Polish ś.
Holding the shortcut sends only one command. Players that do not expose their
state receive the standard media-key fallback with a neutral confirmation.

Run `build/AudioControl.exe`. Press **Left Alt+A** to cycle connected playback
outputs alphabetically. A brief overlay confirms the output without taking focus.
Disconnected devices are skipped. Hold the shortcut without repeated switches.

Left-click the tray icon to cycle. Right-click to exclude outputs from the cycle
or exit. Selections persist in `%LOCALAPPDATA%\AudioControl\AudioControl.ini`.
On first launch, an existing INI beside the executable is copied there without
overwriting existing user settings. Edit the Hotkey section and restart to
change keys; invalid settings produce an error explaining what to correct.
Use `--portable` to keep settings beside the executable instead (requires a
writable folder). Long executable and settings paths are supported.
Running a second instance exits harmlessly. Run `./startup.ps1` to launch automatically
at Windows sign-in for your account; `./startup.ps1 -Disable` removes it. The startup
shortcut points to `build/AudioControl.exe`, so keep this folder in place or rerun
the script after moving it. Add `-Portable` to `startup.ps1` to persist portable mode.
Right Alt+A (AltGr) passes through for Polish characters. LeftAltA=1 uses a
side-specific keyboard hook; its callback only tracks key state and posts the
cycle action. Set LeftAltA=0 to use the generic Modifiers/Key settings instead.

Changes the console and multimedia default output; leaves the communications
default and microphones unchanged. Apps pinned to a specific output may not move.
The overlay is intended for the desktop/borderless games; exclusive fullscreen
can hide it. Windows remains responsible for Bluetooth connection/switch latency.
While visible, the overlay reasserts its topmost position every 100 ms without
taking focus, so other floating tool windows do not leave it obscured. This
timer stops when the overlay hides.

## Build and check

Install Visual Studio with Desktop development with C++, then run `./build.ps1`.
The x64 release statically links the C++ runtime and needs no separate runtime install.
Builds treat warnings as errors and enable/verify Control Flow Guard, with ASLR
and NX enabled. `./build.ps1 -OutputDirectory build/review` builds a separate
copy without replacing a running executable. CI uses the same scripts on two
Windows runner images; see `.github/workflows/build.yml`.
Run `build/AudioControl.exe --probe` for read-only device/COM diagnostics and cycle
selection checks. Results go to `build/AudioControl.ini.probe.txt`.

Run `./test.ps1` for automated regression tests. These use fake audio/media
backends and local WinRT operations; no audio device or media player is needed,
and no output or playback settings are changed. `./test.ps1 -WindowsIntegration`
also checks keyboard-thread startup, independent message handling, and shutdown
on an interactive Windows desktop, without injecting keystrokes. It also checks
overlay cache reuse and GDI cleanup. Settings tests use isolated folders under
`build/tests`; they do not edit your real settings.

The keyboard hook runs on a dedicated message-loop thread, separate from audio
switching and overlay rendering. Media async requests share a three-second
deadline, cancel on timeout/shutdown, and permit a fresh request after timeout.
A timed-out command is never automatically retried or replaced by a toggle,
because the player might already have acted. Cancellation is best effort.
Both console and multimedia outputs are verified after a switch. Failed switches
attempt to restore both previous outputs and report if restoration is incomplete.

The private Windows PolicyConfig COM interface is used for switching because the
documented MMDevice API exposes discovery, but no default-endpoint setter. Its ABI
is isolated in PolicyConfig.h; compatibility must be checked on future
Windows versions. Reference: https://github.com/tartakynov/audioswitch/blob/master/IPolicyConfig.h

Memory numbers should distinguish private committed memory from working set
(which also includes shared Windows DLL pages). Measure after the overlay hides.
The overlay caches one background by size and DPI, bounds long text and popup
height, and reuses its bitmap/DC. The retained cache uses about eight bytes per
pixel (pristine background plus working bitmap).

See [ARCHITECTURE.md](ARCHITECTURE.md) for module ownership and
[COMPATIBILITY.md](COMPATIBILITY.md) for the Windows verification checklist.
