# Development

## Build

Install Visual Studio with Desktop development with C++, then run `./build.ps1`
from the repository root. The x64 C++20 release statically links the C++ runtime.
Builds treat warnings as errors and enable/verify Control Flow Guard, with ASLR
and NX enabled.

Use `./build.ps1 -OutputDirectory build/review` when an existing executable may
be running. This produces a separate copy without replacing it.

## Choose verification by change

- Production code: run `./test.ps1` and a release build. Tests link actual modules
  with fake audio/media backends and local WinRT operations; no audio device or
  player is needed, and output/playback settings are not changed. Settings tests
  use isolated folders under `build/tests`.
- Startup script: run `./startup.Tests.ps1` (also included in `./test.ps1`). It
  uses isolated fixtures with mocked registry, desktop-session and shortcut APIs;
  it never edits live startup. Check `./startup.ps1 -Status` separately as the
  desktop user to verify the exact Run command through Windows' independent
  startup inventory. Do not replace a live registration with a worktree build.
  For example, use `-ExecutablePath C:\github\AudioControl\build\AudioControl.exe`
  when intentionally registering that permanent installation. Sign-in execution
  requires a separate manual check; do not sign out during automated checks.
- Keyboard thread or overlay/GDI changes: also run
  `./test.ps1 -WindowsIntegration` on an interactive Windows desktop. This checks
  keyboard-thread startup, independent message handling and shutdown without
  injecting keystrokes, plus overlay cache reuse and GDI cleanup.
- Device discovery or COM compatibility: run the built executable with `--probe`.
  For the default build, results go to `build/AudioControl.ini.probe.txt`. The
  probe is read-only and checks discovery, COM availability and selection logic.
- Real switching, player behavior or Windows support claims: follow the
  [compatibility checklist](compatibility.md). Automated tests and the probe do
  not establish these results.
- Documentation only: check links and commands against their owning files;
  a binary rebuild is unnecessary unless behavior or build inputs also change.

[CI](../.github/workflows/build.yml) runs hardware-independent tests and release
builds on Windows Server 2022 and 2025. These are build checks, not proof of
client Windows audio compatibility.

## Package

Run `./package.ps1 -Version 0.1.0` with the intended release version. It runs
automated tests, builds separately under `build/release`, and writes a ZIP and
SHA-256 checksum under `build/releases`. Only the executable, README, MIT license
and startup script are included. The README uses repository URLs for developer
documentation so its links also work in the extracted release.

## Documentation ownership

[README](../README.md) explains the product and its use;
[architecture](architecture.md) records module ownership and cross-module
contracts; [compatibility](compatibility.md) owns manual verification procedures.
Keep implementation details and rationale close to the relevant source.

Record new compatibility evidence or measurements in a dated file under
`docs/verification/`, including environment, commands, results and limitations.
Update the compatibility table when support evidence changes. Archived results
are snapshots, not a checklist or a claim that the current checkout was tested.
