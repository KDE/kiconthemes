/* vi: ts=8 sts=4 sw=4

    kiconloader.cpp: An icon loader for KDE with theming functionality.

    This file is part of the KDE project, module kdeui.
    SPDX-FileCopyrightText: 2000 Geert Jansen <jansen@kde.org>
    SPDX-FileCopyrightText: 2000 Antonio Larrosa <larrosa@kde.org>
    SPDX-FileCopyrightText: 2010 Michael Pyne <mpyne@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kiconloader.h"
#include "kiconloader_p.h"

// kdecore
#include <KConfigGroup>
#include <KSharedConfig>
#include <kshareddatacache.h>
#ifdef QT_DBUS_LIB
#include <QDBusConnection>
#include <QDBusMessage>
#endif
#include <QCryptographicHash>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

// kdeui
#include "debug.h"
#include "kiconcolors.h"
#include "kiconeffect.h"
#include "kicontheme.h"

// kwidgetsaddons
#include <KPixmapSequence>

#include <KColorScheme>
#include <KCompressionDevice>

#include <QBuffer>
#include <QByteArray>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QMovie>
#include <QPainter>
#include <QPixmap>
#include <QPixmapCache>
#include <QStringBuilder> // % operator for QString
#include <QtGui/private/qiconloader_p.h>

#include <qplatformdefs.h> //for readlink

#include <assert.h>

namespace
{
// Used to make cache keys for icons with no group. Result type is QString*
QString NULL_EFFECT_FINGERPRINT()
{
    return QStringLiteral("noeffect");
}

}

/**
 * Function to convert an uint32_t to AARRGGBB hex values.
 *
 * W A R N I N G !
 * This function is for internal use!
 */
KICONTHEMES_EXPORT void uintToHex(uint32_t colorData, QChar *buffer)
{
    static const char hexLookup[] = "0123456789abcdef";
    buffer += 7;
    uchar *colorFields = reinterpret_cast<uchar *>(&colorData);

    for (int i = 0; i < 4; i++) {
        *buffer-- = hexLookup[*colorFields & 0xf];
        *buffer-- = hexLookup[*colorFields >> 4];
        colorFields++;
    }
}

static QString paletteId(const KIconColors &colors)
{
    // 8 per color. We want 3 colors thus 8*4=32.
    QString buffer(32, Qt::Uninitialized);

    uintToHex(colors.text().rgba(), buffer.data());
    uintToHex(colors.highlight().rgba(), buffer.data() + 8);
    uintToHex(colors.highlightedText().rgba(), buffer.data() + 16);
    uintToHex(colors.background().rgba(), buffer.data() + 24);

    return buffer;
}

/*** KIconThemeNode: A node in the icon theme dependency tree. ***/

class KIconThemeNode
{
public:
    KIconThemeNode(KIconTheme *_theme);
    ~KIconThemeNode();

    KIconThemeNode(const KIconThemeNode &) = delete;
    KIconThemeNode &operator=(const KIconThemeNode &) = delete;

    void queryIcons(QStringList *lst, int size, KIconLoader::Context context) const;
    void queryIconsByContext(QStringList *lst, int size, KIconLoader::Context context) const;
    QString findIcon(const QString &name, int size, KIconLoader::MatchType match) const;

    KIconTheme *theme;
};

KIconThemeNode::KIconThemeNode(KIconTheme *_theme)
{
    theme = _theme;
}

KIconThemeNode::~KIconThemeNode()
{
    delete theme;
}

void KIconThemeNode::queryIcons(QStringList *result, int size, KIconLoader::Context context) const
{
    // add the icons of this theme to it
    *result += theme->queryIcons(size, context);
}

void KIconThemeNode::queryIconsByContext(QStringList *result, int size, KIconLoader::Context context) const
{
    // add the icons of this theme to it
    *result += theme->queryIconsByContext(size, context);
}

QString KIconThemeNode::findIcon(const QString &name, int size, KIconLoader::MatchType match) const
{
    return theme->iconPath(name, size, match);
}

extern KICONTHEMES_EXPORT int kiconloader_ms_between_checks;
KICONTHEMES_EXPORT int kiconloader_ms_between_checks = 5000;

class KIconLoaderGlobalData : public QObject
{
    Q_OBJECT

public:
    KIconLoaderGlobalData()
    {
        const QStringList genericIconsFiles = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("mime/generic-icons"));
        // qCDebug(KICONTHEMES) << genericIconsFiles;
        for (const QString &file : genericIconsFiles) {
            parseGenericIconsFiles(file);
        }

#ifdef QT_DBUS_LIB
        QDBusConnection::sessionBus().connect(QString(),
                                              QStringLiteral("/KIconLoader"),
                                              QStringLiteral("org.kde.KIconLoader"),
                                              QStringLiteral("iconChanged"),
                                              this,
                                              SIGNAL(iconChanged(int)));
#endif
    }

    void emitChange(KIconLoader::Group group)
    {
#ifdef QT_DBUS_LIB
        QDBusMessage message = QDBusMessage::createSignal(QStringLiteral("/KIconLoader"), QStringLiteral("org.kde.KIconLoader"), QStringLiteral("iconChanged"));
        message.setArguments(QList<QVariant>() << int(group));
        QDBusConnection::sessionBus().send(message);
#endif
    }

    QString genericIconFor(const QString &icon) const
    {
        return m_genericIcons.value(icon);
    }

Q_SIGNALS:
    void iconChanged(int group);

private:
    void parseGenericIconsFiles(const QString &fileName);
    QHash<QString, QString> m_genericIcons;
};

void KIconLoaderGlobalData::parseGenericIconsFiles(const QString &fileName)
{
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream stream(&file);
        // In Qt6 the encoding is UTF-8 by default, so it should work for icon file names;
        // I think this code had "ISO 8859-1" (i.e. Latin-1) as an optimization, but file
        // names on Linux are UTF-8 by default, so this would be more robust.
        // Note that in Qt6 we can have the same behaviour by using QTextStream::setEncoding().
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        stream.setCodec("ISO 8859-1");
#endif
        while (!stream.atEnd()) {
            const QString line = stream.readLine();
            if (line.isEmpty() || line[0] == QLatin1Char('#')) {
                continue;
            }
            const int pos = line.indexOf(QLatin1Char(':'));
            if (pos == -1) { // syntax error
                continue;
            }
            QString mimeIcon = line.left(pos);
            const int slashindex = mimeIcon.indexOf(QLatin1Char('/'));
            if (slashindex != -1) {
                mimeIcon[slashindex] = QLatin1Char('-');
            }

            const QString genericIcon = line.mid(pos + 1);
            m_genericIcons.insert(mimeIcon, genericIcon);
            // qCDebug(KICONTHEMES) << mimeIcon << "->" << genericIcon;
        }
    }
}

