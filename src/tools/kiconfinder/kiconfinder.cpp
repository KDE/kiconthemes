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

#include <kcmdlineargs.h>
#include <kiconloader.h>
#include <kdeversion.h>
#include <klocale.h>
#include <stdio.h>
#include <kapplication.h>

int main(int argc, char *argv[])
{
    KCmdLineArgs::init( argc, argv, "kiconfinder", 0, ki18n("Icon Finder"), KDE_VERSION_STRING , ki18n("Finds an icon based on its name"));


    KCmdLineOptions options;

    options.add("+iconname", ki18n("The icon name to look for"));

    KCmdLineArgs::addCmdLineOptions( options );

    KComponentData instance("kiconfinder");

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
    if( args->count() < 1 ) {
        printf( "No icon name specified\n" );
        return 1;
    }
    const QString iconName = args->arg( 0 );
    const QString icon = KIconLoader::global()->iconPath(iconName, KIconLoader::Desktop /*TODO configurable*/, true);
    if ( !icon.isEmpty() ) {
        printf("%s\n", icon.toLatin1().constData());
    } else {
        return 1; // error
    }

    return 0;
}
