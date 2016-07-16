/*
    Copyright (C) 2016 David Rosca <nowrep@gmail.com>

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

#include <QTest>
#include <QStandardPaths>

#include <KSharedConfig>
#include <KConfigGroup>
#include <KIconEngine>
#include <KIconLoader>

extern KICONTHEMES_EXPORT int kiconloader_ms_between_checks;

class KIconEngine_UnitTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);

        // Remove icon cache
        const QString cacheFile = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/icon-cache.kcache";
        QFile::remove(cacheFile);

        KConfigGroup cg(KSharedConfig::openConfig(), "Icons");
        cg.writeEntry("Theme", "oxygen");
        cg.sync();

        QDir testDataDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
        testIconsDir = QDir(testDataDir.absoluteFilePath(QStringLiteral("icons")));

        // we will be recursively deleting these, so a sanity check is in order
        QVERIFY(testIconsDir.absolutePath().contains(QStringLiteral("qttest")));

        testIconsDir.removeRecursively();

        // set up a minimal Oxygen icon theme, in case it is not installed
        QVERIFY(testIconsDir.mkpath(QStringLiteral("oxygen/22x22/apps")));
        QVERIFY(QFile::copy(QStringLiteral(":/oxygen.theme"), testIconsDir.filePath(QStringLiteral("oxygen/index.theme"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("oxygen/22x22/apps/kde.png"))));
        QVERIFY(testIconsDir.mkpath(QStringLiteral("oxygen/22x22/actions"))); // we need the dir to exist since KIconThemes caches mDirs

        // Clear SHM cache
        KIconLoader::global()->reconfigure(QString());
    }

    void testValidIconName()
    {
        QIcon icon(new KIconEngine(QStringLiteral("kde"), KIconLoader::global()));
        QVERIFY(!icon.isNull());
        QVERIFY(!icon.name().isEmpty());
    }

    void testInvalidIconName()
    {
        QIcon icon(new KIconEngine(QStringLiteral("invalid-icon-name"), KIconLoader::global()));
#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
        QEXPECT_FAIL("", "IsNullHook needs Qt 5.7", Continue);
#endif
        QVERIFY(icon.isNull());
        QVERIFY2(icon.name().isEmpty(), qPrintable(icon.name()));
    }

    void testUnknownIconNotCached() // QIcon version of the test in kiconloader_unittest.cpp
    {
        // This is a test to ensure that "unknown" icons are cached as unknown
        // for performance reasons, but after a while they'll be looked up again
        // so that newly installed icons can be used without a reboot.

        kiconloader_ms_between_checks = 500000;

        QString actionIconsSubdir = QStringLiteral("oxygen/22x22/actions");
        QVERIFY(testIconsDir.mkpath(actionIconsSubdir));
        QString actionIconsDir = testIconsDir.filePath(actionIconsSubdir);

        QString nonExistingIconName = QStringLiteral("asvdfg_fhqwhgds");
        QString newIconPath = actionIconsDir + QLatin1String("/")
                              + nonExistingIconName + QLatin1String(".png");
        QFile::remove(newIconPath);

        // Find a non-existent icon
        QIcon icon(new KIconEngine(nonExistingIconName, KIconLoader::global()));
        QVERIFY(icon.isNull());
        QVERIFY(icon.name().isEmpty());

        // Install the existing icon by copying.
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), newIconPath));

        // Attempt to find the icon again, the cache will still be used for now.
        QIcon icon2(new KIconEngine(nonExistingIconName, KIconLoader::global()));
        QVERIFY(icon2.isNull());
        QVERIFY(icon2.name().isEmpty());

        // Force a recheck to happen on next lookup
        kiconloader_ms_between_checks = 0;

        // Verify the icon can now be found.
        QIcon nowExistingIcon(new KIconEngine(nonExistingIconName, KIconLoader::global()));
        QVERIFY(!nowExistingIcon.isNull());
        QCOMPARE(nowExistingIcon.name(), nonExistingIconName);

        // And verify again, this time with the cache
        kiconloader_ms_between_checks = 50000;
        QIcon icon3(new KIconEngine(nonExistingIconName, KIconLoader::global()));
        QVERIFY(!icon3.isNull());
        QCOMPARE(icon3.name(), nonExistingIconName);

    }
private:
    QDir testIconsDir;
};

QTEST_MAIN(KIconEngine_UnitTest)

#include "kiconengine_unittest.moc"
