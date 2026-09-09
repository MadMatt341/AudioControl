#include "App.h"
#include "OverlayLayout.h"
std::wstring overlay, overlayLabel = L"AUDIO CONTROL";
GdiObject font, labelFont;
UINT overlayDpi = 96;
int Scale(int value) {
    return MulDiv(value, overlayDpi, 96);
}
void UpdateFonts() {
    GdiObject next(CreateFont(-Scale(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                              CLEARTYPE_QUALITY, 0, L"Consolas"));
    GdiObject nextLabel(CreateFont(-Scale(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                                   CLEARTYPE_QUALITY, 0, L"Consolas"));
    if (!next || !nextLabel)
        throw HRESULT(E_OUTOFMEMORY);
    font = std::move(next);
    labelFont = std::move(nextLabel);
}
struct OverlayCache {
    int width = 0, height = 0;
    UINT dpi = 0;
    GdiObject bitmap;
    MemoryDC dc;
    void *raw = nullptr;
    std::vector<DWORD> background;
} cache;
unsigned cacheBuilds = 0;
OverlayCacheStats GetOverlayCacheStats() {
    return {cacheBuilds, cache.background.size() * sizeof(DWORD) * 2};
}
bool RenderOverlay(HWND hwnd, int w, int h, POINT *destination);
void RaiseOverlay(HWND hwnd) {
    // Topmost windows still have an order among themselves. Reinsert at its
    // front without activating the overlay or moving/resizing its pixels.
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}
void Show(const std::wstring &text, const std::wstring &label) {
    overlay = BoundedOverlayText(text);
    overlayLabel = BoundedOverlayText(label);
    POINT p{};
    if (!GetCursorPos(&p))
        throw HRESULT(E_FAIL);
    MONITORINFO mi{sizeof(mi)};
    if (!GetMonitorInfo(MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST), &mi))
        throw HRESULT(E_FAIL);
    UINT dpiX = 96, dpiY = 96;
    GetDpiForMonitor(MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST), MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    if (overlayDpi != dpiX) {
        overlayDpi = dpiX;
        UpdateFonts();
    }
    WindowDC dc(window);
    SelectedObject selected(dc, font);
    SIZE size{};
    GetTextExtentPoint32(dc, overlay.c_str(), static_cast<int>(overlay.size()), &size);
    const int pad = Scale(20);
    int available = std::max<int>(1, mi.rcWork.right - mi.rcWork.left - 2 * pad - 16);
    if (available < Scale(100) || mi.rcWork.bottom - mi.rcWork.top < Scale(100) + 2 * pad)
        return;
    int w = std::min(available, std::clamp<int>(size.cx + Scale(64), Scale(360), Scale(560)));
    RECT measured{0, 0, w - Scale(64), 0};
    DrawText(dc, overlay.c_str(), -1, &measured, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    int h = std::clamp<int>(measured.bottom + Scale(56), Scale(88), Scale(240));
    auto bounded = BoundOverlay(w, h, mi.rcWork.right - mi.rcWork.left, mi.rcWork.bottom - mi.rcWork.top, pad);
    w = bounded.width;
    h = bounded.height;
    POINT destination{mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2 - pad,
                      std::max(mi.rcWork.top + 8, mi.rcWork.bottom - h - Scale(64) - pad)};
    if (RenderOverlay(window, w + 2 * pad, h + 2 * pad, &destination)) {
        ShowWindow(window, SW_SHOWNOACTIVATE);
        RaiseOverlay(window);
        SetTimer(window, OverlayHideTimer, 2000, nullptr);
        // Only while visible: recover if another topmost tool window appears.
        SetTimer(window, OverlayTopmostTimer, 100, nullptr);
    } else
        throw HRESULT(E_FAIL);
}
void ShowDevice(const std::wstring &name, const std::wstring &status) {
    // Split the outer Windows "type (device)" pair, preserving inner parentheses.
    auto split = name.find(L" (");
    if (split != std::wstring::npos && name.back() == L')')
        Show(name.substr(split + 2, name.size() - split - 3), status.empty() ? name.substr(0, split) : status);
    else
        Show(name, status.empty() ? L"AUDIO OUTPUT" : status);
}
// Build the entire frame offscreen, then publish pixels, size and position together.
bool RenderOverlay(HWND hwnd, int w, int h, POINT *destination) {
    int pad = Scale(20);
    RECT r{pad, pad, w - pad, h - pad};
    WindowDC screen(nullptr);
    if (cache.width != w || cache.height != h || cache.dpi != overlayDpi) {
        OverlayCache next;
        next.width = w;
        next.height = h;
        next.dpi = overlayDpi;
        next.dc.reset(CreateCompatibleDC(screen));
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w;
        info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        next.bitmap.reset(CreateDIBSection(screen, &info, DIB_RGB_COLORS, &next.raw, nullptr, 0));
        if (!next.bitmap || !next.dc)
            throw HRESULT(E_OUTOFMEMORY);
        next.background.resize(static_cast<size_t>(w) * h);
        auto pixels = next.background.data();
        float radius = float(Scale(9));
        auto distance = [&](float x, float y) {
            float qx = std::abs(x - w * 0.5f) - (w - 2 * pad) * 0.5f + radius;
            float qy = std::abs(y - h * 0.5f) - (h - 2 * pad) * 0.5f + radius;
            return std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - radius;
        };
        // Analytic coverage gives smooth corners and a subtle translucent shadow.
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                float d = distance(x + 0.5f, y + 0.5f);
                float cover = std::clamp(0.5f - d, 0.0f, 1.0f);
                float inside = std::clamp(0.5f - d - Scale(1), 0.0f, 1.0f);
                float sd = std::max(0.0f, distance(x + 0.5f - Scale(3), y + 0.5f - Scale(5)));
                float shadow = 0.22f * std::exp(-sd * sd / (2.0f * Scale(6) * Scale(6)));
                size_t i = static_cast<size_t>(y) * w + x;
                BYTE alpha = static_cast<BYTE>(std::lround((cover + shadow * (1 - cover)) * 255));
                auto channel = [&](int fill, int border) {
                    return DWORD(std::lround(fill * inside + border * (cover - inside)));
                };
                pixels[i] = (DWORD(alpha) << 24) | (channel(24, 58) << 16) | (channel(28, 68) << 8) | channel(34, 78);
            }
        cache = std::move(next);
        ++cacheBuilds;
    }
    HDC dc = cache.dc.get();
    SelectedObject selectedBitmap(dc, cache.bitmap);
    auto pixels = static_cast<DWORD *>(cache.raw);
    std::copy(cache.background.begin(), cache.background.end(), pixels);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(125, 211, 190));
    SelectedObject selectedFont(dc, labelFont);
    RECT label{pad + Scale(28), pad + Scale(20), r.right - Scale(28), pad + Scale(37)};
    DrawText(dc, overlayLabel.c_str(), -1, &label, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(dc, font);
    SetTextColor(dc, RGB(236, 241, 246));
    r.left += Scale(28);
    r.right -= Scale(28);
    r.top += Scale(42);
    r.bottom -= Scale(12);
    DrawText(dc, overlay.c_str(), -1, &r, DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
    GdiFlush();
    // Restore alpha cleared by GDI text in the opaque panel interior.
    for (size_t i = 0; i < cache.background.size(); ++i)
        pixels[i] = (pixels[i] & 0x00ffffff) | (cache.background[i] & 0xff000000);
    POINT source{};
    SIZE size{w, h};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    BOOL updated = UpdateLayeredWindow(hwnd, screen, destination, &size, dc, &source, 0, &blend, ULW_ALPHA);
    return updated != FALSE;
}

void ReleaseOverlay() {
    font.reset();
    labelFont.reset();
    cache = OverlayCache{};
}
