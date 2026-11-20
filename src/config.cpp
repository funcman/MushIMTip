#include "config.h"

namespace mushimtip {

namespace {

constexpr wchar_t kRegKey[]  = L"Software\\MushIMTip";
constexpr wchar_t kRunKey[]  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kAppName[] = L"MushIMTip";

constexpr wchar_t kValFollowCaret[]     = L"FollowCaret";
constexpr wchar_t kValSizeFollowCaret[] = L"SizeFollowCaret";
constexpr wchar_t kValScalePct[]        = L"ScalePct";
constexpr wchar_t kValAlpha[]           = L"Alpha";
constexpr wchar_t kValMaxHeight[]       = L"MaxHeight";
constexpr wchar_t kValColorChinese[]    = L"ColorChinese";
constexpr wchar_t kValColorEnglish[]    = L"ColorEnglish";

DWORD ReadDword(HKEY key, const wchar_t* name, DWORD fallback) {
    DWORD value = fallback;
    DWORD size = sizeof(value);
    if (RegGetValueW(key, name, nullptr, RRF_RT_REG_DWORD,
                     nullptr, &value, &size) != ERROR_SUCCESS)
        return fallback;
    return value;
}

void WriteDword(HKEY key, const wchar_t* name, DWORD value) {
    RegSetValueExW(key, name, 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

} // namespace

Config LoadConfig() {
    Config config;

    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return config;

    config.followCaret     = ReadDword(key, kValFollowCaret,     config.followCaret) != 0;
    config.sizeFollowCaret = ReadDword(key, kValSizeFollowCaret, config.sizeFollowCaret) != 0;
    config.scalePct        = static_cast<int>(ReadDword(key, kValScalePct, config.scalePct));
    config.alpha           = static_cast<int>(ReadDword(key, kValAlpha,    config.alpha));
    config.maxHeightPx     = static_cast<int>(ReadDword(key, kValMaxHeight, config.maxHeightPx));
    config.colorChinese    = ReadDword(key, kValColorChinese, config.colorChinese);
    config.colorEnglish    = ReadDword(key, kValColorEnglish, config.colorEnglish);

    RegCloseKey(key);
    return config;
}

void SaveConfig(const Config& config) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return;

    WriteDword(key, kValFollowCaret,     config.followCaret);
    WriteDword(key, kValSizeFollowCaret, config.sizeFollowCaret);
    WriteDword(key, kValScalePct,        static_cast<DWORD>(config.scalePct));
    WriteDword(key, kValAlpha,           static_cast<DWORD>(config.alpha));
    WriteDword(key, kValMaxHeight,       static_cast<DWORD>(config.maxHeightPx));
    WriteDword(key, kValColorChinese,    config.colorChinese);
    WriteDword(key, kValColorEnglish,    config.colorEnglish);

    RegCloseKey(key);
}

bool IsAutoStartEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;
    DWORD size = 0;
    bool present = RegQueryValueExW(key, kAppName, nullptr, nullptr, nullptr, &size) == ERROR_SUCCESS;
    RegCloseKey(key);
    return present;
}

void SetAutoStart(bool enable) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;
    if (enable) {
        wchar_t path[MAX_PATH + 2] = {};
        GetModuleFileNameW(nullptr, path + 1, MAX_PATH);
        path[0] = L'"';
        size_t len = wcslen(path);
        path[len] = L'"';
        path[len + 1] = L'\0';
        RegSetValueExW(key, kAppName, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(path),
                       static_cast<DWORD>((wcslen(path) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kAppName);
    }
    RegCloseKey(key);
}

} // namespace mushimtip
