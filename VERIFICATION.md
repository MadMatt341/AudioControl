# Local verification — 2026-09-08

## Maintainability, performance and hardening improvements

- Split the application into focused modules; tests now link production modules
  instead of including a `.cpp`. Added architecture and compatibility documents.
- `test.ps1 -WindowsIntegration`: 32 checks passed with `/W4 /WX`. New coverage:
  endpoint failure isolation, cleanup on exception, bounded popup/text, invalid
  hotkeys, non-overwriting configuration migration, paths longer than 260
  characters, actual GDI cache reuse and resource cleanup.
- Render measurement for a 500 x 150 frame: 3.3083 ms first render versus
  0.187405 ms average over 20 cached renders. This local sample is not a latency
  guarantee. The cache retains roughly eight bytes per pixel.
- GDI resource count returned to baseline after rendering and releasing the
  cache/fonts, after warming Windows' process-wide stock objects.
- Release build passed; the build script checks the produced PE header for
  Control Flow Guard. Compilation and linking both enable CFG; ASLR and NX are
  explicitly enabled. Manifest requests ordinary user privileges and long paths.
- Rebuilt probe passed on Windows 11 x64 build 26200, enumerated four endpoints,
  instantiated PolicyConfig and passed the selection/shortcut checks.
- Restarted the updated installed executable. Verified its window belongs to
  the new process and verified the migrated LocalAppData INI exactly matches the
  prior executable-adjacent file by SHA-256. The legacy file remains intact.
- Fresh process after popup dismissal: 3.43 MiB private bytes, 20.41 MiB working
  set, and 0 ms added CPU time during a five-second idle sample. These readings
  precede media use and are machine-specific, not guaranteed limits.
- GitHub workflow configured for Windows Server 2022 and 2025 builds/tests. It
  has not run remotely from this workspace. Real switching/player behavior on
  additional client Windows builds still requires the COMPATIBILITY.md checklist.

## Reliability fixes

- Release source rebuilt with `/W4 /WX`; no warnings or errors. The review binary
  is in `build/review/AudioControl.exe`; the running installation was not replaced.
- `./test.ps1 -WindowsIntegration`: 24 checks passed. Coverage includes shortcut
  behavior; selection; both-role switching; mismatched defaults; failed writes,
  verification and rollback; missing prior endpoints; media rejection, timeout,
  cancellation, retry and stale completions; actual WinRT delayed completion;
  and independent keyboard-thread message handling and shutdown.
- Tests use fake audio/media backends and local asynchronous operations. The
  optional hook-thread check does not inject keys or change audio outputs.
- The rebuilt `--probe` passed, enumerated four active outputs and instantiated
  PolicyConfig. Actual device switching, player control, physical AltGr typing,
  and disconnect races still need interactive verification.
- Media async waits have a shared three-second deadline, checked at 50 ms
  intervals while a request is pending. There is no idle polling. Cancellation
  cannot undo a player command already accepted. Synchronous external COM calls
  are not forcibly interrupted; shutdown waits at most one second for media
  cleanup before process exit.

## Left Alt+A update

- Rebuilt with no compiler warnings and restarted the utility.
- Shortcut state checks passed for Left Alt+A, held-key repeat suppression,
  release after Alt, plain A, Right Alt+A with and without AltGr's Ctrl,
  both Alt keys held, and Shift+Left Alt+A.
- Right Alt combinations pass through without registering a generic Alt+A hotkey.
- Physical Polish-layout typing still needs a live user check.

## Initial build measurements

- MSVC x64 optimized release build passed with `/W4` and no warnings.
- Executable: 200,704 bytes (196 KiB).
- Read-only probe successfully enumerated four active playback endpoints,
  read the current multimedia default and instantiated the PolicyConfig interface.
- Cycle selection checks passed: next item, wraparound, absent current device,
  and a single-device list.
- Running tray process after the startup overlay dismissed: 1.93 MiB private
  committed memory, 12.54 MiB working set (includes shared DLL pages).
- CPU time increased by 0 ms during a five-second idle sample.

These are measurements on this machine, not guaranteed limits. Actual hotkey
switching, post-switch memory, disconnect races, and fullscreen overlay behavior
have not yet been exercised. The probe does not change the audio output.
