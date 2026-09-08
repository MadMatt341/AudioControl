#pragma once
#include <filesystem>
#include <string>
struct HotkeySettings {
    bool leftAltA = true;
    unsigned modifiers = 1, key = 'A';
};
std::filesystem::path ExecutablePath();
std::filesystem::path InitializeSettings(bool portable);
HotkeySettings ReadHotkeySettings(const std::filesystem::path &path);
unsigned ParseSetting(const std::wstring &text, unsigned maximum);
// Exposed to test migration in an isolated temporary directory.
std::filesystem::path PrepareSettings(const std::filesystem::path &directory, const std::filesystem::path &legacy);
