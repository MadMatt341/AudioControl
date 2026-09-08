#include "App.h"
HHOOK keyboardHook;
KernelHandle keyboardThread, keyboardReady;
DWORD keyboardThreadId = 0;
struct ShortcutState {
    bool down[256]{};
    bool consumed[256]{};
    bool cycleEnabled = true;
    // 0 = pass through, 1 = consume, 2 = cycle, 3 = media play/pause.
    int Handle(DWORD key, bool pressed) {
        if (key >= 256)
            return 0;
        bool repeat = down[key];
        down[key] = pressed;
        if (key != 'S' && (key != 'A' || !cycleEnabled))
            return 0;
        if (!pressed) {
            bool wasConsumed = consumed[key];
            consumed[key] = false;
            return wasConsumed ? 1 : 0;
        }
        if (consumed[key])
            return 1;
        if (!repeat && down[VK_LMENU] && !down[VK_RMENU] && !down[VK_LCONTROL] && !down[VK_RCONTROL] &&
            !down[VK_LSHIFT] && !down[VK_RSHIFT] && !down[VK_LWIN] && !down[VK_RWIN]) {
            consumed[key] = true;
            return key == 'A' ? 2 : 3;
        }
        return 0;
    }
} shortcut;
LRESULT CALLBACK KeyboardProc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION) {
        const auto &key = *reinterpret_cast<const KBDLLHOOKSTRUCT *>(lp);
        bool pressed = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
        int action = shortcut.Handle(key.vkCode, pressed);
        // Never perform COM or drawing work inside the keyboard hook.
        if (action == 2)
            PostMessage(window, CycleMessage, 0, 0);
        if (action == 3)
            PostMessage(window, MediaMessage, 0, 0);
        if (action)
            return 1;
    }
    return CallNextHookEx(keyboardHook, code, wp, lp);
}
bool ShortcutChecks() {
    ShortcutState s;
    s.Handle(VK_LMENU, true);
    if (s.Handle('S', true) != 3 || s.Handle('S', true) != 1 || s.Handle('A', true) != 2 || s.Handle('S', false) != 1 ||
        s.Handle('A', false) != 1)
        return false;
    s = {};
    s.Handle(VK_LCONTROL, true);
    s.Handle(VK_RMENU, true);
    if (s.Handle('S', true) != 0 || s.Handle('S', false) != 0)
        return false;
    s = {};
    s.Handle(VK_RMENU, true);
    if (s.Handle('S', true) != 0)
        return false;
    s = {};
    s.cycleEnabled = false;
    s.Handle(VK_LMENU, true);
    if (s.Handle('A', true) != 0 || s.Handle('S', true) != 3)
        return false;
    s = {};
    s.Handle(VK_LMENU, true);
    if (s.Handle('A', true) != 2 || s.Handle('A', true) != 1)
        return false;
    s.Handle(VK_LMENU, false);
    if (s.Handle('A', false) != 1 || s.Handle('A', true) != 0)
        return false;
    s = {};
    s.Handle(VK_LCONTROL, true);
    s.Handle(VK_RMENU, true);
    if (s.Handle('A', true) != 0 || s.Handle('A', false) != 0)
        return false;
    s = {};
    s.Handle(VK_RMENU, true);
    if (s.Handle('A', true) != 0)
        return false;
    s = {};
    s.Handle(VK_LMENU, true);
    s.Handle(VK_RMENU, true);
    if (s.Handle('A', true) != 0)
        return false;
    s = {};
    s.Handle(VK_LMENU, true);
    s.Handle(VK_LSHIFT, true);
    return s.Handle('A', true) == 0;
}
DWORD WINAPI KeyboardWorker(void *context) {
    // Only this thread owns the hook and shortcut state after startup.
    MSG msg{};
    PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE);
    for (int k = 0; k < 256; ++k)
        shortcut.down[k] = (GetAsyncKeyState(k) & 0x8000) != 0;
    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, static_cast<HINSTANCE>(context), 0);
    SetEvent(keyboardReady);
    if (!keyboardHook)
        return 1;
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnhookWindowsHookEx(keyboardHook);
    return 0;
}
bool StartKeyboard(HINSTANCE instance) {
    keyboardReady = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!keyboardReady)
        return false;
    keyboardThread = CreateThread(nullptr, 0, KeyboardWorker, instance, 0, &keyboardThreadId);
    if (keyboardThread)
        WaitForSingleObject(keyboardReady, INFINITE);
    keyboardReady.reset();
    return keyboardThread && keyboardHook;
}

void StopKeyboard() {
    if (keyboardThread) {
        PostThreadMessage(keyboardThreadId, WM_QUIT, 0, 0);
        WaitForSingleObject(keyboardThread, INFINITE);
        keyboardThread.reset();
    }
}
void ConfigureKeyboard(bool enabled) {
    shortcut.cycleEnabled = enabled;
}
