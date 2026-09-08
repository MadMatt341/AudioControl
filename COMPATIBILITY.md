# Windows compatibility

The application targets x64 Windows 10 version 1809 or later and Windows 11.
Media session control sets the minimum version. Default-output switching depends
on the private PolicyConfig ABI, so OS-version claims require interactive checks.

| Environment | Automated checks | Device probe | Actual switching/player control |
| --- | --- | --- | --- |
| Local Windows 11 x64, build 26200 | 32 checks passed | Passed, four endpoints | Manual validation required after OS/driver changes |
| Windows 10 1809–22H2 | Not run in this workspace | Not run | Not run |
| Other Windows 11 builds | Not run in this workspace | Not run | Not run |
| GitHub Windows Server 2022 / 2025 runners | Workflow configured | Not required | Not supported as audio compatibility evidence |

Before claiming a new Windows build as verified:

1. Run `test.ps1 -WindowsIntegration` on an interactive desktop.
2. Run `AudioControl.exe --probe` and record the OS build, compiler version,
   active endpoints, result and date in VERIFICATION.md.
3. Switch between two real outputs and confirm both console and multimedia
   roles change, while communications and microphones remain unchanged.
4. Test one included output with previously mismatched defaults, disconnection
   during switching, play/pause, AltGr typing and Explorer restart.
5. Check a long device name, multiple DPI settings and competing topmost windows.
6. Measure memory and CPU after the popup hides; distinguish private bytes from
   working set. Repeat after media use because Windows loads additional modules.

The probe instantiates PolicyConfig and reads endpoints but does not call its
setter. A passing probe alone is not a successful switching test.

References: [media session API](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessionmanager),
[private ABI reference](https://github.com/tartakynov/audioswitch/blob/master/IPolicyConfig.h),
[Windows runner images](https://github.com/actions/runner-images).
