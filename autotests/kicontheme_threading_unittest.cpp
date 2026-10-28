/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 KDE Contributors

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <kicontheme.h>

#include <QStandardPaths>
#include <QTest>

#include <atomic>
#include <thread>
#include <vector>

class KIconTheme_Threading_UnitTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void cleanup()
    {
        KIconTheme::forceThemeForTests(QString()); // Reset override.
    }

    // KIconTheme keeps its caches in process-global Q_GLOBAL_STATICs
    // (_theme, _theme_list and _themeOverride) that are read and written without any locking, while
    // KIconLoader::global() is thread_local, so any thread that loads a themed icon
    // mutates them.
    // Concurrent KIconTheme::current() which do *_theme() = *_themeOverride()
    // can happen from several threads.
    void testConcurrentCurrentAndReconfigure()
    {
        KIconTheme::forceThemeForTests(QStringLiteral("breeze"));

        std::atomic_bool stop{false};
        std::atomic<quint64> iterations{0};

        auto reader = [&stop, &iterations]() {
            while (!stop.load(std::memory_order_relaxed)) {
                const QString theme = KIconTheme::current();
                Q_UNUSED(theme);
                iterations.fetch_add(1, std::memory_order_relaxed);
            }
        };
        auto writer = [&stop]() {
            QString name(1024, QLatin1Char('a'));
            while (!stop.load(std::memory_order_relaxed)) {
                name.fill(QLatin1Char('b'));
                KIconTheme::forceThemeForTests(name);
            }
        };
        auto clearer = [&stop]() {
            while (!stop.load(std::memory_order_relaxed)) {
                KIconTheme::reconfigure();
            }
        };

        std::vector<std::thread> threads;
        for (int i = 0; i < 6; ++i) {
            threads.emplace_back(reader);
        }
        for (int i = 0; i < 2; ++i) {
            threads.emplace_back(writer);
        }
        threads.emplace_back(clearer);

        std::this_thread::sleep_for(std::chrono::seconds(3));
        stop.store(true, std::memory_order_relaxed);

        for (auto &thread : threads) {
            thread.join();
        }

        qInfo() << "completed" << iterations.load() << "concurrent KIconTheme::current() calls";
    }
};

QTEST_GUILESS_MAIN(KIconTheme_Threading_UnitTest)

#include "kicontheme_threading_unittest.moc"