Q_GLOBAL_STATIC(KIconLoaderGlobalData, s_globalData)

KIconLoaderPrivate::KIconLoaderPrivate(const QString &_appname, const QStringList &extraSearchPaths, KIconLoader *qq)
    : q(qq)
    , m_appname(_appname)
{
    q->connect(s_globalData, &KIconLoaderGlobalData::iconChanged, q, [this](int group) {
        _k_refreshIcons(group);
    });
    init(m_appname, extraSearchPaths);
}

KIconLoaderPrivate::~KIconLoaderPrivate()
{
    clear();
}

KIconLoaderPrivate *KIconLoaderPrivate::get(KIconLoader *loader)
{
    return loader->d.get();
}

void KIconLoaderPrivate::clear()
{
    /* antlarr: There's no need to delete d->mpThemeRoot as it's already
    deleted when the elements of d->links are deleted */
    qDeleteAll(links);
    delete[] mpGroups;
    delete mIconCache;
    mpGroups = nullptr;
    mIconCache = nullptr;
    mPixmapCache.clear();
    m_appname.clear();
    searchPaths.clear();
    links.clear();
    mIconThemeInited = false;
    mThemesInTree.clear();
}

void KIconLoaderPrivate::drawOverlays(const KIconLoader *iconLoader, KIconLoader::Group group, int state, QPixmap &pix, const QStringList &overlays)
{
    if (overlays.isEmpty()) {
        return;
    }

    const int width = pix.size().width();
    const int height = pix.size().height();
    const int iconSize = qMin(width, height);
    int overlaySize;

    if (iconSize < 32) {
        overlaySize = 8;
    } else if (iconSize <= 48) {
        overlaySize = 16;
    } else if (iconSize <= 96) {
        overlaySize = 22;
    } else if (iconSize < 256) {
        overlaySize = 32;
    } else {
        overlaySize = 64;
    }

    QPainter painter(&pix);

    int count = 0;
    for (const QString &overlay : overlays) {
        // Ensure empty strings fill up a emblem spot
        // Needed when you have several emblems to ensure they're always painted
        // at the same place, even if one is not here
        if (overlay.isEmpty()) {
            ++count;
            continue;
        }

        // TODO: should we pass in the kstate? it results in a slower
        //      path, and perhaps emblems should remain in the default state
        //      anyways?
        QPixmap pixmap = iconLoader->loadIcon(overlay, group, overlaySize, state, QStringList(), nullptr, true);

        if (pixmap.isNull()) {
            continue;
        }

        // match the emblem's devicePixelRatio to the original pixmap's
        pixmap.setDevicePixelRatio(pix.devicePixelRatio());
        const int margin = pixmap.devicePixelRatio() * 0.05 * iconSize;

        QPoint startPoint;
        switch (count) {
        case 0:
            // bottom right corner
            startPoint = QPoint(width - overlaySize - margin, height - overlaySize - margin);
            break;
        case 1:
            // bottom left corner
            startPoint = QPoint(margin, height - overlaySize - margin);
            break;
        case 2:
            // top left corner
            startPoint = QPoint(margin, margin);
            break;
        case 3:
            // top right corner
            startPoint = QPoint(width - overlaySize - margin, margin);
            break;
        }

        startPoint /= pix.devicePixelRatio();

        painter.drawPixmap(startPoint, pixmap);

        ++count;
        if (count > 3) {
            break;
        }
    }
}

void KIconLoaderPrivate::_k_refreshIcons(int group)
{
    KSharedConfig::Ptr sharedConfig = KSharedConfig::openConfig();
    sharedConfig->reparseConfiguration();
    const QString newThemeName = sharedConfig->group("Icons").readEntry("Theme", QStringLiteral("breeze"));
    if (!newThemeName.isEmpty()) {
        // NOTE Do NOT use QIcon::setThemeName here it makes Qt not use icon engine of the platform theme
        //      anymore (KIconEngine on Plasma, which breaks recoloring) and overwrites a user set themeName
        // TODO KF6 this should be done in the Plasma QPT
        QIconLoader::instance()->updateSystemTheme();
    }

    q->newIconLoader();
    mIconAvailability.clear();
    Q_EMIT q->iconChanged(group);
}

bool KIconLoaderPrivate::shouldCheckForUnknownIcons()
{
    if (mLastUnknownIconCheck.isValid() && mLastUnknownIconCheck.elapsed() < kiconloader_ms_between_checks) {
        return false;
    }
    mLastUnknownIconCheck.start();
    return true;
}

KIconLoader::KIconLoader(const QString &appname, const QStringList &extraSearchPaths, QObject *parent)
    : QObject(parent)
    , d(new KIconLoaderPrivate(appname, extraSearchPaths, this))
{
    setObjectName(appname);
}

void KIconLoader::reconfigure(const QString &_appname, const QStringList &extraSearchPaths)
{
    d->mIconCache->clear();
    d->clear();
    d->init(_appname, extraSearchPaths);
}

void KIconLoaderPrivate::init(const QString &_appname, const QStringList &extraSearchPaths)
{
    extraDesktopIconsLoaded = false;
    mIconThemeInited = false;
    mpThemeRoot = nullptr;

    searchPaths = extraSearchPaths;

    m_appname = !_appname.isEmpty() ? _appname : QCoreApplication::applicationName();

    // Initialize icon cache
    mIconCache = new KSharedDataCache(QStringLiteral("icon-cache"), 10 * 1024 * 1024);
    // Cost here is number of pixels, not size. So this is actually a bit
    // smaller.
    mPixmapCache.setMaxCost(10 * 1024 * 1024);

    // These have to match the order in kiconloader.h
    static const char *const groups[] = {"Desktop", "Toolbar", "MainToolbar", "Small", "Panel", "Dialog", nullptr};
    KSharedConfig::Ptr config = KSharedConfig::openConfig();

    // loading config and default sizes
    initIconThemes();
    KIconTheme *defaultSizesTheme = links.empty() ? nullptr : links.first()->theme;
    mpGroups = new KIconGroup[static_cast<int>(KIconLoader::LastGroup)];
    for (KIconLoader::Group i = KIconLoader::FirstGroup; i < KIconLoader::LastGroup; ++i) {
        if (groups[i] == nullptr) {
            break;
        }

        KConfigGroup cg(config, QLatin1String(groups[i]) + QStringLiteral("Icons"));
        mpGroups[i].size = cg.readEntry("Size", 0);

        if (!mpGroups[i].size && defaultSizesTheme) {
            mpGroups[i].size = defaultSizesTheme->defaultSize(i);
        }
    }
}

