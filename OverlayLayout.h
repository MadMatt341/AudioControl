#pragma once
#include <algorithm>
#include <string>
struct OverlaySize {
    int width, height;
};
inline OverlaySize BoundOverlay(int width, int height, int workWidth, int workHeight, int pad) {
    return {std::clamp(width, 1, std::max(1, std::min(4096, workWidth - 2 * pad - 16))),
            std::clamp(height, 1, std::max(1, std::min(2160, workHeight - 2 * pad - 16)))};
}
inline std::wstring BoundedOverlayText(const std::wstring &text) {
    constexpr size_t limit = 2048;
    if (text.size() <= limit)
        return text;
    size_t count = limit - 1;
    if (text[count - 1] >= 0xD800 && text[count - 1] <= 0xDBFF)
        --count;
    return text.substr(0, count) + L"\u2026";
}
