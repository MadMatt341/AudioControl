# AudioControl

A native Windows 10/11 audio-output cycling utility. No managed runtime, browser,
audio processing, network traffic, or idle timers.

Open source under the [MIT license](LICENSE). This is a small personal utility;
bug reports and contributions are welcome, but support has no guaranteed timeline.

## Use

Run `build/AudioControl.exe`.

- **Left Alt+A** — switch audio output.
- **Left Alt+S** — play/pause media.
- **Tray icon** — left-click to switch; right-click to choose outputs or quit.

A brief popup confirms each action. Right Alt (AltGr) works normally.
The popup says **Output N**, with matching output numbers in the tray
menu, so identically named monitor speakers are distinguishable. Numbers follow
the connected audio-output list (not Windows display numbers) and can change
when devices are connected, disconnected, or renamed. Excluding an output from
the cycle does not renumber the list.

Run `./startup.ps1` to launch at sign-in, or `./startup.ps1 -Disable` to undo.
Settings: `%LOCALAPPDATA%\AudioControl\AudioControl.ini`. Use `--portable` to
store them beside the executable instead.

Microphones and the communications default stay unchanged. Exclusive fullscreen
may hide the popup.

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
