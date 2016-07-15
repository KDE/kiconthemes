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

        // Clear SHM cache
        KIconLoader::global()->reconfigure(QString());

        KConfigGroup cg(KSharedConfig::openConfig(), "Icons");
        cg.writeEntry("Theme", "oxygen");
        cg.sync();

        QDir testDataDir = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
        QDir testIconsDir = QDir(testDataDir.absoluteFilePath(QStringLiteral("icons")));

        // we will be recursively deleting these, so a sanity check is in order
        QVERIFY(testIconsDir.absolutePath().contains(QStringLiteral("qttest")));

        testIconsDir.removeRecursively();

        // set up a minimal Oxygen icon theme, in case it is not installed
        QVERIFY(testIconsDir.mkpath(QStringLiteral("oxygen/22x22/apps")));
        QVERIFY(QFile::copy(QStringLiteral(":/oxygen.theme"), testIconsDir.filePath(QStringLiteral("oxygen/index.theme"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("oxygen/22x22/apps/kde.png"))));
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
};

QTEST_MAIN(KIconEngine_UnitTest)

#include "kiconengine_unittest.moc"
