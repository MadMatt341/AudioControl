# AudioControl structure

| Module | Responsibility |
| --- | --- |
| `AudioControl.cpp` | Startup, tray menu, window messages, exception boundary, shutdown |
| `AudioDevices.cpp` | Endpoint discovery, selection and default-output changes |
| `PolicyConfig.h` | The private Windows ABI; no UI or application logic |
| `MediaControl.cpp`, `WinrtOperation.h` | On-demand media requests, bounded waits and cancellation |
| `Shortcuts.cpp` | Shortcut state and the dedicated keyboard message thread |
| `Overlay.cpp`, `OverlayLayout.h` | Bounded popup layout, reusable background and GDI rendering |
| `Settings.cpp` | Settings location, one-time migration and shortcut validation |
| `AudioLogic.h` | Deterministic selection/failure-recovery policies shared with tests |
| `NativeResources.h` | Scope-based ownership of native resources |

The UI thread owns the application window, configuration use and overlay cache.
Keyboard state belongs to the hook thread after startup. Its only cross-thread
actions are posting messages. Media request data is immutable while its worker
runs; the stop flag is atomic, and results carry a generation number.

Shutdown stops the hook thread before destroying UI resources. Media cancellation
is cooperative: asynchronous waits observe a three-second deadline and shutdown,
but Windows COM calls and cancellation cannot be forcibly interrupted safely.

One cached overlay frame retains a DIB and pristine background pixels. A change
in frame size or DPI replaces it. Text is redrawn from the pristine background,
so repeated messages cannot leave stale glyphs. Layout caps both screen extent
and allocation size; cache memory trades a small retained allocation for avoiding
repeated shadow calculations. The topmost timer runs only while visible.

Window-callback exceptions are contained and reported. COM device strings,
property variants, GDI selections, fonts, bitmaps, menus, handles and DCs have
scope-based cleanup. Shared Windows icons/cursors are not owned by the app.

`test.ps1` compiles the actual modules with fake audio/media backends; it does
not include a production `.cpp` file. Optional desktop tests exercise real GDI,
cache reuse/resource cleanup and keyboard-thread lifecycle. The GitHub workflow
runs hardware-independent tests and release builds on Windows Server runners;
these are build checks, not proof of client Windows audio compatibility.