void KIconLoaderPrivate::initIconThemes()
{
    if (mIconThemeInited) {
        return;
    }
    // qCDebug(KICONTHEMES);
    mIconThemeInited = true;

    // Add the default theme and its base themes to the theme tree
    KIconTheme *def = new KIconTheme(KIconTheme::current(), m_appname);
    if (!def->isValid()) {
        delete def;
        // warn, as this is actually a small penalty hit
        qCDebug(KICONTHEMES) << "Couldn't find current icon theme, falling back to default.";
        def = new KIconTheme(KIconTheme::defaultThemeName(), m_appname);
        if (!def->isValid()) {
            qCDebug(KICONTHEMES) << "Standard icon theme" << KIconTheme::defaultThemeName() << "not found!";
            delete def;
            return;
        }
    }
    mpThemeRoot = new KIconThemeNode(def);
    mThemesInTree.append(def->internalName());
    links.append(mpThemeRoot);
    addBaseThemes(mpThemeRoot, m_appname);

    // Insert application specific themes at the top.
    searchPaths.append(m_appname + QStringLiteral("/pics"));

    // Add legacy icon dirs.
    searchPaths.append(QStringLiteral("icons")); // was xdgdata-icon in KStandardDirs
    // These are not in the icon spec, but e.g. GNOME puts some icons there anyway.
    searchPaths.append(QStringLiteral("pixmaps")); // was xdgdata-pixmaps in KStandardDirs
}

KIconLoader::~KIconLoader() = default;

QStringList KIconLoader::searchPaths() const
{
    return d->searchPaths;
}

void KIconLoader::addAppDir(const QString &appname, const QString &themeBaseDir)
{
    d->searchPaths.append(appname + QStringLiteral("/pics"));
    d->addAppThemes(appname, themeBaseDir);
}

void KIconLoaderPrivate::addAppThemes(const QString &appname, const QString &themeBaseDir)
{
    KIconTheme *def = new KIconTheme(QStringLiteral("hicolor"), appname, themeBaseDir);
    if (!def->isValid()) {
        delete def;
        def = new KIconTheme(KIconTheme::defaultThemeName(), appname, themeBaseDir);
    }
    KIconThemeNode *node = new KIconThemeNode(def);
    bool addedToLinks = false;

    if (!mThemesInTree.contains(appname)) {
        mThemesInTree.append(appname);
        links.append(node);
        addedToLinks = true;
    }
    addBaseThemes(node, appname);

    if (!addedToLinks) {
        // Nodes in links are being deleted later - this one needs manual care.
        delete node;
    }
}

void KIconLoaderPrivate::addBaseThemes(KIconThemeNode *node, const QString &appname)
{
    // Quote from the icon theme specification:
    //   The lookup is done first in the current theme, and then recursively
    //   in each of the current theme's parents, and finally in the
    //   default theme called "hicolor" (implementations may add more
    //   default themes before "hicolor", but "hicolor" must be last).
    //
    // So we first make sure that all inherited themes are added, then we
    // add the KDE default theme as fallback for all icons that might not be
    // present in an inherited theme, and hicolor goes last.

    addInheritedThemes(node, appname);
    addThemeByName(QIcon::fallbackThemeName(), appname);
    addThemeByName(QStringLiteral("hicolor"), appname);
}

void KIconLoaderPrivate::addInheritedThemes(KIconThemeNode *node, const QString &appname)
{
    const QStringList inheritedThemes = node->theme->inherits();

    for (const auto &inheritedTheme : inheritedThemes) {
        if (inheritedTheme == QLatin1String("hicolor")) {
            // The icon theme spec says that "hicolor" must be the very last
            // of all inherited themes, so don't add it here but at the very end
            // of addBaseThemes().
            continue;
        }
        addThemeByName(inheritedTheme, appname);
    }
}

void KIconLoaderPrivate::addThemeByName(const QString &themename, const QString &appname)
{
    if (mThemesInTree.contains(themename + appname)) {
        return;
    }
    KIconTheme *theme = new KIconTheme(themename, appname);
    if (!theme->isValid()) {
        delete theme;
        return;
    }
    KIconThemeNode *n = new KIconThemeNode(theme);
    mThemesInTree.append(themename + appname);
    links.append(n);
    addInheritedThemes(n, appname);
}

void KIconLoaderPrivate::addExtraDesktopThemes()
{
    if (extraDesktopIconsLoaded) {
        return;
    }

    QStringList list;
    const QStringList icnlibs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons"), QStandardPaths::LocateDirectory);
    for (const auto &iconDir : icnlibs) {
        QDir dir(iconDir);
        if (!dir.exists()) {
            continue;
        }
        const auto defaultEntries = dir.entryInfoList(QStringList(QStringLiteral("default.*")), QDir::Dirs);
        for (const auto &defaultEntry : defaultEntries) {
            if (!QFileInfo::exists(defaultEntry.filePath() + QLatin1String("/index.desktop")) //
                && !QFileInfo::exists(defaultEntry.filePath() + QLatin1String("/index.theme"))) {
                continue;
            }
            if (defaultEntry.isSymbolicLink()) {
                const QString themeName = QDir(defaultEntry.symLinkTarget()).dirName();
                if (!list.contains(themeName)) {
                    list.append(themeName);
                }
            }
        }
    }

    for (const auto &theme : list) {
        // Don't add the KDE defaults once more, we have them anyways.
        if (theme == QLatin1String("default.kde") || theme == QLatin1String("default.kde4")) {
            continue;
        }
        addThemeByName(theme, QLatin1String(""));
    }

    extraDesktopIconsLoaded = true;
}

void KIconLoader::drawOverlays(const QStringList &overlays, QPixmap &pixmap, KIconLoader::Group group, int state) const
{
    d->drawOverlays(this, group, state, pixmap, overlays);
}

QString KIconLoaderPrivate::removeIconExtension(const QString &name) const
{
    if (name.endsWith(QLatin1String(".png")) //
        || name.endsWith(QLatin1String(".xpm")) //
        || name.endsWith(QLatin1String(".svg"))) {
        return name.left(name.length() - 4);
    } else if (name.endsWith(QLatin1String(".svgz"))) {
        return name.left(name.length() - 5);
    }

    return name;
}

void KIconLoaderPrivate::normalizeIconMetadata(KIconLoader::Group &group, QSize &size, int &state) const
{
    if ((state < 0) || (state >= KIconLoader::LastState)) {
        qCWarning(KICONTHEMES) << "Invalid icon state:" << state << ", should be one of KIconLoader::States";
        state = KIconLoader::DefaultState;
    }

    if (size.width() < 0 || size.height() < 0) {
        size = {};
    }

    // For "User" icons, bail early since the size should be based on the size on disk,
    // which we've already checked.
    if (group == KIconLoader::User) {
        return;
    }

    if ((group < -1) || (group >= KIconLoader::LastGroup)) {
        qCWarning(KICONTHEMES) << "Invalid icon group:" << group << ", should be one of KIconLoader::Group";
        group = KIconLoader::Desktop;
    }

    // If size == 0, use default size for the specified group.
    if (size.isNull()) {
        if (group < 0) {
            qWarning() << "Neither size nor group specified!";
            group = KIconLoader::Desktop;
        }
        size = QSize(mpGroups[group].size, mpGroups[group].size);
    }
}

