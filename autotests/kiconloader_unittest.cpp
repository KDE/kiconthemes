/* This file is part of the KDE libraries
    Copyright 2008 David Faure <faure@kde.org>

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

#include <QStandardPaths>
#include <QTest>
#include <QTemporaryDir>

#include <kpixmapsequence.h>

#include <KSharedConfig>
#include <KConfigGroup>

class KIconLoader_UnitTest : public QObject
{
    Q_OBJECT

private:
    QDir testDataDir;
    QDir testIconsDir;
    QString appName;
    QDir appDataDir;

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);

        const QStringList genericIconsFiles = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "mime/generic-icons");
        QVERIFY(!genericIconsFiles.isEmpty()); // KIconLoader relies on fallbacks to generic icons (e.g. x-office-document), which comes from a shared-mime-info file. Make sure it's installed!

        KConfigGroup cg(KSharedConfig::openConfig(), "Icons");
        cg.writeEntry("Theme", "breeze");
        cg.sync();

        testDataDir = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
        testIconsDir = QDir(testDataDir.absoluteFilePath(QStringLiteral("icons")));

        appName = QStringLiteral("kiconloader_unittest");
        appDataDir = QDir(testDataDir.absoluteFilePath(appName));

        // we will be recursively deleting these, so a sanity check is in order
        QVERIFY(testIconsDir.absolutePath().contains(QStringLiteral("qttest")));
        QVERIFY(appDataDir.absolutePath().contains(QStringLiteral("qttest")));

        testIconsDir.removeRecursively();
        appDataDir.removeRecursively();

        QVERIFY(appDataDir.mkpath(QStringLiteral("pics")));
        QVERIFY(QFile::copy(QStringLiteral(":/app-image.png"), appDataDir.filePath(QStringLiteral("pics/image1.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/app-image.png"), appDataDir.filePath(QStringLiteral("pics/image2.png"))));

        // set up a minimal Oxygen icon theme, in case it is not installed
        QVERIFY(testIconsDir.mkpath(QStringLiteral("oxygen/22x22/actions")));
        QVERIFY(testIconsDir.mkpath(QStringLiteral("oxygen/22x22/animations")));
        QVERIFY(testIconsDir.mkpath(QStringLiteral("oxygen/22x22/apps")));
        QVERIFY(testIconsDir.mkpath(QStringLiteral("oxygen/22x22/mimetypes")));
        QVERIFY(QFile::copy(QStringLiteral(":/oxygen.theme"), testIconsDir.filePath(QStringLiteral("oxygen/index.theme"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("oxygen/22x22/apps/kde.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/anim-22x22.png"), testIconsDir.filePath(QStringLiteral("oxygen/22x22/animations/process-working.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("oxygen/22x22/mimetypes/text-plain.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("oxygen/22x22/mimetypes/application-octet-stream.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("oxygen/22x22/mimetypes/image-x-generic.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("oxygen/22x22/mimetypes/video-x-generic.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("oxygen/22x22/mimetypes/x-office-document.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("oxygen/22x22/mimetypes/audio-x-generic.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("oxygen/22x22/mimetypes/unknown.png"))));

        // set up a minimal Breeze icon theme, fallback to breeze
        QVERIFY(testIconsDir.mkpath(QStringLiteral("breeze/22x22/actions")));
        QVERIFY(testIconsDir.mkpath(QStringLiteral("breeze/22x22/animations")));
        QVERIFY(testIconsDir.mkpath(QStringLiteral("breeze/22x22/apps")));
        QVERIFY(testIconsDir.mkpath(QStringLiteral("breeze/22x22/mimetypes")));
        QVERIFY(testIconsDir.mkpath(QStringLiteral("breeze/22x22/appsNoContext")));
        QVERIFY(testIconsDir.mkpath(QStringLiteral("breeze/22x22/appsNoType")));
        QVERIFY(testIconsDir.mkpath(QStringLiteral("breeze/22x22/appsNoContextOrType")));
        QVERIFY(QFile::copy(QStringLiteral(":/breeze.theme"), testIconsDir.filePath(QStringLiteral("breeze/index.theme"))));
        //kde.png is missing, it should fallback to oxygen
        //QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("breeze/22x22/apps/kde.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("breeze/22x22/appsNoContext/iconindirectorywithoutcontext.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("breeze/22x22/appsNoType/iconindirectorywithouttype.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("breeze/22x22/appsNoContextOrType/iconindirectorywithoutcontextortype.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/anim-22x22.png"), testIconsDir.filePath(QStringLiteral("breeze/22x22/animations/process-working.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("breeze/22x22/mimetypes/text-plain.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("breeze/22x22/mimetypes/application-octet-stream.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("breeze/22x22/mimetypes/image-x-generic.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("breeze/22x22/mimetypes/video-x-generic.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("breeze/22x22/mimetypes/x-office-document.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("breeze/22x22/mimetypes/audio-x-generic.png"))));
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), testIconsDir.filePath(QStringLiteral("breeze/22x22/mimetypes/unknown.png"))));
    }

    void cleanupTestCase()
    {
        testIconsDir.removeRecursively();
        appDataDir.removeRecursively();
    }

    void init()
    {
        // Remove icon cache
        const QString cacheFile = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/icon-cache.kcache";
        QFile::remove(cacheFile);

        // Clear SHM cache
        KIconLoader::global()->reconfigure(QString());
    }

    void testUnknownIconNotCached()
    {
        // This is a test to ensure that "unknown" icons do not pin themselves
        // in the icon loader. Or in other words, if an "unknown" icon is
        // returned, but the appropriate icon is subsequently installed
        // properly, the next request for that icon should return the new icon
        // instead of the unknown icon.

        QString actionIconsSubdir = QStringLiteral("oxygen/22x22/actions");
        QVERIFY(testIconsDir.mkpath(actionIconsSubdir));
        QString actionIconsDir = testIconsDir.filePath(actionIconsSubdir);

        QString nonExistingIconName = QStringLiteral("fhqwhgads_homsar");
        QString newIconPath = actionIconsDir + QLatin1String("/")
                              + nonExistingIconName + QLatin1String(".png");
        QFile::remove(newIconPath);

        KIconLoader iconLoader;

        // Find a non-existent icon, allowing unknown icon to be returned
        QPixmap nonExistingIcon = iconLoader.loadIcon(
                                      nonExistingIconName, KIconLoader::Toolbar);
        QCOMPARE(nonExistingIcon.isNull(), false);

        // Install the existing icon by copying.
        QVERIFY(QFile::copy(QStringLiteral(":/test-22x22.png"), newIconPath));

        // Verify the icon can now be found.
        QPixmap nowExistingIcon = iconLoader.loadIcon(
                                      nonExistingIconName, KIconLoader::Toolbar);
        QVERIFY(nowExistingIcon.cacheKey() != nonExistingIcon.cacheKey());
        QCOMPARE(iconLoader.iconPath(nonExistingIconName, KIconLoader::Toolbar),
                 newIconPath);
    }

    void testLoadIconCanReturnNull()
    {
        // This is a test for the "canReturnNull" argument of KIconLoader::loadIcon().
        // We try to load an icon that doesn't exist, first with canReturnNull=false (the default)
        // then with canReturnNull=true.
        KIconLoader iconLoader;
        // We expect a warning here... This doesn't work though, due to the extended debug
        //QTest::ignoreMessage(QtWarningMsg, "KIconLoader::loadIcon: No such icon \"this-icon-does-not-exist\"");
        QPixmap pix = iconLoader.loadIcon("this-icon-does-not-exist", KIconLoader::Desktop, 16);
        QVERIFY(!pix.isNull());
        QCOMPARE(pix.size(), QSize(16, 16));
        // Try it again, to see if the cache interfers
        pix = iconLoader.loadIcon("this-icon-does-not-exist", KIconLoader::Desktop, 16);
        QVERIFY(!pix.isNull());
        QCOMPARE(pix.size(), QSize(16, 16));
        // And now set canReturnNull to true
        pix = iconLoader.loadIcon("this-icon-does-not-exist", KIconLoader::Desktop, 16, KIconLoader::DefaultState,
                                  QStringList(), 0, true);
        QVERIFY(pix.isNull());
        // Try getting the "unknown" icon again, to see if the above call didn't put a null icon into the cache...
        pix = iconLoader.loadIcon("this-icon-does-not-exist", KIconLoader::Desktop, 16);
        QVERIFY(!pix.isNull());
        QCOMPARE(pix.size(), QSize(16, 16));

        // Another one, to clear "last" cache
        pix = iconLoader.loadIcon("this-icon-does-not-exist-either", KIconLoader::Desktop, 16);
        QVERIFY(!pix.isNull());
        QCOMPARE(pix.size(), QSize(16, 16));

        // Now load the initial one again - do we get the warning again?
        pix = iconLoader.loadIcon("this-icon-does-not-exist", KIconLoader::Desktop, 16);
        QVERIFY(!pix.isNull());
        QCOMPARE(pix.size(), QSize(16, 16));

        pix = iconLoader.loadIcon("#crazyname", KIconLoader::NoGroup, 1600);
        QVERIFY(!pix.isNull());
        QCOMPARE(pix.size(), QSize(1600, 1600));
    }

    void testAppPicsDir()
    {
        KIconLoader appIconLoader(appName);
        QString iconPath = appIconLoader.iconPath("image1", KIconLoader::User);
        QCOMPARE(iconPath, appDataDir.filePath("pics/image1.png"));
        QVERIFY(QFile::exists(iconPath));

        // Load it again, to use the "last loaded" cache
        QString iconPath2 = appIconLoader.iconPath("image1", KIconLoader::User);
        QCOMPARE(iconPath, iconPath2);
        // Load something else, to clear the "last loaded" cache
        QString iconPathTextEdit = appIconLoader.iconPath("image2", KIconLoader::User);
        QCOMPARE(iconPathTextEdit, appDataDir.filePath("pics/image2.png"));
        QVERIFY(QFile::exists(iconPathTextEdit));
        // Now load kdialog again, to use the real kiconcache
        iconPath2 = appIconLoader.iconPath("image1", KIconLoader::User);
        QCOMPARE(iconPath, iconPath2);
    }

    void testAppPicsDir_KDE_icon()
    {
        // #### This test is broken; it passes even if appName is set to foobar, because
        // QIcon::pixmap returns an unknown icon if it can't find the real icon...
        KIconLoader appIconLoader(appName);
        // Now using KDE::icon. Separate test so that KIconLoader isn't fully inited.
        QIcon icon = KDE::icon("image1", &appIconLoader);
        {
            QPixmap pix = icon.pixmap(QSize(22, 22));
            QVERIFY(!pix.isNull());
        }
        QCOMPARE(icon.actualSize(QSize(96, 22)), QSize(22, 22));
        QCOMPARE(icon.actualSize(QSize(22, 96)), QSize(22, 22));
        QCOMPARE(icon.actualSize(QSize(22, 16)), QSize(16, 16));

        // Can we ask for a really small size?
        {
            QPixmap pix8 = icon.pixmap(QSize(8, 8));
            QCOMPARE(pix8.size(), QSize(8, 8));
        }
    }

    void testLoadMimeTypeIcon_data()
    {
        QTest::addColumn<QString>("iconName");
        QTest::addColumn<QString>("expectedFileName");

        QTest::newRow("existing icon") << "text-plain" << "text-plain.png";
        QTest::newRow("octet-stream icon") << "application-octet-stream" << "application-octet-stream.png";
        QTest::newRow("non-existing icon") << "foo-bar" << "application-octet-stream.png";
        // Test this again, because now we won't go into the "fast path" of loadMimeTypeIcon anymore.
        QTest::newRow("existing icon again") << "text-plain" << "text-plain.png";
        QTest::newRow("generic fallback") << "image-foo-bar" << "image-x-generic.png";
        QTest::newRow("video generic fallback") << "video-foo-bar" << "video-x-generic.png";
        QTest::newRow("image-x-generic itself") << "image-x-generic" << "image-x-generic.png";
        QTest::newRow("x-office-document icon") << "x-office-document" << "x-office-document.png";
        QTest::newRow("unavailable generic icon") << "application/x-font-vfont" << "application-octet-stream.png";
        QTest::newRow("#184852") << "audio/x-tuxguitar" << "audio-x-generic.png";
        QTest::newRow("#178847") << "image/x-compressed-xcf" << "image-x-generic.png";
        QTest::newRow("mimetype generic icon") << "application-x-fluid" << "x-office-document.png";
    }

    void testLoadMimeTypeIcon()
    {
        QFETCH(QString, iconName);
        QFETCH(QString, expectedFileName);
        KIconLoader iconLoader;
        QString path;
        QPixmap pix = iconLoader.loadMimeTypeIcon(iconName, KIconLoader::Desktop, 24,
                      KIconLoader::DefaultState, QStringList(),
                      &path);
        QVERIFY(!pix.isNull());
        QCOMPARE(path.section('/', -1), expectedFileName);

        // do the same test using a global iconloader, so that
        // we get into the final return statement, which can only happen
        // if d->extraDesktopIconsLoaded becomes true first....
        QString path2;
        pix = KIconLoader::global()->loadMimeTypeIcon(iconName, KIconLoader::Desktop, 24,
                KIconLoader::DefaultState, QStringList(),
                &path2);
        QVERIFY(!pix.isNull());
        QCOMPARE(path2, path);
    }

    void testHasIcon()
    {
        QVERIFY(KIconLoader::global()->hasIcon("kde"));
        QVERIFY(KIconLoader::global()->hasIcon("kde"));
        QVERIFY(KIconLoader::global()->hasIcon("process-working"));
        QVERIFY(!KIconLoader::global()->hasIcon("no-such-icon-exists"));
    }

    void testIconPath()
    {
        // Test iconPath with non-existing icon
        const QString path = KIconLoader::global()->iconPath("nope-no-such-icon", KIconLoader::Desktop, true /*canReturnNull*/);
        QVERIFY2(path.isEmpty(), qPrintable(path));

        const QString unknownPath = KIconLoader::global()->iconPath("nope-no-such-icon", KIconLoader::Desktop, false);
        QVERIFY(!unknownPath.isEmpty());
        QVERIFY(QFile::exists(unknownPath));
    }

    void testPathStore()
    {
        QString path;
        KIconLoader::global()->loadIcon("kde", KIconLoader::Desktop, 24,
                                        KIconLoader::DefaultState, QStringList(),
                                        &path);
        QVERIFY(!path.isEmpty());
        QVERIFY(QFile::exists(path));

        // Compare with iconPath()
        QString path2 = KIconLoader::global()->iconPath("kde", KIconLoader::Desktop);
        QCOMPARE(path2, path);

        path = QString();
        KIconLoader::global()->loadIcon("does_not_exist", KIconLoader::Desktop, 24,
                                        KIconLoader::DefaultState, QStringList(),
                                        &path, true /* canReturnNull */);
        QVERIFY2(path.isEmpty(), qPrintable(path));

        path = "some filler to check loadIcon() clears the variable";
        KIconLoader::global()->loadIcon("does_not_exist", KIconLoader::Desktop, 24,
                                        KIconLoader::DefaultState, QStringList(),
                                        &path, true /* canReturnNull */);
        QVERIFY2(path.isEmpty(), qPrintable(path));

        //Test that addAppDir doesn't break loading of icons from the old known paths
        KIconLoader loader;
        //only addAppDir
        loader.addAppDir("kiconloader_unittest");
        path = QString();
        loader.loadIcon("kde", KIconLoader::Desktop, 24,
                                        KIconLoader::DefaultState, QStringList(),
                                        &path);
        QVERIFY(!path.isEmpty());

        path = QString();
        loader.loadIcon("image1", KIconLoader::Desktop, 24,
                                        KIconLoader::DefaultState, QStringList(),
                                        &path);
        QVERIFY(!path.isEmpty());

        //only reconfigure
        KIconLoader loader2;
        path = QString();
        loader2.reconfigure("kiconloader_unittest");
        loader2.loadIcon("kde", KIconLoader::Desktop, 24,
                                        KIconLoader::DefaultState, QStringList(),
                                        &path);
        QVERIFY(!path.isEmpty());
        loader2.loadIcon("image1", KIconLoader::Desktop, 24,
                                        KIconLoader::DefaultState, QStringList(),
                                        &path);
        QVERIFY(!path.isEmpty());

        //both addAppDir and reconfigure
        KIconLoader loader3;
        path = QString();
        loader3.addAppDir("kiconloader_unittest");
        loader3.reconfigure("kiconloader_unittest");
        loader3.loadIcon("kde", KIconLoader::Desktop, 24,
                                        KIconLoader::DefaultState, QStringList(),
                                        &path);
        QVERIFY(!path.isEmpty());

        path = QString();
        loader3.loadIcon("image1", KIconLoader::Desktop, 24,
                                        KIconLoader::DefaultState, QStringList(),
                                        &path);
        QVERIFY(!path.isEmpty());
    }

    void testPathsNoContextType() {
        {
            QString path;
            KIconLoader::global()->loadIcon("iconindirectorywithoutcontext", KIconLoader::Desktop, 24,
                                        KIconLoader::DefaultState, QStringList(),
                                        &path);
            QVERIFY(path.endsWith("appsNoContext/iconindirectorywithoutcontext.png"));
        }
        {
            QString path;
            KIconLoader::global()->loadIcon("iconindirectorywithouttype", KIconLoader::Desktop, 24,
                                        KIconLoader::DefaultState, QStringList(),
                                        &path);
            QVERIFY(path.endsWith("appsNoType/iconindirectorywithouttype.png"));
        }
        {
            QString path;
            KIconLoader::global()->loadIcon("iconindirectorywithoutcontextortype", KIconLoader::Desktop, 24,
                                        KIconLoader::DefaultState, QStringList(),
                                        &path);
            QVERIFY(path.endsWith("appsNoContextOrType/iconindirectorywithoutcontextortype.png"));
        }
    }

    void testLoadIconNoGroupOrSize() // #246016
    {
        QPixmap pix = KIconLoader::global()->loadIcon("connected", KIconLoader::NoGroup);
        QVERIFY(!pix.isNull());
    }

    void testLoadPixmapSequence()
    {
        KPixmapSequence seq =  KIconLoader::global()->loadPixmapSequence("process-working", 22);
        QVERIFY(seq.isValid());
    }
};

QTEST_MAIN(KIconLoader_UnitTest)

#include "kiconloader_unittest.moc"
