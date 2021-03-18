/*
    SPDX-FileCopyrightText: 2021 Volker Krause <vkrause@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QStandardPaths>
#include <QTest>

#include <KIconEngine>
#include <KIconLoader>

class KIconEngine_Scaled_UnitTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
        QStandardPaths::setTestModeEnabled(true);
    }

    void testCenterIcon()
    {
        QIcon icon(new KIconEngine(QStringLiteral(":/test-22x22.png"), KIconLoader::global()));
        QVERIFY(!icon.isNull());

        QWindow w;
        QCOMPARE(w.devicePixelRatio(), 2.0);
        auto image = icon.pixmap(&w, QSize(22, 22)).toImage();
        QCOMPARE(image.devicePixelRatio(), 2.0);
        QCOMPARE(image.size(), QSize(44, 44));

        QImage unscaled;
        QVERIFY(unscaled.load(QStringLiteral(":/test-22x22.png")));
        QVERIFY(!unscaled.isNull());
        QCOMPARE(unscaled.size(), QSize(22, 22));
        unscaled.setDevicePixelRatio(2.0);
        unscaled = unscaled.convertToFormat(image.format()).scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QCOMPARE(image, unscaled);

        // center vertically
        QVERIFY(icon.pixmap(&w, QSize(22, 26)).toImage().copy(0, 4, 44, 44) == image);
        // center horizontally
        QVERIFY(icon.pixmap(&w, QSize(26, 22)).toImage().copy(4, 0, 44, 44) == image);
    }
};

QTEST_MAIN(KIconEngine_Scaled_UnitTest)

#include "kiconengine_scaled_unittest.moc"
