#include "App.h"
#include "PolicyConfig.h"
#include "Settings.h"
#include <stdexcept>
std::wstring config;
HWND window = nullptr;
UINT taskbarMessage = 0;
void StopWorkers() {
    StopKeyboard();
    StopMedia();
}
void Tray(DWORD action) {
    NOTIFYICONDATA n{sizeof(n)};
    n.hWnd = window;
    n.uID = 1;
    n.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    n.uCallbackMessage = TrayMessage;
    n.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(n.szTip, L"AudioControl - cycle audio output");
    if (!Shell_NotifyIcon(action, &n) && action == NIM_ADD)
        MessageBox(
            window,
            L"Could not add the tray icon. Shortcuts still work; restart AudioControl if the icon does not return.",
            L"AudioControl", MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);
}
void Menu() {
    MenuOwner menu(CreatePopupMenu());
    if (!menu)
        throw HRESULT(E_OUTOFMEMORY);
    AppendMenu(menu, MF_STRING, 1, L"Next output");
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(menu, MF_STRING | MF_DISABLED, 0, L"Include in cycle:");
    std::vector<Device> ds;
    unsigned skipped = 0;
    try {
        auto e = Enumerator();
        ds = Devices(e.Get(), &skipped);
    } catch (HRESULT hr) {
        wchar_t error[128];
        swprintf_s(error, L"Cannot read audio outputs (0x%08X); try again", static_cast<unsigned>(hr));
        AppendMenu(menu, MF_STRING | MF_DISABLED, 0, error);
    }
    if (skipped)
        AppendMenu(menu, MF_STRING | MF_DISABLED, 0, L"Some outputs became unavailable; reopen to refresh");
    for (size_t i = 0; i < ds.size(); ++i) {
        std::wstring label = ds[i].name;
        for (size_t p = 0; (p = label.find(L'&', p)) != std::wstring::npos; p += 2)
            label.insert(p, 1, L'&');
        AppendMenu(menu, MF_STRING | (Included(ds[i]) ? MF_CHECKED : 0), 100 + i, label.c_str());
    }
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(menu, MF_STRING, 2, L"Exit");
    POINT p;
    GetCursorPos(&p);
    SetForegroundWindow(window);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, window, nullptr);
    menu.reset();
    PostMessage(window, WM_NULL, 0, 0);
    if (cmd == 1)
        Cycle();
    else if (cmd == 2)
        DestroyWindow(window);
    else if (cmd >= 100 && cmd - 100 < ds.size()) {
        auto &d = ds[cmd - 100];
        if (!WritePrivateProfileString(L"Devices", d.id.c_str(), Included(d) ? L"0" : L"1", config.c_str()))
            Show(L"Unable to save device selection");
    }
}
LRESULT Dispatch(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (taskbarMessage && msg == taskbarMessage) {
        Tray(NIM_ADD);
        return 0;
    }
    switch (msg) {
    case MediaMessage: {
        BeginMedia(hwnd);
        return 0;
    }
    case MediaResultMessage: {
        if (!CompleteMedia(lp))
            return 0;
        if (wp == 1)
            Show(L"Pause", L"MEDIA CONTROL");
        else if (wp == 2)
            Show(L"Play", L"MEDIA CONTROL");
        else if (wp == 3)
            Show(L"Player did not accept the command", L"MEDIA CONTROL");
        else if (wp == 4)
            Show(L"Media request timed out; press again to retry", L"MEDIA CONTROL");
        else if (wp == 5)
            return 0;
        else {
            DefWindowProc(hwnd, WM_APPCOMMAND, reinterpret_cast<WPARAM>(hwnd),
                          MAKELPARAM(0, FAPPCOMMAND_KEY | APPCOMMAND_MEDIA_PLAY_PAUSE));
            Show(L"Media command sent", L"Playback state unavailable");
        }
        return 0;
    }
    case CycleMessage:
    case WM_HOTKEY:
        Cycle();
        return 0;
    case TrayMessage:
        if (lp == WM_RBUTTONUP)
            Menu();
        if (lp == WM_LBUTTONUP)
            Cycle();
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_DPICHANGED:
        return 0; // Show lays out using the destination DPI.
    case WM_TIMER:
        if (wp == OverlayHideTimer) {
            KillTimer(hwnd, OverlayHideTimer);
            KillTimer(hwnd, OverlayTopmostTimer);
            ShowWindow(hwnd, SW_HIDE);
        } else if (wp == OverlayTopmostTimer && IsWindowVisible(hwnd))
            RaiseOverlay(hwnd);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        StopWorkers();
        UnregisterHotKey(hwnd, 1);
        KillTimer(hwnd, OverlayHideTimer);
        KillTimer(hwnd, OverlayTopmostTimer);
        Tray(NIM_DELETE);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
void ReportError(const wchar_t *message) noexcept {
    MessageBox(window, message, L"AudioControl", MB_OK | MB_ICONERROR);
}
LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) noexcept {
    try {
        return Dispatch(hwnd, msg, wp, lp);
    } catch (HRESULT hr) {
        wchar_t error[128];
        swprintf_s(error, L"Windows operation failed (0x%08X). Please try again.", static_cast<unsigned>(hr));
        ReportError(error);
    } catch (...) {
        ReportError(L"AudioControl could not complete the operation. Please try again.");
    }
    return 0;
}
int RunApplication(HINSTANCE instance) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Check(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    ScopeExit uninitialize([] { CoUninitialize(); });
    int argumentCount = 0;
    auto arguments = CommandLineToArgvW(GetCommandLine(), &argumentCount);
    if (!arguments)
        throw HRESULT(E_OUTOFMEMORY);
    ScopeExit freeArguments([&] { LocalFree(arguments); });
    bool probe = false, portable = false;
    for (int i = 1; i < argumentCount; ++i) {
        if (wcscmp(arguments[i], L"--probe") == 0)
            probe = true;
        else if (wcscmp(arguments[i], L"--portable") == 0)
            portable = true;
        else
            throw std::runtime_error("Unknown argument. Supported options: --probe, --portable");
    }
    // Probe output stays beside the tested executable; no user-settings migration.
    if (probe)
        config = (ExecutablePath().parent_path() / L"AudioControl.ini").wstring();
    if (probe) {
        int result = 0;
        FILE *file = nullptr;
        _wfopen_s(&file, (config + L".probe.txt").c_str(), L"w, ccs=UTF-8");
        if (!file) {
            return 2;
        }
        ScopeExit closeFile([&] { fclose(file); });
        OSVERSIONINFOW version{sizeof(version)};
#pragma warning(suppress : 4996)
        GetVersionExW(&version);
        fwprintf(file, L"Windows %lu.%lu build %lu; x64; diagnostics do not change defaults.\n", version.dwMajorVersion,
                 version.dwMinorVersion, version.dwBuildNumber);
        try {
            auto e = Enumerator();
            auto ds = Devices(e.Get());
            auto current = Current(e.Get(), eMultimedia);
            ComPtr<Policy> policy;
            Check(CoCreateInstance(PolicyClass, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&policy)));
            fwprintf(file, L"Policy interface available. %zu active outputs.\n", ds.size());
            for (auto &d : ds)
                fwprintf(file, L"%s %s\n", d.id == current ? L"*" : L"-", d.name.c_str());
            std::vector<Device> sample{{L"a", L"A"}, {L"b", L"B"}, {L"c", L"C"}};
            if (Next(sample, L"a") != 1 || Next(sample, L"c") != 0 || Next(sample, L"missing") != 0 ||
                Next({sample[0]}, L"a") != 0)
                result = 3;
            fwprintf(file, L"Cycle selection checks: %s\n", result ? L"FAIL" : L"PASS");
            bool shortcutsPassed = ShortcutChecks();
            fwprintf(file, L"Left Alt / AltGr / repeat checks: %s\n", shortcutsPassed ? L"PASS" : L"FAIL");
            if (!shortcutsPassed)
                result = 4;
        } catch (HRESULT hr) {
            fwprintf(file, L"Error: 0x%08X\n", static_cast<unsigned>(hr));
            result = 1;
        }
        return result;
    }

    KernelHandle singleton(CreateMutex(nullptr, TRUE, L"Local\\AudioControl.Native.Cycle"));
    if (!singleton)
        throw HRESULT(E_FAIL);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;
    config = InitializeSettings(portable).wstring();
    ScopeExit releaseFonts([] { ReleaseOverlay(); });
    taskbarMessage = RegisterWindowMessage(L"TaskbarCreated");
    WNDCLASS wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = Proc;
    wc.lpszClassName = L"AudioControlOverlay";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClass(&wc))
        throw HRESULT(E_FAIL);
    window = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, wc.lpszClassName,
                            L"AudioControl", WS_POPUP, 0, 0, 460, 76, nullptr, nullptr, instance, nullptr);
    if (!window)
        throw HRESULT(E_FAIL);
    ScopeExit cleanupWindow([] {
        if (IsWindow(window))
            DestroyWindow(window);
        window = nullptr;
    });
    UpdateFonts();
    auto hotkey = ReadHotkeySettings(config);
    ConfigureKeyboard(hotkey.leftAltA);
    bool registered = StartKeyboard(instance);
    if (registered && !hotkey.leftAltA)
        registered = RegisterHotKey(window, 1, hotkey.modifiers | MOD_NOREPEAT, hotkey.key) != 0;
    if (!registered)
        throw std::runtime_error(
            "Could not register keyboard shortcuts. Check whether another app uses the configured key.");
    Tray(NIM_ADD);
    Show(L"AudioControl ready");
    MSG msg{};
    int status;
    while ((status = GetMessage(&msg, nullptr, 0, 0)) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return status < 0 ? 1 : 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    try {
        return RunApplication(instance);
    } catch (const std::exception &error) {
        std::string message = error.what();
        std::wstring wide(message.begin(), message.end());
        wide += L"\nSettings: " + config;
        ReportError(wide.c_str());
    } catch (...) {
        ReportError(L"AudioControl could not start. Check your settings and Windows audio service.");
    }
    return 1;
}
