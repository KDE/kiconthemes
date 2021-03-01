/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 Christoph Cullmann <cullmann@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <kiconloader.h>

#include <QStandardPaths>
#include <QTest>

#include <KConfigGroup>
#include <KSharedConfig>

class KIconLoader_ResourceThemeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);

        // set our test theme only present in :/icons
        KConfigGroup cg(KSharedConfig::openConfig(), "Icons");
        cg.writeEntry("Theme", "themeinresource");
        cg.sync();
    }

    void testThemeFound()
    {
        // try to load icon that can only be found in resource theme and check we found it in the resource
        QString path;
        KIconLoader::global()->loadIcon(QStringLiteral("someiconintheme"), KIconLoader::Desktop, 22, KIconLoader::DefaultState, QStringList(), &path);
        QCOMPARE(path, QStringLiteral(":/icons/themeinresource/22x22/appsNoContext/someiconintheme.png"));
    }
};

QTEST_MAIN(KIconLoader_ResourceThemeTest)

#include "kiconloader_resourcethemetest.moc"
