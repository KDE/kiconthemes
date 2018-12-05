/******************************************************************************
 *  Copyright 2014 Harald Fernengel <harry@kdevelop.org>                      *
 *                                                                            *
 *  This program is free software; you can redistribute it and/or             *
 *  modify it under the terms of the GNU Lesser General Public                *
 *                                                                            *
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

/* This tool converts an svg to a Mac OS X icns file.
 * Note: Requires the 'iconutil' Mac OS X binary
 */

#include <QCommandLineParser>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <QCoreApplication>
#include <QPainter>
#include <QSvgRenderer>

#include "../../../kiconthemes_version.h"

#include <stdio.h>

#define EXIT_ON_ERROR(isOk, ...) \
    do { \
        if (!(isOk)) { \
            fprintf(stderr, __VA_ARGS__); \
            return 1; \
        } \
    } while (false);

static bool writeImage(QSvgRenderer &svg, int size, const QString &outFile1, const QString &outFile2 = QString())
{
    QImage out(size, size, QImage::Format_ARGB32);
    out.fill(Qt::transparent);

    QPainter painter(&out);
    svg.render(&painter);
    painter.end();

    if (!out.save(outFile1)) {
        fprintf(stderr, "Unable to write %s\n", qPrintable(outFile1));
        return false;
    }
    if (!outFile2.isEmpty() && !out.save(outFile2)) {
        fprintf(stderr, "Unable to write %s\n", qPrintable(outFile2));
        return false;
    }

    return true;
}

int main(int argc, char *argv[])
{
    // it turns out we don't actually need to be a QGuiApplication in order
    // to use QPainter on a QImage. Downgrading to a QCoreApplication should
    // make us usable on headless (CI) servers and remove some QPA-related
    // runtime overhead.
    // Alternatively we create a QGuiApplication after setting QT_QPA_PLATFORM:
    // qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("minimal"));
    QCoreApplication app(argc, argv);

    app.setApplicationName("ksvg2icns");
    app.setApplicationVersion(KICONTHEMES_VERSION_STRING);
    QCommandLineParser parser;
    parser.setApplicationDescription(app.translate("main", "Creates an icns file from an svg image"));
    parser.addPositionalArgument("iconname", app.translate("main", "The svg icon to convert"));
    parser.addHelpOption();

    parser.process(app);
    if (parser.positionalArguments().isEmpty()) {
        parser.showHelp();
        return 1;
    }

    bool isOk;

    // create a temporary dir to create an iconset
    QTemporaryDir tmpDir("ksvg2icns");
    tmpDir.setAutoRemove(true);

    isOk = tmpDir.isValid();
    EXIT_ON_ERROR(isOk, "Unable to create temporary directory\n");

    isOk = QDir(tmpDir.path()).mkdir("out.iconset");
    EXIT_ON_ERROR(isOk, "Unable to create out.iconset directory\n");

    const QString outPath = tmpDir.path() + "/out.iconset";

    const QStringList &args = app.arguments();
    EXIT_ON_ERROR(args.size() == 2,
                  "Usage: %s svg-image\n",
                  qPrintable(args.value(0)));

    const QString &svgFileName = args.at(1);

    // open the actual svg file
    QSvgRenderer svg;
    isOk = svg.load(svgFileName);
    EXIT_ON_ERROR(isOk, "Unable to load %s\n", qPrintable(svgFileName));

    // The sizes are from:
    // https://developer.apple.com/library/mac/documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/Optimizing/Optimizing.html

    struct OutFiles
    {
        int size;
        QString out1;
        QString out2;
    };

    // create the pngs in various resolutions
    const OutFiles outFiles[] = {
        { 1024, outPath + "/icon_512x512@2x.png", QString() },
        {  512, outPath + "/icon_512x512.png",    outPath + "/icon_256x256@2x.png" },
        {  256, outPath + "/icon_256x256.png",    outPath + "/icon_128x128@2x.png" },
        {  128, outPath + "/icon_128x128.png",    QString() },
        {   64, outPath + "/icon_32x32@2x.png",   QString() },
        {   32, outPath + "/icon_32x32.png",       outPath + "/icon_16x16@2x.png" },
        {   16, outPath + "/icon_16x16.png",       QString() }
    };

    for (size_t i = 0; i < sizeof(outFiles) / sizeof(OutFiles); ++i) {
        isOk = writeImage(svg, outFiles[i].size, outFiles[i].out1, outFiles[i].out2);
        if (!isOk) {
            return 1;
        }
    }

    // convert the iconset to icns using the "iconutil" command

    const QString outIcns = QFileInfo(svgFileName).baseName() + ".icns";

    const QStringList utilArgs = QStringList()
            << "-c" << "icns"
            << "-o" << outIcns
            << outPath;

    QProcess iconUtil;
    iconUtil.setProgram("iconUtil");
    iconUtil.setArguments(utilArgs);

    iconUtil.start("iconutil", utilArgs, QIODevice::ReadOnly);
    isOk = iconUtil.waitForFinished(-1);
    EXIT_ON_ERROR(isOk, "Unable to launch iconutil: %s\n", qPrintable(iconUtil.errorString()));

    EXIT_ON_ERROR(iconUtil.exitStatus() == QProcess::NormalExit, "iconutil crashed!\n");
    EXIT_ON_ERROR(iconUtil.exitCode() == 0, "iconutil returned %d\n", iconUtil.exitCode());

    return 0;
}