QString KIconLoaderPrivate::makeCacheKey(const QString &name,
                                         KIconLoader::Group group,
                                         const QStringList &overlays,
                                         const QSize &size,
                                         qreal scale,
                                         int state,
                                         const KIconColors &colors) const
{
    // The KSharedDataCache is shared so add some namespacing. The following code
    // uses QStringBuilder (new in Qt 4.6)

    /* clang-format off */
    return (group == KIconLoader::User ? QLatin1String("$kicou_") : QLatin1String("$kico_"))
            % name
            % QLatin1Char('_')
            % (size.width() == size.height() ? QString::number(size.height()) : QString::number(size.height()) % QLatin1Char('x') % QString::number(size.width()))
            % QLatin1Char('@')
            % QString::number(scale, 'f', 1)
            % QLatin1Char('_')
            % overlays.join(QLatin1Char('_'))
            % (group >= 0 ? mpEffect.fingerprint(group, state) : NULL_EFFECT_FINGERPRINT())
            % QLatin1Char('_')
            % paletteId(colors)
            % (q->theme() && q->theme()->followsColorScheme() && state == KIconLoader::SelectedState ? QStringLiteral("_selected") : QString());
    /* clang-format on */
}

QByteArray KIconLoaderPrivate::processSvg(const QString &path, KIconLoader::States state, const KIconColors &colors) const
{
    std::unique_ptr<QIODevice> device;

    if (path.endsWith(QLatin1String("svgz"))) {
        device.reset(new KCompressionDevice(path, KCompressionDevice::GZip));
    } else {
        device.reset(new QFile(path));
    }

    if (!device->open(QIODevice::ReadOnly)) {
        return QByteArray();
    }

    const QString styleSheet = colors.stylesheet(state);
    QByteArray processedContents;
    QXmlStreamReader reader(device.get());

    QBuffer buffer(&processedContents);
    buffer.open(QIODevice::WriteOnly);
    QXmlStreamWriter writer(&buffer);
    while (!reader.atEnd()) {
        if (reader.readNext() == QXmlStreamReader::StartElement //
            && reader.qualifiedName() == QLatin1String("style") //
            && reader.attributes().value(QLatin1String("id")) == QLatin1String("current-color-scheme")) {
            writer.writeStartElement(QStringLiteral("style"));
            writer.writeAttributes(reader.attributes());
            writer.writeCharacters(styleSheet);
            writer.writeEndElement();
            while (reader.tokenType() != QXmlStreamReader::EndElement) {
                reader.readNext();
            }
        } else if (reader.tokenType() != QXmlStreamReader::Invalid) {
            writer.writeCurrentToken(reader);
        }
    }
    buffer.close();

    return processedContents;
}

QImage KIconLoaderPrivate::createIconImage(const QString &path, const QSize &size, qreal scale, KIconLoader::States state, const KIconColors &colors)
{
    // TODO: metadata in the theme to make it do this only if explicitly supported?
    QImageReader reader;
    QBuffer buffer;

    if (q->theme() && q->theme()->followsColorScheme() && (path.endsWith(QLatin1String("svg")) || path.endsWith(QLatin1String("svgz")))) {
        buffer.setData(processSvg(path, state, colors));
        reader.setDevice(&buffer);
    } else {
        reader.setFileName(path);
    }

    if (!reader.canRead()) {
        return QImage();
    }

    if (!size.isNull()) {
        // ensure we keep aspect ratio
        const QSize wantedSize = size * scale;
        QSize finalSize(reader.size());
        if (finalSize.isNull()) {
            // nothing to scale
            finalSize = wantedSize;
        } else {
            // like QSvgIconEngine::pixmap try to keep aspect ratio
            finalSize.scale(wantedSize, Qt::KeepAspectRatio);
        }
        reader.setScaledSize(finalSize);
    }

    return reader.read();
}

void KIconLoaderPrivate::insertCachedPixmapWithPath(const QString &key, const QPixmap &data, const QString &path = QString())
{
    // Even if the pixmap is null, we add it to the caches so that we record
    // the fact that whatever icon led to us getting a null pixmap doesn't
    // exist.

    QBuffer output;
    output.open(QIODevice::WriteOnly);

    QDataStream outputStream(&output);
    outputStream.setVersion(QDataStream::Qt_4_6);

    outputStream << path;

    // Convert the QPixmap to PNG. This is actually done by Qt's own operator.
    outputStream << data;

    output.close();

    // The byte array contained in the QBuffer is what we want in the cache.
    mIconCache->insert(key, output.buffer());

    // Also insert the object into our process-local cache for even more
    // speed.
    PixmapWithPath *pixmapPath = new PixmapWithPath;
    pixmapPath->pixmap = data;
    pixmapPath->path = path;

    mPixmapCache.insert(key, pixmapPath, data.width() * data.height() + 1);
}

bool KIconLoaderPrivate::findCachedPixmapWithPath(const QString &key, QPixmap &data, QString &path)
{
    // If the pixmap is present in our local process cache, use that since we
    // don't need to decompress and upload it to the X server/graphics card.
    const PixmapWithPath *pixmapPath = mPixmapCache.object(key);
    if (pixmapPath) {
        path = pixmapPath->path;
        data = pixmapPath->pixmap;

        return true;
    }

    // Otherwise try to find it in our shared memory cache since that will
    // be quicker than the disk, especially for SVGs.
    QByteArray result;

    if (!mIconCache->find(key, &result) || result.isEmpty()) {
        return false;
    }

    QBuffer buffer;
    buffer.setBuffer(&result);
    buffer.open(QIODevice::ReadOnly);

    QDataStream inputStream(&buffer);
    inputStream.setVersion(QDataStream::Qt_4_6);

    QString tempPath;
    inputStream >> tempPath;

    if (inputStream.status() == QDataStream::Ok) {
        QPixmap tempPixmap;
        inputStream >> tempPixmap;

        if (inputStream.status() == QDataStream::Ok) {
            data = tempPixmap;
            path = tempPath;

            // Since we're here we didn't have a QPixmap cache entry, add one now.
            PixmapWithPath *newPixmapWithPath = new PixmapWithPath;
            newPixmapWithPath->pixmap = data;
            newPixmapWithPath->path = path;

            mPixmapCache.insert(key, newPixmapWithPath, data.width() * data.height() + 1);

            return true;
        }
    }

    return false;
}

