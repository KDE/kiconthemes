/*
    SPDX-FileCopyrightText: 2019 Richard Yu
    SPDX-FileCopyrightText: 2023 g10 Code GmbH
    SPDX-FileContributor: Sune Stolborg Vuorela <sune@vuorela.dk>
    SPDX-License-Identifier: MIT
*/


class DarkModeSetup {
public:
    // Queries thru dark magic if dark mode is active or not using weird windows non-api's
    // This is valid before QApplication has been created
    bool isDarkModeActive();
    // Tell Qt about our capabilities
    // See https://doc.qt.io/qt-6/qguiapplication.html#platform-specific-arguments for details
    // must be called before creating QApplication
    void tellQt(int mode = 2);
    // gets previously mode told qt
    int qtMode();
};
