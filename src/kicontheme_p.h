/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 Christoph Cullmann <cullmann@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KICONTHEME_P_H
#define KICONTHEME_P_H

/**
 * Support for icon themes in RCC files.
 * The intended use case is standalone apps on Windows / MacOS / etc.
 * For this reason we use AppDataLocation: BINDIR/data on Windows, Resources on OS X.
 * Will be triggered by KIconLoaderGlobalData construction.
 */
void initRCCIconTheme();

#endif