QString KIconLoaderPrivate::findMatchingIconWithGenericFallbacks(const QString &name, int size, qreal scale) const
{
    QString path = findMatchingIcon(name, size, scale);
    if (!path.isEmpty()) {
        return path;
    }

    const QString genericIcon = s_globalData()->genericIconFor(name);
    if (!genericIcon.isEmpty()) {
        path = findMatchingIcon(genericIcon, size, scale);
    }
    return path;
}

QString KIconLoaderPrivate::findMatchingIcon(const QString &name, int size, qreal scale) const
{
    // This looks for the exact match and its
    // generic fallbacks in each themeNode one after the other.

    // In theory we should only do this for mimetype icons, not for app icons,
    // but that would require different APIs. The long term solution is under
    // development for Qt >= 5.8, QFileIconProvider calling QPlatformTheme::fileIcon,
    // using QMimeType::genericIconName() to get the proper -x-generic fallback.
    // Once everyone uses that to look up mimetype icons, we can kill the fallback code
    // from this method.

    bool genericFallback = name.endsWith(QLatin1String("-x-generic"));;
    QString path;
    for (KIconThemeNode *themeNode : std::as_const(links)) {
        QString currentName = name;

        while (!currentName.isEmpty()) {
            path = themeNode->theme->iconPathByName(currentName, size, KIconLoader::MatchBest, scale);
            if (!path.isEmpty()) {
                return path;
            }

            if (genericFallback) {
                // we already tested the base name
                break;
            }

            int rindex = currentName.lastIndexOf(QLatin1Char('-'));
            if (rindex > 1) { // > 1 so that we don't split x-content or x-epoc
                currentName.truncate(rindex);

                if (currentName.endsWith(QLatin1String("-x"))) {
                    currentName.chop(2);
                }
            } else {
                // From update-mime-database.c
                static const QSet<QString> mediaTypes = QSet<QString>{QStringLiteral("text"),
                                                                      QStringLiteral("application"),
                                                                      QStringLiteral("image"),
                                                                      QStringLiteral("audio"),
                                                                      QStringLiteral("inode"),
                                                                      QStringLiteral("video"),
                                                                      QStringLiteral("message"),
                                                                      QStringLiteral("model"),
                                                                      QStringLiteral("multipart"),
                                                                      QStringLiteral("x-content"),
                                                                      QStringLiteral("x-epoc")};
                // Shared-mime-info spec says:
                // "If [generic-icon] is not specified then the mimetype is used to generate the
                // generic icon by using the top-level media type (e.g. "video" in "video/ogg")
                // and appending "-x-generic" (i.e. "video-x-generic" in the previous example)."
                if (mediaTypes.contains(currentName)) {
                    currentName += QLatin1String("-x-generic");
                    genericFallback = true;
                } else {
                    break;
                }
            }
        }
    }

    if (path.isEmpty()) {
        const QStringList fallbackPaths = QIcon::fallbackSearchPaths();

        for (const QString &path : fallbackPaths) {
            const QString extensions[] = {QStringLiteral(".png"), QStringLiteral(".svg"), QStringLiteral(".svgz"), QStringLiteral(".xpm")};

            for (const QString &ext : extensions) {
                const QString file = path + '/' + name + ext;

                if (QFileInfo::exists(file)) {
                    return file;
                }
            }
        }
    }

    return path;
}

QString KIconLoaderPrivate::preferredIconPath(const QString &name)
{
    QString path;

    auto it = mIconAvailability.constFind(name);
    const auto end = mIconAvailability.constEnd();

    if (it != end && it.value().isEmpty() && !shouldCheckForUnknownIcons()) {
        return path; // known to be unavailable
    }

    if (it != end) {
        path = it.value();
    }

    if (path.isEmpty()) {
        path = q->iconPath(name, KIconLoader::Desktop, KIconLoader::MatchBest);
        mIconAvailability.insert(name, path);
    }

    return path;
}

inline QString KIconLoaderPrivate::unknownIconPath(int size, qreal scale) const
{
    QString path = findMatchingIcon(QStringLiteral("unknown"), size, scale);
    if (path.isEmpty()) {
        qCDebug(KICONTHEMES) << "Warning: could not find \"unknown\" icon for size" << size << "at scale" << scale;
        return QString();
    }
    return path;
}

QString KIconLoaderPrivate::locate(const QString &fileName)
{
    for (const QString &dir : std::as_const(searchPaths)) {
        const QString path = dir + QLatin1Char('/') + fileName;
        if (QDir(dir).isAbsolute()) {
            if (QFileInfo::exists(path)) {
                return path;
            }
        } else {
            const QString fullPath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, path);
            if (!fullPath.isEmpty()) {
                return fullPath;
            }
        }
    }
    return QString();
}

// Finds the absolute path to an icon.

QString KIconLoader::iconPath(const QString &_name, int group_or_size, bool canReturnNull) const
{
    return iconPath(_name, group_or_size, canReturnNull, 1 /*scale*/);
}

QString KIconLoader::iconPath(const QString &_name, int group_or_size, bool canReturnNull, qreal scale) const
{
    // we need to honor resource :/ paths and QDir::searchPaths => use QDir::isAbsolutePath, see bug 434451
    if (_name.isEmpty() || QDir::isAbsolutePath(_name)) {
        // we have either an absolute path or nothing to work with
        return _name;
    }

    QString name = d->removeIconExtension(_name);

    QString path;
    if (group_or_size == KIconLoader::User) {
        path = d->locate(name + QLatin1String(".png"));
        if (path.isEmpty()) {
            path = d->locate(name + QLatin1String(".svgz"));
        }
        if (path.isEmpty()) {
            path = d->locate(name + QLatin1String(".svg"));
        }
        if (path.isEmpty()) {
            path = d->locate(name + QLatin1String(".xpm"));
        }
        return path;
    }

    if (group_or_size >= KIconLoader::LastGroup) {
        qCDebug(KICONTHEMES) << "Invalid icon group:" << group_or_size;
        return path;
    }

    int size;
    if (group_or_size >= 0) {
        size = d->mpGroups[group_or_size].size;
    } else {
        size = -group_or_size;
    }

    if (_name.isEmpty()) {
        if (canReturnNull) {
            return QString();
        } else {
            return d->unknownIconPath(size, scale);
        }
    }

    path = d->findMatchingIconWithGenericFallbacks(name, size, scale);

    if (path.isEmpty()) {
        // Try "User" group too.
        path = iconPath(name, KIconLoader::User, true);
        if (!path.isEmpty() || canReturnNull) {
            return path;
        }

        return d->unknownIconPath(size, scale);
    }
    return path;
}

