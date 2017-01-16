/*
 * Copyright 2014 Alex Merry <alex.merry@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <kicondialog.h>

#include <QApplication>
#include <QTextStream>

#include <iostream>

class Listener : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void dialog1Done()
    {
        KIconDialog *dialog = new KIconDialog();
        dialog->setup(KIconLoader::Toolbar, KIconLoader::Action);
        QString icon = dialog->openDialog();
        QTextStream(stdout) << "Icon \"" << icon << "\" was chosen (openDialog)\n";
        delete dialog;

        icon = KIconDialog::getIcon(KIconLoader::Desktop, KIconLoader::MimeType,
                true, 48, true, nullptr, QStringLiteral("Test dialog"));
        QTextStream(stdout) << "Icon \"" << icon << "\" was chosen (getIcon)\n";
    }

    void iconChosen(const QString &name)
    {
        QTextStream(stdout) << "Icon \"" << name << "\" was chosen (showDialog).\n";
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    Listener listener;

    KIconDialog dialog;
    QObject::connect(&dialog, &KIconDialog::newIconName,
                     &listener, &Listener::iconChosen);
    QObject::connect(&dialog, &QDialog::finished,
                     &listener, &Listener::dialog1Done);

    dialog.showDialog();

    return app.exec();
}

#include "kicondialogtest.moc"

