/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 Christoph Cullmann <cullmann@kde.org>
    SPDX-FileCopyrightText: 2016 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <kiconloader.h>
#include <kicontheme.h>

#include <QDir>
#include <QStandardPaths>
#include <QTest>

#include <KSharedConfig>

// Install icontheme.rcc where KIconThemes will find it.
// This must be done before QCoreApplication is even created, given the Q_COREAPP_STARTUP_FUNCTION in kiconthemes
void earlyInit()
{
    QStandardPaths::setTestModeEnabled(true);
    qputenv("XDG_DATA_DIRS", "/doesnotexist"); // ensure hicolor/oxygen/breeze are not found
    QFile rcc(QStringLiteral("icontheme.rcc"));
    Q_ASSERT(rcc.exists());
    QCoreApplication::setApplicationName(QStringLiteral("myappname")); // for a fixed location on Unix (appname is empty here otherwise)
    const QString destDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(destDir);
    const QString dest = destDir + QStringLiteral("/icontheme.rcc");
    QFile::remove(dest);
    if (!rcc.copy(dest)) {
        qWarning() << "Error copying to" << dest;
    }
}
Q_CONSTRUCTOR_FUNCTION(earlyInit)

class KIconLoader_RCCThemeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Remove icon cache
        const QString cacheFile = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/icon-cache.kcache");
        QFile::remove(cacheFile);

        // Clear SHM cache
        KIconLoader::global()->reconfigure(QString());
    }

    void testThemeName()
    {
        QCOMPARE(QIcon::themeName(), m_internalThemeName);
    }

    void testQIconFromTheme()
    {
        // load icon with Qt API
        QVERIFY(!QIcon::fromTheme(QStringLiteral("someiconintheme")).isNull());
    }

    void testKIconLoader()
    {
        // Check that direct usage of KIconLoader (e.g. from KToolBar) works
        QVERIFY(KIconLoader::global()->theme());
        QCOMPARE(KIconLoader::global()->theme()->internalName(), m_internalThemeName);

        // load icon with KIconLoader API (unlikely to happen in reality)
        QString path;
        KIconLoader::global()->loadIcon(QStringLiteral("someiconintheme"), KIconLoader::Desktop, 22, KIconLoader::DefaultState, QStringList(), &path);
        QCOMPARE(path, QString(QStringLiteral(":/icons/") + m_internalThemeName + QStringLiteral("/22x22/appsNoContext/someiconintheme.png")));
    }

private:
    const QString m_internalThemeName = QStringLiteral("kf5_rcc_theme");
};

QTEST_MAIN(KIconLoader_RCCThemeTest)

#include "kiconloader_rcctest.moc"