QPixmap
KIconLoader::loadMimeTypeIcon(const QString &_iconName, KIconLoader::Group group, int size, int state, const QStringList &overlays, QString *path_store) const
{
    QString iconName = _iconName;
    const int slashindex = iconName.indexOf(QLatin1Char('/'));
    if (slashindex != -1) {
        iconName[slashindex] = QLatin1Char('-');
    }

    if (!d->extraDesktopIconsLoaded) {
        const QPixmap pixmap = loadIcon(iconName, group, size, state, overlays, path_store, true);
        if (!pixmap.isNull()) {
            return pixmap;
        }
        d->addExtraDesktopThemes();
    }
    const QPixmap pixmap = loadIcon(iconName, group, size, state, overlays, path_store, true);
    if (pixmap.isNull()) {
        // Icon not found, fallback to application/octet-stream
        return loadIcon(QStringLiteral("application-octet-stream"), group, size, state, overlays, path_store, false);
    }
    return pixmap;
}

QPixmap KIconLoader::loadIcon(const QString &_name,
                              KIconLoader::Group group,
                              int size,
                              int state,
                              const QStringList &overlays,
                              QString *path_store,
                              bool canReturnNull) const
{
    return loadScaledIcon(_name, group, 1.0 /*scale*/, size, state, overlays, path_store, canReturnNull);
}

QPixmap KIconLoader::loadScaledIcon(const QString &_name,
                                    KIconLoader::Group group,
                                    qreal scale,
                                    int size,
                                    int state,
                                    const QStringList &overlays,
                                    QString *path_store,
                                    bool canReturnNull) const
{
    return loadScaledIcon(_name, group, scale, QSize(size, size), state, overlays, path_store, canReturnNull);
}

QPixmap KIconLoader::loadScaledIcon(const QString &_name,
                                    KIconLoader::Group group,
                                    qreal scale,
                                    const QSize &size,
                                    int state,
                                    const QStringList &overlays,
                                    QString *path_store,
                                    bool canReturnNull) const
{
    return loadScaledIcon(_name, group, scale, size, state, overlays, path_store, canReturnNull, {});
}

QPixmap KIconLoader::loadScaledIcon(const QString &_name,
                                    KIconLoader::Group group,
                                    qreal scale,
                                    const QSize &_size,
                                    int state,
                                    const QStringList &overlays,
                                    QString *path_store,
                                    bool canReturnNull,
                                    const std::optional<KIconColors> &colors) const

{
    QString name = _name;
    bool favIconOverlay = false;

    if (_size.width() < 0 || _size.height() < 0 || _name.isEmpty()) {
        return QPixmap();
    }

    QSize size = _size;

    /*
     * This method works in a kind of pipeline, with the following steps:
     * 1. Sanity checks.
     * 2. Convert _name, group, size, etc. to a key name.
     * 3. Check if the key is already cached.
     * 4. If not, initialize the theme and find/load the icon.
     * 4a Apply overlays
     * 4b Re-add to cache.
     */

    // Special case for absolute path icons.
    if (name.startsWith(QLatin1String("favicons/"))) {
        favIconOverlay = true;
        name = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1Char('/') + name + QStringLiteral(".png");
    }

    // we need to honor resource :/ paths and QDir::searchPaths => use QDir::isAbsolutePath, see bug 434451
    const bool absolutePath = QDir::isAbsolutePath(name);
    if (!absolutePath) {
        name = d->removeIconExtension(name);
    }

    // Don't bother looking for an icon with no name.
    if (name.isEmpty()) {
        return QPixmap();
    }

    // May modify group, size, or state. This function puts them into sane
    // states.
    d->normalizeIconMetadata(group, size, state);

    // See if the image is already cached.
    auto usedColors = colors ? *colors : d->mCustomColors ? d->mColors : KIconColors(qApp->palette());
    QString key = d->makeCacheKey(name, group, overlays, size, scale, state, usedColors);
    QPixmap pix;

    bool iconWasUnknown = false;
    QString path;

    if (d->findCachedPixmapWithPath(key, pix, path)) {
        pix.setDevicePixelRatio(scale);

        if (path_store) {
            *path_store = path;
        }

        if (!path.isEmpty()) {
            return pix;
        } else {
            // path is empty for "unknown" icons, which should be searched for
            // anew regularly
            if (!d->shouldCheckForUnknownIcons()) {
                return canReturnNull ? QPixmap() : pix;
            }
        }
    }

    // Image is not cached... go find it and apply effects.

    favIconOverlay = favIconOverlay && std::min(size.height(), size.width()) > 22;

    // First we look for non-User icons. If we don't find one we'd search in
    // the User space anyways...
    if (group != KIconLoader::User) {
        if (absolutePath && !favIconOverlay) {
            path = name;
        } else {
            path = d->findMatchingIconWithGenericFallbacks(favIconOverlay ? QStringLiteral("text-html") : name, std::min(size.height(), size.width()), scale);
        }
    }

    if (path.isEmpty()) {
        // We do have a "User" icon, or we couldn't find the non-User one.
        path = (absolutePath) ? name : iconPath(name, KIconLoader::User, canReturnNull);
    }

    // Still can't find it? Use "unknown" if we can't return null.
    // We keep going in the function so we can ensure this result gets cached.
    if (path.isEmpty() && !canReturnNull) {
        path = d->unknownIconPath(std::min(size.height(), size.width()), scale);
        iconWasUnknown = true;
    }

    QImage img;
    if (!path.isEmpty()) {
        img = d->createIconImage(path, size, scale, static_cast<KIconLoader::States>(state), usedColors);
    }

    if (group >= 0 && group < KIconLoader::LastGroup) {
        img = d->mpEffect.apply(img, group, state);
    }

    if (favIconOverlay) {
        QImage favIcon(name, "PNG");
        if (!favIcon.isNull()) { // if favIcon not there yet, don't try to blend it
            QPainter p(&img);

            // Align the favicon overlay
            QRect r(favIcon.rect());
            r.moveBottomRight(img.rect().bottomRight());
            r.adjust(-1, -1, -1, -1); // Move off edge

            // Blend favIcon over img.
            p.drawImage(r, favIcon);
        }
    }

    pix = QPixmap::fromImage(img);

    // TODO: If we make a loadIcon that returns the image we can convert
    // drawOverlays to use the image instead of pixmaps as well so we don't
    // have to transfer so much to the graphics card.
    d->drawOverlays(this, group, state, pix, overlays);

    // Don't add the path to our unknown icon to the cache, only cache the
    // actual image.
    if (iconWasUnknown) {
        path.clear();
    }

    d->insertCachedPixmapWithPath(key, pix, path);

    if (path_store) {
        *path_store = path;
    }

    return pix;
}

KPixmapSequence KIconLoader::loadPixmapSequence(const QString &xdgIconName, int size) const
{
    return KPixmapSequence(iconPath(xdgIconName, -size), size);
}

