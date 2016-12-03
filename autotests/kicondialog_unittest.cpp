/* This file is part of the KDE libraries
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

#include <kicondialog.h>

#include <QStandardPaths>
#include <QPixmap>
#include <QTest>

#include <KSharedConfig>
#include <KConfigGroup>

class KIconDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);

        // set our test theme only present in :/icons
        KConfigGroup cg(KSharedConfig::openConfig(), "Icons");
        cg.writeEntry("Theme", "themeinresource");
        cg.sync();
    }

    void testConstructor()
    {
        // At least we test that we don't hit any assert
        KIconDialog iconDialog;
    }
};

QTEST_MAIN(KIconDialogTest)

#include "kicondialog_unittest.moc"
