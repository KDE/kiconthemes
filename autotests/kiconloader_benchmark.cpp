/* This file is part of the KDE libraries
    Copyright 2016 Aleix Pol Gonzalez

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
#include <kiconengine.h>

#include <QStandardPaths>
#include <QTest>

class KIconLoader_Benchmark : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void init() {
#if 0 // Enable this code to benchmark very first startup.
      // Starting the application again uses the on-disk cache, so actually benchmarking -with- a cache is more relevant.
        // Remove icon cache
        const QString cacheFile = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/icon-cache.kcache";
        QFile::remove(cacheFile);

        // Clear SHM cache
        KIconLoader::global()->reconfigure(QString());
#endif
    }

    void benchmarkExistingIcons() {
        //icon list I get to load kwrite
        static QStringList icons = {
            QStringLiteral("accessories-text-editor"),
            QStringLiteral("bookmarks"),
            QStringLiteral("dialog-close"),
            QStringLiteral("edit-cut"),
            QStringLiteral("edit-paste"),
            QStringLiteral("edit-copy"),
            QStringLiteral("document-save"),
            QStringLiteral("edit-undo"),
            QStringLiteral("edit-redo"),
            QStringLiteral("code-context"),
            QStringLiteral("document-print"),
            QStringLiteral("document-print-preview"),
            QStringLiteral("view-refresh"),
            QStringLiteral("document-save-as"),
            QStringLiteral("preferences-other"),
            QStringLiteral("edit-select-all"),
            QStringLiteral("zoom-in"),
            QStringLiteral("zoom-out"),
            QStringLiteral("edit-find"),
            QStringLiteral("go-down-search"),
            QStringLiteral("go-up-search"),
            QStringLiteral("tools-check-spelling"),
            QStringLiteral("bookmark-new"),
            QStringLiteral("format-indent-more"),
            QStringLiteral("format-indent-less"),
            QStringLiteral("text-plain"),
            QStringLiteral("go-up"),
            QStringLiteral("go-down"),
            QStringLiteral("dialog-ok"),
            QStringLiteral("dialog-cancel"),
            QStringLiteral("window-close"),
            QStringLiteral("document-new"),
            QStringLiteral("document-open"),
            QStringLiteral("document-open-recent"),
            QStringLiteral("window-new"),
            QStringLiteral("application-exit"),
            QStringLiteral("show-menu"),
            QStringLiteral("configure-shortcuts"),
            QStringLiteral("configure-toolbars"),
            QStringLiteral("help-contents"),
            QStringLiteral("help-contextual"),
            QStringLiteral("tools-report-bug"),
            QStringLiteral("preferences-desktop-locale"),
            QStringLiteral("kde")
        };

        QBENCHMARK {
            foreach (const QString &iconName, icons) {
                const QIcon icon = QIcon::fromTheme(iconName);
                if(icon.isNull())
                    QSKIP("missing icons");
                QVERIFY(!icon.pixmap(24, 24).isNull());
                //QVERIFY(!icon.pixmap(512, 512).isNull());
            }
        }
    }

    void benchmarkNonExistingIcon_notCached()
    {
        QBENCHMARK {
            // Remove icon cache
            const QString cacheFile = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/icon-cache.kcache");
            QFile::remove(cacheFile);
            // Clear SHM cache
            KIconLoader::global()->reconfigure(QString());

            QIcon icon(new KIconEngine(QStringLiteral("invalid-icon-name"), KIconLoader::global()));
            QVERIFY(icon.isNull());
            QVERIFY2(icon.name().isEmpty(), qPrintable(icon.name()));
            QVERIFY(!icon.pixmap(QSize(16, 16), QIcon::Normal).isNull());
        }
    }

    void benchmarkNonExistingIcon_cached()
    {
        QBENCHMARK {
            QIcon icon(new KIconEngine(QStringLiteral("invalid-icon-name"), KIconLoader::global()));
            QVERIFY(icon.isNull());
            QVERIFY2(icon.name().isEmpty(), qPrintable(icon.name()));
            QVERIFY(!icon.pixmap(QSize(16, 16), QIcon::Normal).isNull());
        }
    }
};

QTEST_MAIN(KIconLoader_Benchmark)

#include "kiconloader_benchmark.moc"