QMovie *KIconLoader::loadMovie(const QString &name, KIconLoader::Group group, int size, QObject *parent) const
{
    QString file = moviePath(name, group, size);
    if (file.isEmpty()) {
        return nullptr;
    }
    int dirLen = file.lastIndexOf(QLatin1Char('/'));
    const QString icon = iconPath(name, size ? -size : group, true);
    if (!icon.isEmpty() && file.left(dirLen) != icon.left(dirLen)) {
        return nullptr;
    }
    QMovie *movie = new QMovie(file, QByteArray(), parent);
    if (!movie->isValid()) {
        delete movie;
        return nullptr;
    }
    return movie;
}

QString KIconLoader::moviePath(const QString &name, KIconLoader::Group group, int size) const
{
    if (!d->mpGroups) {
        return QString();
    }

    if ((group < -1 || group >= KIconLoader::LastGroup) && group != KIconLoader::User) {
        qCDebug(KICONTHEMES) << "Invalid icon group:" << group << ", should be one of KIconLoader::Group";
        group = KIconLoader::Desktop;
    }
    if (size == 0 && group < 0) {
        qCDebug(KICONTHEMES) << "Neither size nor group specified!";
        group = KIconLoader::Desktop;
    }

    QString file = name + QStringLiteral(".mng");
    if (group == KIconLoader::User) {
        file = d->locate(file);
    } else {
        if (size == 0) {
            size = d->mpGroups[group].size;
        }

        QString path;

        for (KIconThemeNode *themeNode : std::as_const(d->links)) {
            path = themeNode->theme->iconPath(file, size, KIconLoader::MatchExact);
            if (!path.isEmpty()) {
                break;
            }
        }

        if (path.isEmpty()) {
            for (KIconThemeNode *themeNode : std::as_const(d->links)) {
                path = themeNode->theme->iconPath(file, size, KIconLoader::MatchBest);
                if (!path.isEmpty()) {
                    break;
                }
            }
        }

        file = path;
    }
    return file;
}

QStringList KIconLoader::loadAnimated(const QString &name, KIconLoader::Group group, int size) const
{
    QStringList lst;

    if (!d->mpGroups) {
        return lst;
    }

    d->initIconThemes();

    if ((group < -1) || (group >= KIconLoader::LastGroup)) {
        qCDebug(KICONTHEMES) << "Invalid icon group: " << group << ", should be one of KIconLoader::Group";
        group = KIconLoader::Desktop;
    }
    if ((size == 0) && (group < 0)) {
        qCDebug(KICONTHEMES) << "Neither size nor group specified!";
        group = KIconLoader::Desktop;
    }

    QString file = name + QStringLiteral("/0001");
    if (group == KIconLoader::User) {
        file = d->locate(file + QStringLiteral(".png"));
    } else {
        if (size == 0) {
            size = d->mpGroups[group].size;
        }
        file = d->findMatchingIcon(file, size, 1); // FIXME scale
    }
    if (file.isEmpty()) {
        return lst;
    }

    QString path = file.left(file.length() - 8);
    QDir dir(QFile::encodeName(path));
    if (!dir.exists()) {
        return lst;
    }

    const auto entryList = dir.entryList();
    for (const QString &entry : entryList) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const QStringView chunk = QStringView(entry).left(4);
#else
        const QStringRef chunk = entry.leftRef(4);
#endif
        if (!chunk.toUInt()) {
            continue;
        }

        lst += path + entry;
    }
    lst.sort();
    return lst;
}

KIconTheme *KIconLoader::theme() const
{
    if (d->mpThemeRoot) {
        return d->mpThemeRoot->theme;
    }
    return nullptr;
}

int KIconLoader::currentSize(KIconLoader::Group group) const
{
    if (!d->mpGroups) {
        return -1;
    }

    if (group < 0 || group >= KIconLoader::LastGroup) {
        qCDebug(KICONTHEMES) << "Invalid icon group:" << group << ", should be one of KIconLoader::Group";
        return -1;
    }
    return d->mpGroups[group].size;
}

QStringList KIconLoader::queryIconsByDir(const QString &iconsDir) const
{
    const QDir dir(iconsDir);
    const QStringList formats = QStringList() << QStringLiteral("*.png") << QStringLiteral("*.xpm") << QStringLiteral("*.svg") << QStringLiteral("*.svgz");
    const QStringList lst = dir.entryList(formats, QDir::Files);
    QStringList result;
    for (const auto &file : lst) {
        result += iconsDir + QLatin1Char('/') + file;
    }
    return result;
}

QStringList KIconLoader::queryIconsByContext(int group_or_size, KIconLoader::Context context) const
{
    QStringList result;
    if (group_or_size >= KIconLoader::LastGroup) {
        qCDebug(KICONTHEMES) << "Invalid icon group:" << group_or_size;
        return result;
    }
    int size;
    if (group_or_size >= 0) {
        size = d->mpGroups[group_or_size].size;
    } else {
        size = -group_or_size;
    }

    for (KIconThemeNode *themeNode : std::as_const(d->links)) {
        themeNode->queryIconsByContext(&result, size, context);
    }

    // Eliminate duplicate entries (same icon in different directories)
    QString name;
    QStringList res2;
    QStringList entries;
    for (const auto &icon : std::as_const(result)) {
        const int n = icon.lastIndexOf(QLatin1Char('/'));
        if (n == -1) {
            name = icon;
        } else {
            name = icon.mid(n + 1);
        }
        name = d->removeIconExtension(name);
        if (!entries.contains(name)) {
            entries += name;
            res2 += icon;
        }
    }
    return res2;
}

QStringList KIconLoader::queryIcons(int group_or_size, KIconLoader::Context context) const
{
    d->initIconThemes();

    QStringList result;
    if (group_or_size >= KIconLoader::LastGroup) {
        qCDebug(KICONTHEMES) << "Invalid icon group:" << group_or_size;
        return result;
    }
    int size;
    if (group_or_size >= 0) {
        size = d->mpGroups[group_or_size].size;
    } else {
        size = -group_or_size;
    }

    for (KIconThemeNode *themeNode : std::as_const(d->links)) {
        themeNode->queryIcons(&result, size, context);
    }

    // Eliminate duplicate entries (same icon in different directories)
    QString name;
    QStringList res2;
    QStringList entries;
    for (const auto &icon : std::as_const(result)) {
        const int n = icon.lastIndexOf(QLatin1Char('/'));
        if (n == -1) {
            name = icon;
        } else {
            name = icon.mid(n + 1);
        }
        name = d->removeIconExtension(name);
        if (!entries.contains(name)) {
            entries += name;
            res2 += icon;
        }
    }
    return res2;
}

