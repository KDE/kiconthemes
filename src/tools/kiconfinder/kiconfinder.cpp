/*  This file is part of the KDE project
    Copyright (C) 2008 David Faure <faure@kde.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QGuiApplication>
#include <QCommandLineParser>
#include <kiconloader.h>
#include <../kiconthemes_version.h>

#include <stdio.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kiconfinder"));
    app.setApplicationVersion(QStringLiteral(KICONTHEMES_VERSION_STRING));
    QCommandLineParser parser;
    parser.setApplicationDescription(app.translate("main", "Finds an icon based on its name"));
    parser.addPositionalArgument(QStringLiteral("iconname"), app.translate("main", "The icon name to look for"));
    parser.addHelpOption();

    parser.process(app);
    if(parser.positionalArguments().isEmpty())
        parser.showHelp();

    for(const QString& iconName : parser.positionalArguments()) {
        const QString icon = KIconLoader::global()->iconPath(iconName, KIconLoader::Desktop /*TODO configurable*/, true);
        if ( !icon.isEmpty() ) {
            printf("%s\n", icon.toLocal8Bit().constData());
        } else {
            return 1; // error
        }
    }

    return 0;
}
