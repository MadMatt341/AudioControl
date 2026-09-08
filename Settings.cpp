#include "App.h"
#include "Settings.h"
#include "NativeResources.h"
#include <shlobj.h>
#include <cwctype>
#include <stdexcept>

std::filesystem::path ExecutablePath() {
    std::vector<wchar_t> buffer(256);
    while (buffer.size() <= 32768) {
        const DWORD count = GetModuleFileName(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (!count)
            Check(HRESULT_FROM_WIN32(GetLastError()));
        if (count < buffer.size())
            return std::wstring(buffer.data(), count);
        buffer.resize(buffer.size() * 2);
    }
    throw std::runtime_error("Executable path is too long");
}
std::filesystem::path PrepareSettings(const std::filesystem::path &directory, const std::filesystem::path &legacy) {
    auto full = std::filesystem::absolute(directory).wstring();
    if (full.size() >= 240 && full.rfind(L"\\\\?\\", 0) != 0)
        full = full.rfind(L"\\\\", 0) == 0 ? L"\\\\?\\UNC\\" + full.substr(2) : L"\\\\?\\" + full;
    std::filesystem::create_directories(full);
    const auto target = std::filesystem::path(full) / L"AudioControl.ini";
    if (!std::filesystem::exists(target)) {
        if (std::filesystem::exists(legacy)) {
            // Never overwrite an existing user configuration.
            std::filesystem::copy_file(legacy, target, std::filesystem::copy_options::skip_existing);
        } else if (!WritePrivateProfileString(L"Hotkey", L"LeftAltA", L"1", target.c_str())) {
            throw std::runtime_error("Unable to create settings file");
        }
    }
    return target;
}
std::filesystem::path InitializeSettings(bool portable) {
    const auto legacy = ExecutablePath().parent_path() / L"AudioControl.ini";
    if (portable)
        return PrepareSettings(legacy.parent_path(), legacy);
    PWSTR raw = nullptr;
    Check(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw));
    TaskString folder(raw);
    return PrepareSettings(std::filesystem::path(folder.get()) / L"AudioControl", legacy);
}
unsigned ParseSetting(const std::wstring &text, unsigned maximum) {
    if (text.empty())
        throw std::runtime_error("Empty hotkey setting");
    unsigned value = 0;
    for (wchar_t ch : text) {
        if (ch < L'0' || ch > L'9')
            throw std::runtime_error("Hotkey settings must be decimal numbers");
        unsigned digit = ch - L'0';
        if (digit > maximum || value > (maximum - digit) / 10)
            throw std::runtime_error("Hotkey setting is out of range");
        value = value * 10 + digit;
    }
    return value;
}
HotkeySettings ReadHotkeySettings(const std::filesystem::path &path) {
    auto read = [&](const wchar_t *key, const wchar_t *fallback, unsigned max) {
        wchar_t value[64]{};
        DWORD count = GetPrivateProfileString(L"Hotkey", key, fallback, value, 64, path.c_str());
        if (count >= 63)
            throw std::runtime_error("Hotkey setting is too long");
        return ParseSetting(value, max);
    };
    HotkeySettings result;
    result.leftAltA = read(L"LeftAltA", L"1", 1) != 0;
    // Generic settings only apply when side-specific Alt+A is disabled.
    if (!result.leftAltA) {
        result.modifiers = read(L"Modifiers", L"1", 15);
        result.key = read(L"Key", L"65", 254);
        if (result.key < 8 || result.key == VK_F12 || result.key == VK_SHIFT || result.key == VK_CONTROL ||
            result.key == VK_MENU || result.key == VK_LWIN || result.key == VK_RWIN ||
            (result.key >= VK_LSHIFT && result.key <= VK_RMENU))
            throw std::runtime_error("Choose a non-modifier keyboard key; F12 is reserved by Windows");
        if (result.modifiers == MOD_ALT && result.key == 'S')
            throw std::runtime_error("Alt+S is reserved for media control");
    }
    return result;
}