// used by KIconDialog to find out which contexts to offer in a combobox
bool KIconLoader::hasContext(KIconLoader::Context context) const
{
    for (KIconThemeNode *themeNode : std::as_const(d->links)) {
        if (themeNode->theme->hasContext(context)) {
            return true;
        }
    }
    return false;
}

KIconEffect *KIconLoader::iconEffect() const
{
    return &d->mpEffect;
}

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 82)
bool KIconLoader::alphaBlending(KIconLoader::Group group) const
{
    if (!d->mpGroups) {
        return false;
    }

    if (group < 0 || group >= KIconLoader::LastGroup) {
        qCDebug(KICONTHEMES) << "Invalid icon group:" << group << ", should be one of KIconLoader::Group";
        return false;
    }
    return true;
}
#endif

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 0)
QIcon KIconLoader::loadIconSet(const QString &name, KIconLoader::Group g, int s, bool canReturnNull)
{
    QIcon iconset;
    QPixmap tmp = loadIcon(name, g, s, KIconLoader::ActiveState, QStringList(), nullptr, canReturnNull);
    iconset.addPixmap(tmp, QIcon::Active, QIcon::On);
    // we don't use QIconSet's resizing anyway
    tmp = loadIcon(name, g, s, KIconLoader::DisabledState, QStringList(), nullptr, canReturnNull);
    iconset.addPixmap(tmp, QIcon::Disabled, QIcon::On);
    tmp = loadIcon(name, g, s, KIconLoader::DefaultState, QStringList(), nullptr, canReturnNull);
    iconset.addPixmap(tmp, QIcon::Normal, QIcon::On);
    return iconset;
}
#endif

// Easy access functions

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 63)
QPixmap DesktopIcon(const QString &name, int force_size, int state, const QStringList &overlays)
{
    KIconLoader *loader = KIconLoader::global();
    return loader->loadIcon(name, KIconLoader::Desktop, force_size, state, overlays);
}
#endif

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 0)
QIcon DesktopIconSet(const QString &name, int force_size)
{
    KIconLoader *loader = KIconLoader::global();
    return loader->loadIconSet(name, KIconLoader::Desktop, force_size);
}
#endif

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 63)
QPixmap BarIcon(const QString &name, int force_size, int state, const QStringList &overlays)
{
    KIconLoader *loader = KIconLoader::global();
    return loader->loadIcon(name, KIconLoader::Toolbar, force_size, state, overlays);
}
#endif

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 0)
QIcon BarIconSet(const QString &name, int force_size)
{
    KIconLoader *loader = KIconLoader::global();
    return loader->loadIconSet(name, KIconLoader::Toolbar, force_size);
}
#endif

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 63)
QPixmap SmallIcon(const QString &name, int force_size, int state, const QStringList &overlays)
{
    KIconLoader *loader = KIconLoader::global();
    return loader->loadIcon(name, KIconLoader::Small, force_size, state, overlays);
}
#endif

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 0)
QIcon SmallIconSet(const QString &name, int force_size)
{
    KIconLoader *loader = KIconLoader::global();
    return loader->loadIconSet(name, KIconLoader::Small, force_size);
}
#endif

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 63)
QPixmap MainBarIcon(const QString &name, int force_size, int state, const QStringList &overlays)
{
    KIconLoader *loader = KIconLoader::global();
    return loader->loadIcon(name, KIconLoader::MainToolbar, force_size, state, overlays);
}
#endif

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 0)
QIcon MainBarIconSet(const QString &name, int force_size)
{
    KIconLoader *loader = KIconLoader::global();
    return loader->loadIconSet(name, KIconLoader::MainToolbar, force_size);
}
#endif

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 65)
QPixmap UserIcon(const QString &name, int state, const QStringList &overlays)
{
    KIconLoader *loader = KIconLoader::global();
    return loader->loadIcon(name, KIconLoader::User, 0, state, overlays);
}
#endif

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 0)
QIcon UserIconSet(const QString &name)
{
    KIconLoader *loader = KIconLoader::global();
    return loader->loadIconSet(name, KIconLoader::User);
}
#endif

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 66)
int IconSize(KIconLoader::Group group)
{
    KIconLoader *loader = KIconLoader::global();
    return loader->currentSize(group);
}
#endif

QPixmap KIconLoader::unknown()
{
    QPixmap pix;
    if (QPixmapCache::find(QStringLiteral("unknown"), &pix)) { // krazy:exclude=iconnames
        return pix;
    }

    const QString path = global()->iconPath(QStringLiteral("unknown"), KIconLoader::Small, true); // krazy:exclude=iconnames
    if (path.isEmpty()) {
        qCDebug(KICONTHEMES) << "Warning: Cannot find \"unknown\" icon.";
        pix = QPixmap(32, 32);
    } else {
        pix.load(path);
        QPixmapCache::insert(QStringLiteral("unknown"), pix); // krazy:exclude=iconnames
    }

    return pix;
}

bool KIconLoader::hasIcon(const QString &name) const
{
    return !d->preferredIconPath(name).isEmpty();
}

void KIconLoader::setCustomPalette(const QPalette &palette)
{
    d->mCustomColors = true;
    d->mColors = KIconColors(palette);
}

QPalette KIconLoader::customPalette() const
{
    return d->mCustomColors ? d->mPalette : QPalette();
}

void KIconLoader::resetPalette()
{
    d->mCustomColors = false;
}

bool KIconLoader::hasCustomPalette() const
{
    return d->mCustomColors;
}

/*** the global icon loader ***/
Q_GLOBAL_STATIC(KIconLoader, globalIconLoader)

KIconLoader *KIconLoader::global()
{
    return globalIconLoader();
}

void KIconLoader::newIconLoader()
{
    if (global() == this) {
        KIconTheme::reconfigure();
    }

    reconfigure(objectName());
    Q_EMIT iconLoaderSettingsChanged();
}

void KIconLoader::emitChange(KIconLoader::Group g)
{
    s_globalData->emitChange(g);
}

#include <kiconengine.h>
QIcon KDE::icon(const QString &iconName, KIconLoader *iconLoader)
{
    return QIcon(new KIconEngine(iconName, iconLoader ? iconLoader : KIconLoader::global()));
}

QIcon KDE::icon(const QString &iconName, const QStringList &overlays, KIconLoader *iconLoader)
{
    return QIcon(new KIconEngine(iconName, iconLoader ? iconLoader : KIconLoader::global(), overlays));
}

QIcon KDE::icon(const QString &iconName, const KIconColors &colors, KIconLoader *iconLoader)
{
    return QIcon(new KIconEngine(iconName, colors, iconLoader ? iconLoader : KIconLoader::global()));
}

#include "kiconloader.moc"
#include "moc_kiconloader.moc"
