// SPDX-FileCopyrightText: 2023 Alexander Lohnau <alexander.lohnau@gmx.de>
// SPDX-License-Identifier: LGPL-2.0-or-later

#include "icondialog_p.h"
#include <QQmlEngine>
#include <QQmlExtensionPlugin>

class KIconThemesQmlPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface")

public:
    void registerTypes(const char *uri) override
    {
        qmlRegisterType<IconDialog>(uri, 1, 0, "IconDialog");
    }
};

#include "plugin.moc"
