/*
    SPDX-FileCopyrightText: 2023 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>
    SPDX-FileContributor: Andre Heinecke <aheinecke@gnupg.org>
    SPDX-FileContributor: Sune Stolborg Vuorela <sune@vuorela.dk>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "DarkModeSetup.h"
#include "debug.h"

#include <QSettings>

#include "windows.h"

namespace
{
bool win_isHighContrastModeActive()
{
    HIGHCONTRAST result;
    result.cbSize = sizeof(HIGHCONTRAST);
    if (SystemParametersInfo(SPI_GETHIGHCONTRAST, result.cbSize, &result, 0)) {
        return (result.dwFlags & HCF_HIGHCONTRASTON);
    }
    return false;
}

bool win_isDarkModeActive()
{
    // Most intutively this GetSysColor seems the right function for this
    // but according to
    // https://learn.microsoft.com/de-de/windows/win32/api/winuser/nf-winuser-getsyscolor
    // these are already either deprecated or removed.

    if (win_isHighContrastModeActive()) {
        // Windows 11 has only one white High Contrast mode. The other
        // three are dark. So even if we can't cath the white one let
        // us assume that it it is dark.
        qCDebug(KICONTHEMES) << "Bright icons for HighContrast";
        return true;
    }

    // This is what KColorschemeWatcher and Qt also look for to check for dark mode. I would use this as a fallback. Because it only works good
    // for dark mode so the style and not the accessibility feature it is not enough for us. But having this as the default behavior is the
    // best way to keep it all in line.
    const QSettings regSettings{QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
                                QSettings::NativeFormat};

    // Nothing set -> default to bright. / Leave it to KColorSchemeManager.
    const bool appsUseLightTheme = regSettings.value(QStringLiteral("AppsUseLightTheme"), true).value<bool>();
    if (!appsUseLightTheme) {
        qCDebug(KICONTHEMES) << "Bright icons for AppsUseLightTheme false";
        return true;
    }
    // Default to no dark mode active
    return false;
}

};

bool DarkModeSetup::isDarkModeActive()
{
    return win_isDarkModeActive();
}

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
