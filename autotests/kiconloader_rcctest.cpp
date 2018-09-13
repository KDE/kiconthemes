/* This file is part of the KDE libraries
    Copyright 2015 Christoph Cullmann <cullmann@kde.org>
    Copyright 2016 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <kiconloader.h>
#include <kicontheme.h>

#include <QDir>
#include <QStandardPaths>
#include <QPixmap>
#include <QTest>

#include <KSharedConfig>
#include <KConfigGroup>

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
        KIconLoader::global()->loadIcon(QStringLiteral("someiconintheme"), KIconLoader::Desktop, 22,
                                    KIconLoader::DefaultState, QStringList(),
                                    &path);
        QCOMPARE(path, QString(QStringLiteral(":/icons/") + m_internalThemeName + QStringLiteral("/22x22/appsNoContext/someiconintheme.png")));

    }
private:
    const QString m_internalThemeName = QStringLiteral("kf5_rcc_theme");
};

QTEST_MAIN(KIconLoader_RCCThemeTest)

#include "kiconloader_rcctest.moc"
