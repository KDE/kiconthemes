/*
    SPDX-FileCopyrightText: 2014 Harald Fernengel <harry@kdevelop.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

/* This tool converts an svg to a Mac OS X icns file.
 * Note: Requires the 'iconutil' Mac OS X binary
 */

#include <QCommandLineParser>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include <QGuiApplication>
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
    QGuiApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("ksvg2icns"));
    app.setApplicationVersion(QStringLiteral(KICONTHEMES_VERSION_STRING));
    QCommandLineParser parser;
    parser.setApplicationDescription(app.translate("main", "Creates an icns file from an svg image"));
    parser.addPositionalArgument("iconname", app.translate("main", "The svg icon to convert"));
    QCommandLineOption pngOnly(QStringLiteral("legacy-png"), QStringLiteral("Only create pngs used by the legacy ecm code"));
    parser.addOption(pngOnly);
    parser.addHelpOption();

    parser.process(app);

    if (parser.positionalArguments().isEmpty()) {
        parser.showHelp(1);
    }

    bool isOk;

    // create a temporary dir to create an iconset
    QTemporaryDir tmpDir(QDir::tempPath() + QStringLiteral("/ksvg2icns"));

    isOk = tmpDir.isValid();
    EXIT_ON_ERROR(isOk, "Unable to create temporary directory\n");

    const QString outPath = tmpDir.filePath(QStringLiteral("out.iconset"));

    isOk = QDir(tmpDir.path()).mkpath(outPath);
    EXIT_ON_ERROR(isOk, "Unable to create out.iconset directory\n");

    const QStringList &args = parser.positionalArguments();
    if(args.size() != 1) {
        parser.showHelp(1);
    }
    const QString &svgFileName = args.at(0);

    // open the actual svg file
    QSvgRenderer svg;
    isOk = svg.load(svgFileName);
    EXIT_ON_ERROR(isOk, "Unable to load %s\n", qPrintable(svgFileName));


    const QString baseName = QFileInfo(svgFileName).baseName();

    struct OutFiles
    {
        int size;
        QString out1;
        QString out2;
    };

    auto addLegacyIcon = [&](int size, bool createHDPI = false,  const QString &_infix = QString()) -> OutFiles {
        const QString path = parser.isSet(pngOnly) ? QStringLiteral(".") : outPath;
        const QString infix = _infix.isEmpty() ? QString() : QStringLiteral("-%1").arg(_infix);
        return { size,
                QStringLiteral("%1/%2-%3%4.png").arg(path, QString::number(size), baseName,  infix),
                createHDPI ? QStringLiteral("%1/%2@2x-%3%4.png").arg(path, QString::number(size / 2), baseName, infix) : QString()
        };
    };

    // create the pngs in various resolutions
    QVector<OutFiles> outFiles;
    if (parser.isSet(pngOnly)) {
        // based on https://invent.kde.org/frameworks/extra-cmake-modules/-/blob/master/modules/ECMAddAppIcon.cmake
        const QString sidebar = QStringLiteral("sidebar");
        outFiles = {
            { addLegacyIcon(1024, false, sidebar) },
            { addLegacyIcon( 512, false, sidebar) },
            { addLegacyIcon( 256, false, sidebar) },
            { addLegacyIcon( 128, false, sidebar) },
            { addLegacyIcon(  64, false, sidebar) },
            { addLegacyIcon(  32, false, sidebar) },
            { addLegacyIcon(  16, false, sidebar) },
            { addLegacyIcon(1024, true) },
            { addLegacyIcon( 256, true) },
            { addLegacyIcon( 128)       },
            { addLegacyIcon(  64)       },
            { addLegacyIcon(  48)       },
            { addLegacyIcon(  32, true) },
            { addLegacyIcon(  24)       },
            { addLegacyIcon(  16)       }
        };
    } else {
#ifndef Q_OS_WIN
        // The sizes are from:
        // https://developer.apple.com/library/mac/documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/Optimizing/Optimizing.html
        outFiles= {
            { 1024, outPath + QStringLiteral("/icon_512x512@2x.png"), QString() },
            {  512, outPath + QStringLiteral("/icon_512x512.png"),    outPath + QStringLiteral("/icon_256x256@2x.png") },
            {  256, outPath + QStringLiteral("/icon_256x256.png"),    outPath + QStringLiteral("/icon_128x128@2x.png") },
            {  128, outPath + QStringLiteral("/icon_128x128.png"),    QString() },
            {   64, outPath + QStringLiteral("/icon_32x32@2x.png"),   QString() },
            {   32, outPath + QStringLiteral("/icon_32x32.png"),      outPath + QStringLiteral("/icon_16x16@2x.png") },
            {   16, outPath + QStringLiteral("/icon_16x16.png"),      QString() }
        };
#else
        // According to https://stackoverflow.com/a/40851713/2886832
        // Windows always chooses the first icon above 255px, all other ones will be ignored
        outFiles = {
            { addLegacyIcon(1024) },
            { addLegacyIcon( 128) },
            { addLegacyIcon(  64) },
            { addLegacyIcon(  48) },
            { addLegacyIcon(  32) },
            { addLegacyIcon(  24) },
            { addLegacyIcon(  16) }
        };
#endif
    }

    for (const OutFiles &outFile : outFiles) {
        isOk = writeImage(svg, outFile.size, outFile.out1, outFile.out2);
        if (!isOk) {
            return 1;
        }
    }

    if (!parser.isSet(pngOnly)) {
        // convert the iconset to icns using the "iconutil" command
#ifndef Q_OS_WIN
        const QString tool = QStringLiteral("iconutil");
        const QString outIcns = baseName + QStringLiteral(".icns");

        const QStringList utilArgs = {
            QStringLiteral("-c"), QStringLiteral("icns"),
            QStringLiteral("-o", outIcns,
            outPath
        };
#else
        const QString tool = QStringLiteral("icotool");
        const QString outIco = baseName + QStringLiteral(".ico");

        QStringList utilArgs = {
            QStringLiteral("-c"),
            QStringLiteral("-o"), outIco
        };
        for (const auto &f : outFiles) {
            if (f.size == 1024) {
                utilArgs << QStringLiteral("-r");
            }
            utilArgs << f.out1;
        }
#endif

        QProcess iconUtil;

        iconUtil.start(tool, utilArgs, QIODevice::ReadOnly);
        isOk = iconUtil.waitForFinished(-1);
        EXIT_ON_ERROR(isOk, "Unable to launch %s: %s\n", qPrintable(tool), qPrintable(iconUtil.errorString()));

        EXIT_ON_ERROR(iconUtil.exitStatus() == QProcess::NormalExit, "%s crashed!\n", qPrintable(tool));
        EXIT_ON_ERROR(iconUtil.exitCode() == 0, "%s returned %d\n", qPrintable(tool), iconUtil.exitCode());
    }


    return 0;
}
