#pragma once
#include <windows.h>
#include <propidl.h>
#include <memory>
#include <utility>

template <class F> class ScopeExit {
    F cleanup;

  public:
    explicit ScopeExit(F f) : cleanup(std::move(f)) {}
    ScopeExit(const ScopeExit &) = delete;
    ~ScopeExit() noexcept {
        cleanup();
    }
};
template <class T, auto Free> class NativeOwner {
    T value = nullptr;

  public:
    NativeOwner() = default;
    explicit NativeOwner(T v) : value(v) {}
    NativeOwner(const NativeOwner &) = delete;
    NativeOwner &operator=(const NativeOwner &) = delete;
    NativeOwner(NativeOwner &&other) noexcept : value(other.release()) {}
    NativeOwner &operator=(NativeOwner &&other) noexcept {
        reset(other.release());
        return *this;
    }
    NativeOwner &operator=(T other) {
        reset(other);
        return *this;
    }
    ~NativeOwner() {
        reset();
    }
    T get() const {
        return value;
    }
    T release() {
        return std::exchange(value, nullptr);
    }
    operator T() const {
        return value;
    }
    void reset(T v = nullptr) {
        if (value)
            Free(value);
        value = v;
    }
    explicit operator bool() const {
        return value != nullptr;
    }
};
using GdiObject = NativeOwner<HGDIOBJ, DeleteObject>;
using MenuOwner = NativeOwner<HMENU, DestroyMenu>;
using KernelHandle = NativeOwner<HANDLE, CloseHandle>;
class WindowDC {
    HWND owner;
    HDC dc;

  public:
    explicit WindowDC(HWND hwnd) : owner(hwnd), dc(GetDC(hwnd)) {
        if (!dc)
            throw HRESULT(E_FAIL);
    }
    WindowDC(const WindowDC &) = delete;
    ~WindowDC() {
        ReleaseDC(owner, dc);
    }
    operator HDC() const {
        return dc;
    }
};
using MemoryDC = NativeOwner<HDC, DeleteDC>;
class SelectedObject {
    HDC dc;
    HGDIOBJ old;

  public:
    SelectedObject(HDC target, HGDIOBJ object) : dc(target), old(SelectObject(target, object)) {
        if (!old || old == HGDI_ERROR)
            throw HRESULT(E_FAIL);
    }
    SelectedObject(const SelectedObject &) = delete;
    ~SelectedObject() {
        SelectObject(dc, old);
    }
};
struct VariantOwner {
    PROPVARIANT value;
    VariantOwner() {
        PropVariantInit(&value);
    }
    VariantOwner(const VariantOwner &) = delete;
    ~VariantOwner() {
        PropVariantClear(&value);
    }
};
struct TaskMemoryFree {
    void operator()(void *p) const {
        CoTaskMemFree(p);
    }
};
using TaskString = std::unique_ptr<wchar_t, TaskMemoryFree>;
