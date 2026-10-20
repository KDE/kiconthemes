/*
    SPDX-FileCopyrightText: 2016 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <kicontheme.h>

class KIconTheme_UnitTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        KIconTheme::forceThemeForTests(QString()); // Reset override.
    }

    void testForceThemeForTests()
    {
        auto forcedName = QStringLiteral("kitten");
        auto resolvedCurrent = KIconTheme::current();
        QVERIFY2(KIconTheme::current() != forcedName, "current theme initially expected to not be mangled");
        // Force a specific theme.
        KIconTheme::forceThemeForTests(forcedName);
        QCOMPARE(KIconTheme::current(), forcedName);
        // Reset override.
        KIconTheme::forceThemeForTests(QString());
        QCOMPARE(KIconTheme::current(), resolvedCurrent);
        // And then override again to make sure we still can.
        KIconTheme::forceThemeForTests(forcedName);
        QCOMPARE(KIconTheme::current(), forcedName);
    }

    void testScaleChoosesTheDirectory_data()
    {
        QTest::addColumn<qreal>("scale");
        QTest::addColumn<QString>("expectedDir");

        // A 64 pixel icon. The 96 directory is scalable from 96 to 256 pixels, as Breeze's places/96.
        QTest::newRow("100%") << 1.0 << QStringLiteral("64");
        QTest::newRow("125%, no directory for 80 pixels") << 1.25 << QStringLiteral("64@2x");
        QTest::newRow("150%, the 96 directory is made for 96 pixels") << 1.5 << QStringLiteral("96");
        QTest::newRow("200%, the 64 drawing at twice its size") << 2.0 << QStringLiteral("64@2x");
    }

    // The icon comes from the drawing that needs the least resizing for the screen's scale.
    void testScaleChoosesTheDirectory()
    {
        QFETCH(qreal, scale);
        QFETCH(QString, expectedDir);

        // Themes are found in the icons directory of the data directories. This one has a data
        // directory of its own, since other tests empty the icons directory of the test location.
        QTemporaryDir dataDir;
        QVERIFY(dataDir.isValid());
        const QByteArray previousDataDirs = qgetenv("XDG_DATA_DIRS");
        qputenv("XDG_DATA_DIRS", QByteArray(QFile::encodeName(dataDir.path()) + ':' + previousDataDirs));
        const auto restoreDataDirs = qScopeGuard([&previousDataDirs] {
            qputenv("XDG_DATA_DIRS", previousDataDirs);
        });
        const QString themeDir = dataDir.path() + QStringLiteral("/icons/scaletest/");
        QVERIFY(QDir().mkpath(themeDir));
        QFile index(themeDir + QStringLiteral("index.theme"));
        QVERIFY(index.open(QIODevice::WriteOnly));
        index.write(
            "[Icon Theme]\nName=scaletest\nDirectories=64,96\nScaledDirectories=64@2x\n\n"
            "[64]\nSize=64\nType=Fixed\n\n"
            "[64@2x]\nSize=64\nScale=2\nType=Fixed\n\n"
            "[96]\nSize=96\nType=Scalable\nMinSize=96\nMaxSize=256\n");
        index.close();
        for (const QString &dir : {QStringLiteral("64"), QStringLiteral("64@2x"), QStringLiteral("96")}) {
            QVERIFY(QDir().mkpath(themeDir + dir));
            QFile icon(themeDir + dir + QStringLiteral("/folder.svg"));
            QVERIFY(icon.open(QIODevice::WriteOnly));
        }

        KIconTheme theme(QStringLiteral("scaletest"));
        QVERIFY(theme.isValid());
        const QString path = theme.iconPath(QStringLiteral("folder.svg"), 64, KIconLoader::MatchBest, scale);
        QCOMPARE(path, themeDir + expectedDir + QStringLiteral("/folder.svg"));
    }
};

QTEST_MAIN(KIconTheme_UnitTest)

#include "kicontheme_unittest.moc"
