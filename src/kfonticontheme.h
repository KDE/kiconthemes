#pragma once

#include <kiconthemes_export.h>

#include <QIcon>
#include <QList>
#include <QString>
#include <QStringList>

#include <QFileDevice>
#include <memory>

#include "kiconloader.h"

class QIODevice;
class KIconColors;

namespace FT
{
class Face;
}

/**
 * @internal
 * Class to use/access icon themes in KDE. This class is used by the
 * iconloader but can be used by others too.
 * @warning You should not use this class externally. This class is exported because
 *          the KCM needs it.
 * @see KIconLoader
 */
class KICONTHEMES_EXPORT KFontIconTheme
{
public:
    /**
     * A small object containing a FreeType error code and what KFontIconTheme was doing
     * before it ran into it
     */
    struct FreeTypeError {
        /** A short description of what KFontIconTheme was doing.
         */
        const char *context;
        /** The FT_Error code.
         */
        int code;
    };

    /**
     * Load a font icon theme from a QFileDevice.
     *
     * @returns KFontIconTheme on success, QFileDevice::FileError on IO error, int on Freetype error
     */
    static std::variant<std::unique_ptr<KFontIconTheme>, QFileDevice::FileError, FreeTypeError> initFromDevice(std::unique_ptr<QFileDevice> device);

    /**
     * The stylized name of the icon theme.
     * @return the (human-readable) name of the theme
     */
    QString name() const;

    /**
     * Query available icons.
     * @return the list of icon names
     */
    QStringList queryIcons() const;

    /**
     * Query if the font contains the given icon name.
     */
    bool containsIcon(const QString &iconName) const;

    /**
     * Create a pixmap for the given icon, if present.
     * Not thread-safe.
     *
     * @return QPixmap if successful, int on Freetype error
     */
    std::variant<QPixmap, FreeTypeError>
    createPixmapForIcon(const QString &iconName, const QSize &size, qreal scale, KIconColors colors, KIconLoader::States state);

private:
    KFontIconTheme(std::unique_ptr<FT::Face> &&face);
    std::unique_ptr<struct KFontIconThemePrivate> const d;
};
