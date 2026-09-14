# AudioControl

A native Windows 10/11 audio-output cycling utility. No managed runtime, browser,
audio processing, network traffic, or idle timers.

Open source under the [MIT license](LICENSE). This is a small personal utility;
bug reports and contributions are welcome, but support has no guaranteed timeline.

## Use

Download the Windows x64 ZIP from [GitHub Releases](https://github.com/MadMatt341/AudioControl/releases)
and extract the complete folder to a permanent location. No installer is needed.

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

To update, quit from the tray menu and extract the new release to the same
location. To remove, run `./startup.ps1 -Disable`, quit, and delete the extracted
folder. You can optionally delete `%LOCALAPPDATA%\AudioControl` to remove settings.

Release binaries are unsigned and Windows may show a security warning. Each
release includes a SHA-256 checksum for the ZIP.

Microphones and the communications default stay unchanged. Exclusive fullscreen
may hide the popup.

## Development

Install Visual Studio with Desktop development with C++, then run `./build.ps1`.
Run `./test.ps1` for hardware-independent regression tests.

See the [development guide](https://github.com/MadMatt341/AudioControl/blob/main/docs/development.md)
for build, verification and packaging details. Repository automation starts with
[AGENTS.md](https://github.com/MadMatt341/AudioControl/blob/main/AGENTS.md).
