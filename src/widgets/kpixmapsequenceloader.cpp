/*
    SPDX-FileCopyrightText: 2000 Geert Jansen <jansen@kde.org>
    SPDX-FileCopyrightText: 2000 Antonio Larrosa <larrosa@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kpixmapsequenceloader.h"

#include <KIconLoader>

namespace KPixmapSequenceLoader
{

KPixmapSequence load(const QString &iconName, int size)
{
    return KPixmapSequence(KIconLoader::global()->iconPath(iconName, -size), size);
}

}

#if KICONTHEMES_VERSION >= QT_VERSION_CHECK(6, 240, 0)
// The include of <KPixmapSequence> needs public linkage to KWidgetsAddons of KIconWidgets,
// just for this one util method (which tries to hide the KIconLoader complexity).
// Also the current (wrong) private linkage of KWidgetsAddons pulls in that runtime dependency for
// any other consumers of KIconWidgets.
// So instead providing the dependency is better left just to the users of this util method,
// who will need KWidgetsAddons anyway to handle the returned KPixmapSequence instance.
#pragma message("Make KPixmapSequenceLoader::load() inline header-only, drop linking KWidgetsAddons (cmp. KQuickIconProvider)")
#endif
