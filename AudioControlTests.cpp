// Console tests exercise production logic without changing audio or playback.
#include "App.h"
#include "WinrtOperation.h"
#include "Settings.h"
#include "OverlayLayout.h"
#include <fstream>
#include <iostream>
#include <functional>
#include <stdexcept>

int failures = 0;
void Test(const char *name, const std::function<bool()> &test) {
    bool passed = false;
    try {
        passed = test();
    } catch (const std::exception &error) {
        std::cout << "  " << error.what() << '\n';
    } catch (HRESULT hr) {
        std::cout << "  HRESULT " << std::hex << hr << std::dec << '\n';
    } catch (...) {
    }
    std::cout << (passed ? "PASS " : "FAIL ") << name << '\n';
    if (!passed)
        ++failures;
}
struct FakeAudio {
    std::array<std::wstring, 2> ids{L"speakers", L"speakers"};
    int writes = 0;
    std::function<void(int, const std::wstring &)> onWrite;
    std::wstring Read(int role) {
        return ids[role];
    }
    void Write(int role, const std::wstring &id) {
        ++writes;
        if (onWrite)
            onWrite(role, id);
        ids[role] = id;
    }
};
struct FakeOperation {
    std::uint64_t time = 0, completesAt = 100;
    bool cancelled = false, cancelThrows = false;
    bool Poll() {
        time += 50;
        return time >= completesAt;
    }
    bool Result() {
        return true;
    }
    void Cancel() {
        cancelled = true;
        if (cancelThrows)
            throw 1;
    }
};
struct FakeMedia {
    int state = 1, calls = 0;
    bool accepted = true, fails = false, timeout = false, pause = false;
    int Discover() {
        return state;
    }
    bool Command(bool value) {
        ++calls;
        pause = value;
        if (timeout)
            throw audio::MediaResult::TimedOut;
        if (fails)
            throw 1;
        return accepted;
    }
};
winrt::Windows::Foundation::IAsyncOperation<bool> DelayedResult() {
    co_await winrt::resume_after(std::chrono::milliseconds(160));
    co_return true;
}
int main(int argc, char **) {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    using namespace audio;
    Test("shortcut repeat, release ordering and AltGr", [] { return ShortcutChecks(); });
    Test("cycle wrap, missing current, single and empty lists", [] {
        std::vector<Device> ds{{L"a", L"A"}, {L"b", L"B"}};
        return Next(ds, L"a") == 1 && Next(ds, L"b") == 0 && Next(ds, L"missing") == 0 && Next({ds[0]}, L"a") == 0 &&
               Next({}, L"") == 0;
    });
    Test("disappearing endpoint does not discard healthy endpoints", [] {
        unsigned skipped = 0;
        auto devices = CollectAvailable<int, HRESULT>(
            3,
            [](unsigned i) {
                if (i == 1)
                    throw HRESULT(E_FAIL);
                return static_cast<int>(i);
            },
            skipped);
        return devices == std::vector<int>{0, 2} && skipped == 1;
    });
    Test("resource cleanup executes on exception", [] {
        int released = 0;
        try {
            ScopeExit cleanup([&] { ++released; });
            throw 1;
        } catch (...) {
        }
        return released == 1;
    });
    Test("oversized overlay fits small work area", [] {
        auto size = BoundOverlay(10000, 10000, 640, 480, 40);
        return size.width + 80 <= 640 && size.height + 80 <= 480 && size.width > 0 && size.height > 0;
    });
    Test("text truncation preserves UTF16 surrogate pairs", [] {
        std::wstring text(2046, L'a');
        text += L"\xD83D\xDE00";
        text += std::wstring(100, L'b');
        auto shortened = BoundedOverlayText(text);
        return shortened.size() <= 2048 && shortened.back() == L'\u2026' && shortened[shortened.size() - 2] == L'a';
    });
    Test("decimal configuration rejects negative, malformed and overflowing values", [] {
        for (const auto &value : {L"-1", L"12x", L"9999999999999999999999", L""}) {
            try {
                ParseSetting(value, 255);
                return false;
            } catch (const std::runtime_error &) {
            }
        }
        return ParseSetting(L"122", 254) == 122;
    });
    Test("configuration migration preserves values and never overwrites user settings", [] {
        auto root = std::filesystem::absolute(L"build/tests/settings-migration");
        std::filesystem::create_directories(root);
        auto legacy = root / L"legacy.ini";
        auto userDir = root / (L"user-" + std::to_wstring(GetTickCount64()));
        WritePrivateProfileString(L"Devices", L"test-endpoint", L"0", legacy.c_str());
        auto path = PrepareSettings(userDir, legacy);
        if (GetPrivateProfileInt(L"Devices", L"test-endpoint", 1, path.c_str()) != 0)
            return false;
        WritePrivateProfileString(L"Devices", L"test-endpoint", L"1", path.c_str());
        PrepareSettings(userDir, legacy);
        return GetPrivateProfileInt(L"Devices", L"test-endpoint", 0, path.c_str()) == 1;
    });
    Test("long settings paths and invalid shortcut combinations", [] {
        auto root = std::filesystem::absolute(L"build/tests/long-path") / std::to_wstring(GetTickCount64());
        for (int i = 0; i < 5; ++i)
            root /= std::wstring(55, L'x');
        auto path = PrepareSettings(root, root / L"missing.ini");
        if (path.wstring().size() <= 260 || !ReadHotkeySettings(path).leftAltA)
            return false;
        WritePrivateProfileString(L"Hotkey", L"LeftAltA", L"0", path.c_str());
        WritePrivateProfileString(L"Hotkey", L"Modifiers", L"1", path.c_str());
        WritePrivateProfileString(L"Hotkey", L"Key", L"83", path.c_str());
        try {
            ReadHotkeySettings(path);
        } catch (const std::runtime_error &) {
            return true;
        }
        return false;
    });
    Test("switch verifies both roles", [] {
        FakeAudio b;
        return SwitchOutput(b, L"headphones") == SwitchResult::Success && b.ids[0] == L"headphones" &&
               b.ids[1] == L"headphones";
    });
    Test("single selected multimedia output repairs console", [] {
        FakeAudio b;
        b.ids[1] = L"headphones";
        return SwitchOutput(b, L"headphones") == SwitchResult::Success && b.ids[0] == L"headphones" && b.writes == 1;
    });
    Test("already consistent performs no writes", [] {
        FakeAudio b;
        return SwitchOutput(b, L"speakers") == SwitchResult::Success && b.writes == 0;
    });
    Test("second write failure restores first role", [] {
        FakeAudio b;
        b.onWrite = [](int role, const auto &id) {
            if (role == 1 && id == L"headphones")
                throw 1;
        };
        return SwitchOutput(b, L"headphones") == SwitchResult::Restored && b.ids[0] == L"speakers";
    });
    Test("write applies then throws; both roles restored", [] {
        FakeAudio b;
        b.onWrite = [&](int role, const auto &id) {
            if (role == 1 && id == L"headphones") {
                b.ids[role] = id;
                throw 1;
            }
        };
        return SwitchOutput(b, L"headphones") == SwitchResult::Restored && b.ids[1] == L"speakers";
    });
    Test("rollback failure reports partial switch", [] {
        FakeAudio b;
        b.onWrite = [](int role, const auto &id) {
            if (role == 1 || id == L"speakers")
                throw 1;
        };
        return SwitchOutput(b, L"headphones") == SwitchResult::Partial;
    });
    Test("verification mismatch triggers restoration", [] {
        FakeAudio b;
        b.onWrite = [&](int role, const auto &id) {
            if (role == 1 && id == L"headphones")
                b.ids[0] = L"other";
        };
        return SwitchOutput(b, L"headphones") == SwitchResult::Restored;
    });
    Test("missing previous endpoint cannot be falsely restored", [] {
        FakeAudio b;
        b.ids[0] = L"";
        b.onWrite = [](int role, const auto &) {
            if (role == 1)
                throw 1;
        };
        return SwitchOutput(b, L"headphones") == SwitchResult::Partial;
    });
    Test("snapshot failure performs no writes", [] {
        struct Backend {
            int writes = 0;
            std::wstring Read(int role) {
                if (role == 1)
                    throw 1;
                return L"speakers";
            }
            void Write(int, const std::wstring &) {
                ++writes;
            }
        } b;
        try {
            SwitchOutput(b, L"headphones");
        } catch (...) {
            return b.writes == 0;
        }
        return false;
    });
    Test("media success chooses pause and play", [] {
        FakeMedia b;
        if (ControlMedia(b) != MediaResult::Paused || !b.pause)
            return false;
        b.state = 2;
        return ControlMedia(b) == MediaResult::Playing && !b.pause;
    });
    Test("unknown session uses fallback without command", [] {
        FakeMedia b;
        b.state = 0;
        return ControlMedia(b) == MediaResult::Fallback && b.calls == 0;
    });
    Test("rejected and throwing commands never fall back", [] {
        FakeMedia b;
        b.accepted = false;
        if (ControlMedia(b) != MediaResult::Rejected)
            return false;
        b.fails = true;
        return ControlMedia(b) == MediaResult::Rejected && b.calls == 2;
    });
    Test("command timeout never issues fallback", [] {
        FakeMedia b;
        b.timeout = true;
        return ControlMedia(b) == MediaResult::TimedOut && b.calls == 1;
    });
    Test("completed operation returns result", [] {
        FakeOperation op;
        return AwaitMedia(op, [&] { return op.time; }, [] { return false; }, 3000) && !op.cancelled;
    });
    Test("stalled operation times out and cancels", [] {
        FakeOperation op;
        op.completesAt = 10000;
        try {
            AwaitMedia(op, [&] { return op.time; }, [] { return false; }, 3000);
        } catch (MediaResult r) {
            return r == MediaResult::TimedOut && op.cancelled && op.time == 3000;
        }
        return false;
    });
    Test("shutdown cancels pending operation", [] {
        FakeOperation op;
        try {
            AwaitMedia(op, [&] { return op.time; }, [] { return true; }, 3000);
        } catch (MediaResult r) {
            return r == MediaResult::Cancelled && op.cancelled && op.time == 0;
        }
    });
    Test("cancellation failure preserves timeout outcome", [] {
        FakeOperation op;
        op.cancelThrows = true;
        try {
            AwaitMedia(op, [&] { return op.time; }, [] { return false; }, 0);
        } catch (MediaResult r) {
            return r == MediaResult::TimedOut;
        }
        return false;
    });
    Test("timeout completion permits retry; stale completion ignored", [] {
        MediaGate gate;
        auto first = gate.Begin();
        if (gate.Begin() != 0 || !gate.Complete(first))
            return false;
        auto next = gate.Begin();
        return next != first && !gate.Complete(first) && gate.busy && gate.Complete(next);
    });
    Test("shutdown invalidates queued media result", [] {
        MediaGate gate;
        auto id = gate.Begin();
        gate.Stop();
        return !gate.Complete(id);
    });
    Test("real WinRT operation survives multiple wait slices", [] {
        WinrtOperation pending{DelayedResult()};
        return AwaitMedia(pending, [] { return GetTickCount64(); }, [] { return false; }, GetTickCount64() + 1000);
    });
    Test("real WinRT late completion after timeout is safe", [] {
        bool timedOut = false;
        {
            WinrtOperation pending{DelayedResult()};
            try {
                AwaitMedia(pending, [] { return GetTickCount64(); }, [] { return false; }, GetTickCount64() + 20);
            } catch (MediaResult result) {
                timedOut = result == MediaResult::TimedOut;
            }
        }
        Sleep(200);
        return timedOut;
    });
    if (argc > 1)
        Test("cached rendering reuses frames and releases GDI resources", [] {
            HWND target =
                CreateWindowEx(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"Overlay test",
                               WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
            if (!target)
                return false;
            // Warm up Windows' process-wide GDI stock objects before counting
            // application-owned resources across a complete second lifecycle.
            UpdateFonts();
            POINT warmPosition{};
            RenderOverlay(target, 500, 150, &warmPosition);
            ReleaseOverlay();
            GdiFlush();
            DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            bool success = false;
            {
                ScopeExit destroy([&] {
                    DestroyWindow(target);
                    ReleaseOverlay();
                });
                UpdateFonts();
                POINT position{0, 0};
                auto builds = GetOverlayCacheStats().builds;
                auto coldStart = std::chrono::steady_clock::now();
                bool rendered = RenderOverlay(target, 500, 150, &position);
                auto warmStart = std::chrono::steady_clock::now();
                for (int i = 0; i < 20; ++i)
                    rendered = RenderOverlay(target, 500, 150, &position) && rendered;
                auto end = std::chrono::steady_clock::now();
                std::cout << "  Render ms: first="
                          << std::chrono::duration<double, std::milli>(warmStart - coldStart).count()
                          << " cached average="
                          << std::chrono::duration<double, std::milli>(end - warmStart).count() / 20 << '\n';
                bool reused = GetOverlayCacheStats().builds == builds + 1;
                rendered = RenderOverlay(target, 600, 160, &position) && rendered;
                success = rendered && reused && GetOverlayCacheStats().builds == builds + 2;
                if (!success)
                    std::cout << "  rendered=" << rendered << " reused=" << reused
                              << " builds=" << GetOverlayCacheStats().builds - builds << " error=" << GetLastError()
                              << '\n';
            }
            GdiFlush();
            auto after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            if (after > before)
                std::cout << "  GDI before=" << before << " after=" << after << '\n';
            return success && after <= before;
        });
    if (argc > 1)
        Test("keyboard thread pumps messages while caller waits", [] {
            if (!StartKeyboard(GetModuleHandle(nullptr))) {
                StopWorkers();
                return false;
            }
            bool separate = keyboardThreadId != GetCurrentThreadId();
            bool posted = PostThreadMessage(keyboardThreadId, WM_QUIT, 0, 0) != FALSE;
            bool exited = WaitForSingleObject(keyboardThread, 1000) == WAIT_OBJECT_0;
            StopWorkers();
            return separate && posted && exited;
        });
    std::cout << failures << " failed\n";
    winrt::uninit_apartment();
    return failures ? 1 : 0;
}
