/*
    SPDX-FileCopyrightText: 2026 Friedrich W. H. Kossebau <kossebau@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KIconButton>

#include <QTest>

class KIconButtonTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testConstructor();
    void test_setIcon();
    void test_resetIcon();
    void test_setIconSize();
    void test_setButtonIconSize();
    void test_setStrictIconSize();
};

void KIconButtonTest::testConstructor()
{
    KIconButton iconButton;

    // check defaults
    QCOMPARE(iconButton.icon(), QString());
    QCOMPARE(iconButton.iconSize(), 0);
    QCOMPARE(iconButton.buttonIconSize(), 48);
    QCOMPARE(iconButton.strictIconSize(), false);
}

void KIconButtonTest::test_setIcon()
{
    KIconButton iconButton;

    // relying on default, check first
    QCOMPARE(iconButton.icon(), QString());

    // do action
    iconButton.setIcon(QStringLiteral("test"));

    // check result
    QCOMPARE(iconButton.icon(), QStringLiteral("test"));
}

void KIconButtonTest::test_resetIcon()
{
    KIconButton iconButton;

    // test only icon string is reset
    // prepare non-default property values
    iconButton.setIcon(QStringLiteral("test"));
    iconButton.setIconSize(42);
    iconButton.setButtonIconSize(23);
    iconButton.setStrictIconSize(true);

    // do action
    iconButton.resetIcon();

    // check result
    QCOMPARE(iconButton.icon(), QString());
    QCOMPARE(iconButton.iconSize(), 42);
    QCOMPARE(iconButton.buttonIconSize(), 23);
    QCOMPARE(iconButton.strictIconSize(), true);
}

void KIconButtonTest::test_setIconSize()
{
    KIconButton iconButton;

    // relying on default, check first
    QCOMPARE(iconButton.iconSize(), 0);
    QCOMPARE(iconButton.buttonIconSize(), 48);

    // do action
    iconButton.setIconSize(42);

    // check result
    QCOMPARE(iconButton.iconSize(), 42);
    QCOMPARE(iconButton.buttonIconSize(), 42);
}

void KIconButtonTest::test_setButtonIconSize()
{
    KIconButton iconButton;

    // relying on default, check first
    QCOMPARE(iconButton.iconSize(), 0);
    QCOMPARE(iconButton.buttonIconSize(), 48);

    // do action
    iconButton.setButtonIconSize(23);

    // check result
    QCOMPARE(iconButton.buttonIconSize(), 23);
    QCOMPARE(iconButton.iconSize(), 0);
}

void KIconButtonTest::test_setStrictIconSize()
{
    KIconButton iconButton;

    // relying on default, check first
    QCOMPARE(iconButton.strictIconSize(), false);

    // do action: changing to true
    iconButton.setStrictIconSize(true);

    // check result
    QCOMPARE(iconButton.strictIconSize(), true);

    // do action: changing back to false
    iconButton.setStrictIconSize(false);

    // check result
    QCOMPARE(iconButton.strictIconSize(), false);
}

QTEST_MAIN(KIconButtonTest)

#include "kiconbutton_unittest.moc"
