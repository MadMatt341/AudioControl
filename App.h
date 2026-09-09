#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#pragma comment(lib, "shcore.lib")
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <memory>
#include "AudioLogic.h"
#include "NativeResources.h"
using Microsoft::WRL::ComPtr;

struct Device {
    std::wstring id, name;
    size_t number = 0;
};
extern std::wstring config;
extern HWND window;
constexpr UINT TrayMessage = WM_APP + 1, CycleMessage = WM_APP + 2, MediaMessage = WM_APP + 3,
               MediaResultMessage = WM_APP + 4;
constexpr UINT_PTR OverlayHideTimer = 1, OverlayTopmostTimer = 2;
void Check(HRESULT hr);
ComPtr<IMMDeviceEnumerator> Enumerator();
std::wstring Current(IMMDeviceEnumerator *, ERole);
std::vector<Device> Devices(IMMDeviceEnumerator *, unsigned *skipped = nullptr);
bool Included(const Device &);
size_t Next(const std::vector<Device> &, const std::wstring &);
void Cycle();
void Show(const std::wstring &, const std::wstring &label = L"AUDIO CONTROL");
void ShowDevice(const std::wstring &, const std::wstring &status = L"");
void RaiseOverlay(HWND);
void UpdateFonts();
void ReleaseOverlay();
bool StartKeyboard(HINSTANCE);
void StopKeyboard();
void ConfigureKeyboard(bool);
bool ShortcutChecks();
extern KernelHandle keyboardThread;
extern DWORD keyboardThreadId;
void StopWorkers();
bool BeginMedia(HWND);
bool CompleteMedia(LPARAM);
void StopMedia();
bool RenderOverlay(HWND, int, int, POINT *);
struct OverlayCacheStats {
    unsigned builds;
    size_t bytes;
};
OverlayCacheStats GetOverlayCacheStats();
