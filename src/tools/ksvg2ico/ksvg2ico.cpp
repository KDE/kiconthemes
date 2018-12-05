/******************************************************************************
 *  Copyright (C) 2007 Ralf Habacker  <ralf.habacker@freenet.de>              *
 *  Copyright (C) 2017 René J.V. Bertin <rjvbertin@gmail.com>                 *
 *                                                                            *
 *  This program is free software; you can redistribute it and/or             *
 *  modify it under the terms of the GNU Lesser General Public                *
 *  License as published by the Free Software Foundation; either              *
 *  version 2.1 of the License, or (at your option) version 3, or any         *
 *  later version accepted by the membership of KDE e.V. (or its              *
 *  successor approved by the membership of KDE e.V.), which shall            *
 *  act as a proxy defined in Section 6 of version 3 of the license.          *
 *                                                                            *
 *  This library is distributed in the hope that it will be useful,           *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 *  Lesser General Public License for more details.                           *
 *                                                                            *
 *  You should have received a copy of the GNU Lesser General Public          *
 *  License along with this library.  If not, see                             *
 *  <http://www.gnu.org/licenses/>.                                           *
 *                                                                            *
 ******************************************************************************/

/*
   svg to ico format converter, adapted from KDEWin's sgv2ico tool.
 */

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QtDebug>

#include <QString>
#include <QStringList>
#include <QProcess>
#include <QFile>
#include <QTemporaryDir>

#include <QSvgRenderer>
#include <QImage>
#include <QPainter>

#include "../../../kiconthemes_version.h"
// load our private QtIcoHandler copy, adapted from Qt 5.7.1
#include "qicohandler.h"

bool verbose = false;
bool debug = false;

#include <stdio.h>

#define EXIT_ON_ERROR(isOk, ...) \
    do { \
        if (!(isOk)) { \
            fprintf(stderr, __VA_ARGS__); \
            return 1; \
        } \
    } while (false);

bool svg2png(QSvgRenderer &renderer, const QString &outFile, int width, int height, QVector<QImage> &imgList)
{
    QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
    img.fill(0);

    QPainter p(&img);
    renderer.render(&p);
    img.save(outFile, "PNG");
    imgList += img.convertToFormat(QImage::Format_ARGB32,   Qt::ColorOnly|Qt::DiffuseAlphaDither|Qt::AvoidDither);
    imgList += img.convertToFormat(QImage::Format_Indexed8, Qt::ColorOnly|Qt::DiffuseAlphaDither|Qt::AvoidDither);
    return true;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QString rcFileName = "";

    app.setApplicationName("ksvg2ico");
    app.setApplicationVersion(KICONTHEMES_VERSION_STRING);
    QCommandLineParser parser;
    parser.setApplicationDescription(app.translate("main", "Creates an ico file from an SVG image"));
    parser.addPositionalArgument("input.sgv[z]", app.translate("main", "The SVG icon to convert"));
    parser.addPositionalArgument("output.ico", app.translate("main", "The name of the resulting ico file"));
    const QCommandLineOption verboseOption(QStringLiteral("verbose"), QStringLiteral("print execution details"));
    const QCommandLineOption debugOption(QStringLiteral("debug"),
        QStringLiteral("print debugging information and don't delete temporary files"));
    const QCommandLineOption rcFileOption(QStringLiteral("rcfile"),
        QStringLiteral("generate the named rc file for the icon"),
        "rcfile", rcFileName);
    parser.addOption(verboseOption);
    parser.addOption(debugOption);
    parser.addOption(rcFileOption);
    parser.addHelpOption();
    parser.addVersionOption();

    parser.process(app);
    if (parser.positionalArguments().isEmpty()) {
        parser.showHelp();
        return 1;
    }

    bool isOk;

    if (parser.isSet(verboseOption)) {
        verbose = true;
    }
    if (parser.isSet(debugOption)) {
        debug = true;
    }

    QString svgFile = parser.positionalArguments().at(0);
    QString icoFile = parser.positionalArguments().at(1);
    if (parser.isSet(rcFileOption)) {
        rcFileName = parser.value(rcFileOption);
    }

    QString pngFile = icoFile;
    pngFile.replace(".ico", "-%1.png");

    // create a temporary dir to create an iconset
    QTemporaryDir tmpDir("ksvg2ico");
    tmpDir.setAutoRemove(!debug);

    isOk = tmpDir.isValid();
    EXIT_ON_ERROR(isOk, "Unable to create temporary directory\n");

    QSvgRenderer renderer;
    isOk = renderer.load(svgFile);
    EXIT_ON_ERROR(isOk, "Unable to load %s\n", qPrintable(svgFile));

    QVector<QImage> imgList;
    // generate PNG versions up to the largest size the ico format is guaranteed to handle:
    foreach (int d, QVector<int>({16, 32, 48, 64, 128, 256})) {
        const QString pngSize = tmpDir.path() + "/" + pngFile.arg(d);
        if (verbose) {
            qDebug() << "converting" << svgFile << "to" << pngSize;
        }
        svg2png(renderer, pngSize, d, d, imgList);
    }
    if (debug || verbose) {
        qWarning() << "Creating" << icoFile << "from:";
        foreach (const auto &img, imgList) {
            qWarning() << img;
        }
    }

    QFile f(icoFile);
    isOk = f.open(QIODevice::WriteOnly);
    EXIT_ON_ERROR(isOk, "Can not open %s for writing", qPrintable(icoFile));

    QtIcoHandler ico(&f);
    isOk = ico.write(imgList);
    f.close();
    EXIT_ON_ERROR(isOk, "Failure writing ico file %s\n", qPrintable(icoFile));

    if (!rcFileName.isEmpty()) {
        QFile rcFile(rcFileName);
        if (!rcFile.open(QIODevice::WriteOnly)) {
          EXIT_ON_ERROR(false, "Can not open %s for writing", qPrintable(rcFileName ));
        }
        QTextStream ts(&rcFile);
        ts << QString( "IDI_ICON1        ICON        DISCARDABLE    \"%1\"\n" ).arg(icoFile);
        rcFile.close();
    }
    return !isOk;
}
