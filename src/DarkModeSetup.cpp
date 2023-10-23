/*
    SPDX-FileCopyrightText: 2019 Richard Yu
    SPDX-FileCopyrightText: 2023 g10 Code GmbH
    SPDX-FileContributor: Sune Stolborg Vuorela <sune@vuorela.dk>
    SPDX-License-Identifier: MIT
*/

#define WIN32_LEAN_AND_MEAN
#include "DarkModeSetup.h"
#include <QByteArray>
#include <QList>
#include <QtGlobal>
#include <Windows.h>
#include <uxtheme.h>
struct DarkModeSetupHelper {
    // Code originates from https://github.com/ysc3839/win32-darkmode/
    enum PreferredAppMode { Default, AllowDark, ForceDark, ForceLight, Max };
    // needed to get version number; some functions depend on the build version
    using fnRtlGetNtVersionNumbers = void(WINAPI *)(LPDWORD major, LPDWORD minor, LPDWORD build);
    // check if apps should use darkmode
    using fnShouldAppsUseDarkMode = bool(WINAPI *)(); // ordinal 132
    // tell back if we can use darkmode; version dependent
    using fnAllowDarkModeForApp = bool(WINAPI *)(bool allow); // ordinal 135, in 1809
    using fnSetPreferredAppMode = PreferredAppMode(WINAPI *)(PreferredAppMode appMode); // ordinal 135, in 1903

    // Tells windows that it should reread things
    using fnRefreshImmersiveColorPolicyState = void(WINAPI *)(); // ordinal 104

    fnRefreshImmersiveColorPolicyState _RefreshImmersiveColorPolicyState = nullptr;
    fnAllowDarkModeForApp _AllowDarkModeForApp = nullptr;
    fnShouldAppsUseDarkMode _ShouldAppsUseDarkMode = nullptr;
    fnSetPreferredAppMode _SetPreferredAppMode = nullptr;

    bool darkModeSupported = false;
    bool darkModeEnabled = false;

    bool IsHighContrast() {
        HIGHCONTRASTW highContrast = {sizeof(highContrast)};
        if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, FALSE)) {
            return highContrast.dwFlags & HCF_HIGHCONTRASTON;
        }
        return false;
    }

    void AllowDarkModeForApp(bool allow) {
        if (_AllowDarkModeForApp) {
            _AllowDarkModeForApp(allow);
        } else if (_SetPreferredAppMode) {
           _SetPreferredAppMode(allow ? AllowDark : Default);
        }
    }

    DarkModeSetupHelper() {
        auto RtlGetNtVersionNumbers = reinterpret_cast<fnRtlGetNtVersionNumbers>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetNtVersionNumbers"));
        if (RtlGetNtVersionNumbers) {
            DWORD buildNumber = 0;
            DWORD major, minor;
            RtlGetNtVersionNumbers(&major, &minor, &buildNumber);
            buildNumber &= ~0xF0000000;

            if (major == 10 && minor == 0) {
                HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr,
                                          LOAD_LIBRARY_SEARCH_SYSTEM32);
                if (hUxtheme) {
                    _RefreshImmersiveColorPolicyState = reinterpret_cast<fnRefreshImmersiveColorPolicyState>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(104)));

                    _ShouldAppsUseDarkMode = reinterpret_cast<fnShouldAppsUseDarkMode>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(132)));

                    auto ord135 = GetProcAddress(hUxtheme, MAKEINTRESOURCEA(135));
                    if (buildNumber < 18362) {
                        _AllowDarkModeForApp = reinterpret_cast<fnAllowDarkModeForApp>(ord135);
                    } else {
                        _SetPreferredAppMode = reinterpret_cast<fnSetPreferredAppMode>(ord135);
                    }

                    if (_ShouldAppsUseDarkMode &&
                        _RefreshImmersiveColorPolicyState &&
                        (_AllowDarkModeForApp || _SetPreferredAppMode)) {
                        darkModeSupported = true;

                        AllowDarkModeForApp(true);
                        _RefreshImmersiveColorPolicyState();

                        darkModeEnabled = _ShouldAppsUseDarkMode() && !IsHighContrast();
                    }
               }
           }
       }
    }
};

const DarkModeSetupHelper &getHelper() {
    static DarkModeSetupHelper h;
    return h;
}

bool DarkModeSetup::isDarkModeActive() { return getHelper().darkModeEnabled; }

void DarkModeSetup::tellQt(int mode) {
    int qtm = qtMode();
    if (qtm > 0) {
        qWarning("Qt darkmode already enabled");
        return;
    }
    QByteArray arr = qgetenv("QT_QPA_PLATFORM");
    if (!arr.isEmpty()) {
        arr.append(',');
    }
    arr.append("windows:darkmode=" + QByteArray::number(mode));
    qputenv("QT_QPA_PLATFORM", arr);
}

int DarkModeSetup::qtMode() {
    const auto envvar = qgetenv("QT_QPA_PLATFORM");
    if (envvar.isEmpty()) {
        return 0;
    }
    const auto list = envvar.split(',');
    for (const auto &element : list) {
        if (element.startsWith("windows:darkmode=")) {
            auto data = element.right(element.length() - 17);
            bool success;
            int number = data.toInt(&success);
            if (success && number >= 0 && number <= 2) {
                return number;
            }
        }
    }
    return 0;
}
