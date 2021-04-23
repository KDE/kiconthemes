/* vi: ts=8 sts=4 sw=4

    kiconloader.cpp: An icon loader for KDE with theming functionality.

    This file is part of the KDE project, module kdeui.
    SPDX-FileCopyrightText: 2000 Geert Jansen <jansen@kde.org>
    SPDX-FileCopyrightText: 2000 Antonio Larrosa <larrosa@kde.org>
    SPDX-FileCopyrightText: 2010 Michael Pyne <mpyne@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kiconloader.h"

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
#include "kiconeffect.h"
#include "kicontheme.h"

// kwidgetsaddons
#include <KPixmapSequence>

#include <KColorScheme>
#include <KCompressionDevice>

#include <QBuffer>
#include <QByteArray>
#include <QCache>
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

#include <qplatformdefs.h> //for readlink

#include <assert.h>

namespace
{
// Used to make cache keys for icons with no group. Result type is QString*
QString NULL_EFFECT_FINGERPRINT()
{
    return QStringLiteral("noeffect");
}

QString STYLESHEET_TEMPLATE()
{
    /* clang-format off */
    return QStringLiteral(".ColorScheme-Text {\
color:%1;\
}\
.ColorScheme-Background{\
color:%2;\
}\
.ColorScheme-Highlight{\
color:%3;\
}\
.ColorScheme-HighlightedText{\
color:%4;\
}\
.ColorScheme-PositiveText{\
color:%5;\
}\
.ColorScheme-NeutralText{\
color:%6;\
}\
.ColorScheme-NegativeText{\
color:%7;\
}");
    /* clang-format on */
}
}

/**
 * Holds a QPixmap for this process, along with its associated path on disk.
 */
struct PixmapWithPath {
    QPixmap pixmap;
    QString path;
};

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

static QString paletteId(const QPalette &pal)
{
    // 8 per color. We want 3 colors thus 8*4=32.
    QString buffer(32, Qt::Uninitialized);

    uintToHex(pal.windowText().color().rgba(), buffer.data());
    uintToHex(pal.highlight().color().rgba(), buffer.data() + 8);
    uintToHex(pal.highlightedText().color().rgba(), buffer.data() + 16);
    uintToHex(pal.window().color().rgba(), buffer.data() + 24);

    return buffer;
}

/*** KIconThemeNode: A node in the icon theme dependancy tree. ***/

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

/*** KIconGroup: Icon type description. ***/

struct KIconGroup {
    int size;
};

extern KICONTHEMES_EXPORT int kiconloader_ms_between_checks;
KICONTHEMES_EXPORT int kiconloader_ms_between_checks = 5000;

/*** d pointer for KIconLoader. ***/
class KIconLoaderPrivate
{
public:
    KIconLoaderPrivate(KIconLoader *q)
        : q(q)
    {
    }

    ~KIconLoaderPrivate()
    {
        clear();
    }

    void clear()
    {
        /* antlarr: There's no need to delete d->mpThemeRoot as it's already
        deleted when the elements of d->links are deleted */
        qDeleteAll(links);
        delete[] mpGroups;
        delete mIconCache;
        mpGroups = nullptr;
        mIconCache = nullptr;
        mPixmapCache.clear();
        appname.clear();
        searchPaths.clear();
        links.clear();
        mIconThemeInited = false;
        mThemesInTree.clear();
    }

    /**
     * @internal
     */
    void init(const QString &_appname, const QStringList &extraSearchPaths = QStringList());

    /**
     * @internal
     */
    void initIconThemes();

    /**
     * @internal
     * tries to find an icon with the name. It tries some extension and
     * match strategies
     */
    QString findMatchingIcon(const QString &name, int size, qreal scale) const;

    /**
     * @internal
     * tries to find an icon with the name.
     * This is one layer above findMatchingIcon -- it also implements generic fallbacks
     * such as generic icons for mimetypes.
     */
    QString findMatchingIconWithGenericFallbacks(const QString &name, int size, qreal scale) const;

    /**
     * @internal
     * Adds themes installed in the application's directory.
     **/
    void addAppThemes(const QString &appname, const QString &themeBaseDir = QString());

    /**
     * @internal
     * Adds all themes that are part of this node and the themes
     * below (the fallbacks of the theme) into the tree.
     */
    void addBaseThemes(KIconThemeNode *node, const QString &appname);

    /**
     * @internal
     * Recursively adds all themes that are specified in the "Inherits"
     * property of the given theme into the tree.
     */
    void addInheritedThemes(KIconThemeNode *node, const QString &appname);

    /**
     * @internal
     * Creates a KIconThemeNode out of a theme name, and adds this theme
     * as well as all its inherited themes into the tree. Themes that already
     * exist in the tree will be ignored and not added twice.
     */
    void addThemeByName(const QString &themename, const QString &appname);

    /**
     * Adds all the default themes from other desktops at the end of
     * the list of icon themes.
     */
    void addExtraDesktopThemes();

    /**
     * @internal
     * return the path for the unknown icon in that size
     */
    QString unknownIconPath(int size, qreal scale) const;

    /**
     * Checks if name ends in one of the supported icon formats (i.e. .png)
     * and returns the name without the extension if it does.
     */
    QString removeIconExtension(const QString &name) const;

    /**
     * @internal
     * Used with KIconLoader::loadIcon to convert the given name, size, group,
     * and icon state information to valid states. All parameters except the
     * name can be modified as well to be valid.
     */
    void normalizeIconMetadata(KIconLoader::Group &group, QSize &size, int &state) const;

    /**
     * @internal
     * Used with KIconLoader::loadIcon to get a base key name from the given
     * icon metadata. Ensure the metadata is normalized first.
     */
    QString makeCacheKey(const QString &name, KIconLoader::Group group, const QStringList &overlays, const QSize &size, qreal scale, int state) const;

    /**
     * @internal
     * If the icon is an SVG file, process it generating a stylesheet
     * following the current color scheme. in this case the icon can use named colors
     * as text color, background color, highlight color, positive/neutral/negative color
     * @see KColorScheme
     */
    QByteArray processSvg(const QString &path, KIconLoader::States state) const;

    /**
     * @internal
     * Creates the QImage for @p path, using SVG rendering as appropriate.
     * @p size is only used for scalable images, but if non-zero non-scalable
     * images will be resized anyways.
     */
    QImage createIconImage(const QString &path, const QSize &size = {}, qreal scale = 1.0, KIconLoader::States state = KIconLoader::DefaultState);

    /**
     * @internal
     * Adds an QPixmap with its associated path to the shared icon cache.
     */
    void insertCachedPixmapWithPath(const QString &key, const QPixmap &data, const QString &path);

    /**
     * @internal
     * Retrieves the path and pixmap of the given key from the shared
     * icon cache.
     */
    bool findCachedPixmapWithPath(const QString &key, QPixmap &data, QString &path);

    /**
     * Find the given file in the search paths.
     */
    QString locate(const QString &fileName);

    /**
     * @internal
     * React to a global icon theme change
     */
    void _k_refreshIcons(int group);

    bool shouldCheckForUnknownIcons()
    {
        if (mLastUnknownIconCheck.isValid() && mLastUnknownIconCheck.elapsed() < kiconloader_ms_between_checks) {
            return false;
        }
        mLastUnknownIconCheck.start();
        return true;
    }

    KIconLoader *const q;

    QStringList mThemesInTree;
    KIconGroup *mpGroups = nullptr;
    KIconThemeNode *mpThemeRoot = nullptr;
    QStringList searchPaths;
    KIconEffect mpEffect;
    QList<KIconThemeNode *> links;

    // This shares the icons across all processes
    KSharedDataCache *mIconCache = nullptr;

    // This caches rendered QPixmaps in just this process.
    QCache<QString, PixmapWithPath> mPixmapCache;

    bool extraDesktopIconsLoaded : 1;
    // lazy loading: initIconThemes() is only needed when the "links" list is needed
    // mIconThemeInited is used inside initIconThemes() to init only once
    bool mIconThemeInited : 1;
    QString appname;

    void drawOverlays(const KIconLoader *loader, KIconLoader::Group group, int state, QPixmap &pix, const QStringList &overlays);

    QHash<QString, bool> mIconAvailability; // icon name -> true (known to be available) or false (known to be unavailable)
    QElapsedTimer mLastUnknownIconCheck; // recheck for unknown icons after kiconloader_ms_between_checks
    // the palette used to recolor svg icons stylesheets
    QPalette mPalette;
    // to keep track if we are using a custom palette or just falling back to qApp;
    bool mCustomPalette = false;
};

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
        stream.setCodec("ISO 8859-1");
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
        // If we're refreshing icons the Qt platform plugin has probably
        // already cached the old theme, which will accidentally filter back
        // into KIconTheme unless we reset it
        QIcon::setThemeName(newThemeName);
    }

    q->newIconLoader();
    mIconAvailability.clear();
    Q_EMIT q->iconChanged(group);
}

KIconLoader::KIconLoader(const QString &_appname, const QStringList &extraSearchPaths, QObject *parent)
    : QObject(parent)
    , d(new KIconLoaderPrivate(this))
{
    setObjectName(_appname);

    connect(s_globalData, &KIconLoaderGlobalData::iconChanged, this, [this](int group) {
        d->_k_refreshIcons(group);
    });
    d->init(_appname, extraSearchPaths);
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

    appname = _appname;
    if (appname.isEmpty()) {
        appname = QCoreApplication::applicationName();
    }

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
    KIconTheme *def = new KIconTheme(KIconTheme::current(), appname);
    if (!def->isValid()) {
        delete def;
        // warn, as this is actually a small penalty hit
        qCDebug(KICONTHEMES) << "Couldn't find current icon theme, falling back to default.";
        def = new KIconTheme(KIconTheme::defaultThemeName(), appname);
        if (!def->isValid()) {
            qCDebug(KICONTHEMES) << "Standard icon theme" << KIconTheme::defaultThemeName() << "not found!";
            delete def;
            return;
        }
    }
    mpThemeRoot = new KIconThemeNode(def);
    mThemesInTree.append(def->internalName());
    links.append(mpThemeRoot);
    addBaseThemes(mpThemeRoot, appname);

    // Insert application specific themes at the top.
    searchPaths.append(appname + QStringLiteral("/pics"));

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
    d->initIconThemes();

    d->searchPaths.append(appname + QStringLiteral("/pics"));
    d->addAppThemes(appname, themeBaseDir);
}

void KIconLoaderPrivate::addAppThemes(const QString &appname, const QString &themeBaseDir)
{
    initIconThemes();

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
    addThemeByName(QStringLiteral("hicolor"), appname);
}

void KIconLoaderPrivate::addInheritedThemes(KIconThemeNode *node, const QString &appname)
{
    const QStringList lst = node->theme->inherits();

    for (QStringList::ConstIterator it = lst.begin(), total = lst.end(); it != total; ++it) {
        if ((*it) == QLatin1String("hicolor")) {
            // The icon theme spec says that "hicolor" must be the very last
            // of all inherited themes, so don't add it here but at the very end
            // of addBaseThemes().
            continue;
        }
        addThemeByName(*it, appname);
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

    initIconThemes();

    QStringList list;
    const QStringList icnlibs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons"), QStandardPaths::LocateDirectory);
    char buf[1000];
    for (QStringList::ConstIterator it = icnlibs.begin(), total = icnlibs.end(); it != total; ++it) {
        QDir dir(*it);
        if (!dir.exists()) {
            continue;
        }
        const QStringList lst = dir.entryList(QStringList(QStringLiteral("default.*")), QDir::Dirs);
        for (QStringList::ConstIterator it2 = lst.begin(), total = lst.end(); it2 != total; ++it2) {
            if (!QFileInfo::exists(*it + *it2 + QStringLiteral("/index.desktop")) //
                && !QFileInfo::exists(*it + *it2 + QStringLiteral("/index.theme"))) {
                continue;
            }
            // TODO: Is any special handling required for NTFS symlinks?
#ifndef Q_OS_WIN
            const int r = readlink(QFile::encodeName(*it + *it2), buf, sizeof(buf) - 1);
            if (r > 0) {
                buf[r] = 0;
                const QDir dir2(buf);
                QString themeName = dir2.dirName();

                if (!list.contains(themeName)) {
                    list.append(themeName);
                }
            }
#endif
        }
    }

    for (QStringList::ConstIterator it = list.constBegin(), total = list.constEnd(); it != total; ++it) {
        // Don't add the KDE defaults once more, we have them anyways.
        if (*it == QLatin1String("default.kde") || *it == QLatin1String("default.kde4")) {
            continue;
        }
        addThemeByName(*it, QLatin1String(""));
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
        qWarning() << "Illegal icon state:" << state;
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
        qWarning() << "Illegal icon group:" << group;
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

QString KIconLoaderPrivate::makeCacheKey(const QString &name, KIconLoader::Group group, const QStringList &overlays, const QSize &size, qreal scale, int state) const
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
            % paletteId(mCustomPalette ? mPalette : qApp->palette())
            % (q->theme() && q->theme()->followsColorScheme() && state == KIconLoader::SelectedState ? QStringLiteral("_selected") : QString());
    /* clang-format on */
}

QByteArray KIconLoaderPrivate::processSvg(const QString &path, KIconLoader::States state) const
{
    QScopedPointer<QIODevice> device;

    if (path.endsWith(QLatin1String("svgz"))) {
        device.reset(new KCompressionDevice(path, KCompressionDevice::GZip));
    } else {
        device.reset(new QFile(path));
    }

    if (!device->open(QIODevice::ReadOnly)) {
        return QByteArray();
    }

    const QPalette pal = mCustomPalette ? mPalette : qApp->palette();
    KColorScheme scheme(QPalette::Active, KColorScheme::Window);

    /* clang-format off */
    QString styleSheet = STYLESHEET_TEMPLATE().arg(
                                state == KIconLoader::SelectedState ? pal.highlightedText().color().name() : pal.windowText().color().name(),
                                state == KIconLoader::SelectedState ? pal.highlight().color().name() : pal.window().color().name(),
                                state == KIconLoader::SelectedState ? pal.highlightedText().color().name() : pal.highlight().color().name(),
                                state == KIconLoader::SelectedState ? pal.highlight().color().name() : pal.highlightedText().color().name(),
                                scheme.foreground(KColorScheme::PositiveText).color().name(),
                                scheme.foreground(KColorScheme::NeutralText).color().name(),
                                scheme.foreground(KColorScheme::NegativeText).color().name());
    /* clang-format on */

    QByteArray processedContents;
    QXmlStreamReader reader(device.data());

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

QImage KIconLoaderPrivate::createIconImage(const QString &path, const QSize &size, qreal scale, KIconLoader::States state)
{
    // TODO: metadata in the theme to make it do this only if explicitly supported?
    QImageReader reader;
    QBuffer buffer;

    if (q->theme() && q->theme()->followsColorScheme() && (path.endsWith(QLatin1String("svg")) || path.endsWith(QLatin1String("svgz")))) {
        buffer.setData(processSvg(path, state));
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
    const_cast<KIconLoaderPrivate *>(this)->initIconThemes();

    // Do two passes through themeNodes.
    //
    // The first pass looks for an exact match in each themeNode one after the other.
    // If one is found and it is an app icon then return that icon.
    //
    // In the next pass (assuming the first pass failed), it looks for
    // generic fallbacks in each themeNode one after the other.

    // In theory we should only do this for mimetype icons, not for app icons,
    // but that would require different APIs. The long term solution is under
    // development for Qt >= 5.8, QFileIconProvider calling QPlatformTheme::fileIcon,
    // using QMimeType::genericIconName() to get the proper -x-generic fallback.
    // Once everone uses that to look up mimetype icons, we can kill the fallback code
    // from this method.

    for (KIconThemeNode *themeNode : qAsConst(links)) {
        const QString path = themeNode->theme->iconPathByName(name, size, KIconLoader::MatchBest, scale);
        if (!path.isEmpty()) {
            return path;
        }
    }

    if (name.endsWith(QLatin1String("-x-generic"))) {
        return QString(); // no further fallback
    }
    bool genericFallback = false;
    QString path;
    for (KIconThemeNode *themeNode : qAsConst(links)) {
        QString currentName = name;

        while (!currentName.isEmpty()) {
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

            if (currentName.isEmpty()) {
                break;
            }

            // qCDebug(KICONTHEMES) << "Looking up" << currentName;
            path = themeNode->theme->iconPathByName(currentName, size, KIconLoader::MatchBest, scale);
            if (!path.isEmpty()) {
                return path;
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
    for (const QString &dir : qAsConst(searchPaths)) {
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

    d->initIconThemes();

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
        qCDebug(KICONTHEMES) << "Illegal icon group:" << group_or_size;
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
                                    const QSize &_size,
                                    int state,
                                    const QStringList &overlays,
                                    QString *path_store,
                                    bool canReturnNull) const

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
    QString key = d->makeCacheKey(name, group, overlays, size, scale, state);
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
    d->initIconThemes();

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
        img = d->createIconImage(path, size, scale, static_cast<KIconLoader::States>(state));
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

    d->initIconThemes();

    if ((group < -1 || group >= KIconLoader::LastGroup) && group != KIconLoader::User) {
        qCDebug(KICONTHEMES) << "Illegal icon group:" << group;
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

        for (KIconThemeNode *themeNode : qAsConst(d->links)) {
            path = themeNode->theme->iconPath(file, size, KIconLoader::MatchExact);
            if (!path.isEmpty()) {
                break;
            }
        }

        if (path.isEmpty()) {
            for (KIconThemeNode *themeNode : qAsConst(d->links)) {
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
        qCDebug(KICONTHEMES) << "Illegal icon group: " << group;
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
        if (!(entry.leftRef(4)).toUInt()) {
            continue;
        }

        lst += path + entry;
    }
    lst.sort();
    return lst;
}

KIconTheme *KIconLoader::theme() const
{
    d->initIconThemes();
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
        qCDebug(KICONTHEMES) << "Illegal icon group:" << group;
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
    for (QStringList::ConstIterator it = lst.begin(), total = lst.end(); it != total; ++it) {
        result += iconsDir + QLatin1Char('/') + *it;
    }
    return result;
}

QStringList KIconLoader::queryIconsByContext(int group_or_size, KIconLoader::Context context) const
{
    d->initIconThemes();

    QStringList result;
    if (group_or_size >= KIconLoader::LastGroup) {
        qCDebug(KICONTHEMES) << "Illegal icon group:" << group_or_size;
        return result;
    }
    int size;
    if (group_or_size >= 0) {
        size = d->mpGroups[group_or_size].size;
    } else {
        size = -group_or_size;
    }

    for (KIconThemeNode *themeNode : qAsConst(d->links)) {
        themeNode->queryIconsByContext(&result, size, context);
    }

    // Eliminate duplicate entries (same icon in different directories)
    QString name;
    QStringList res2, entries;
    QStringList::ConstIterator it;
    for (it = result.constBegin(); it != result.constEnd(); ++it) {
        int n = (*it).lastIndexOf(QLatin1Char('/'));
        if (n == -1) {
            name = *it;
        } else {
            name = (*it).mid(n + 1);
        }
        name = d->removeIconExtension(name);
        if (!entries.contains(name)) {
            entries += name;
            res2 += *it;
        }
    }
    return res2;
}

QStringList KIconLoader::queryIcons(int group_or_size, KIconLoader::Context context) const
{
    d->initIconThemes();

    QStringList result;
    if (group_or_size >= KIconLoader::LastGroup) {
        qCDebug(KICONTHEMES) << "Illegal icon group:" << group_or_size;
        return result;
    }
    int size;
    if (group_or_size >= 0) {
        size = d->mpGroups[group_or_size].size;
    } else {
        size = -group_or_size;
    }

    for (KIconThemeNode *themeNode : qAsConst(d->links)) {
        themeNode->queryIcons(&result, size, context);
    }

    // Eliminate duplicate entries (same icon in different directories)
    QString name;
    QStringList res2, entries;
    QStringList::ConstIterator it;
    for (it = result.constBegin(); it != result.constEnd(); ++it) {
        int n = (*it).lastIndexOf(QLatin1Char('/'));
        if (n == -1) {
            name = *it;
        } else {
            name = (*it).mid(n + 1);
        }
        name = d->removeIconExtension(name);
        if (!entries.contains(name)) {
            entries += name;
            res2 += *it;
        }
    }
    return res2;
}

// used by KIconDialog to find out which contexts to offer in a combobox
bool KIconLoader::hasContext(KIconLoader::Context context) const
{
    d->initIconThemes();

    for (KIconThemeNode *themeNode : qAsConst(d->links))
        if (themeNode->theme->hasContext(context)) {
            return true;
        }
    return false;
}

KIconEffect *KIconLoader::iconEffect() const
{
    return &d->mpEffect;
}

bool KIconLoader::alphaBlending(KIconLoader::Group group) const
{
    if (!d->mpGroups) {
        return false;
    }

    if (group < 0 || group >= KIconLoader::LastGroup) {
        qCDebug(KICONTHEMES) << "Illegal icon group:" << group;
        return false;
    }
    return true;
}

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
    auto it = d->mIconAvailability.constFind(name);
    const auto end = d->mIconAvailability.constEnd();
    if (it != end && !it.value() && !d->shouldCheckForUnknownIcons()) {
        return false; // known to be unavailable
    }
    bool found = it != end && it.value();
    if (!found) {
        if (!iconPath(name, KIconLoader::Desktop, KIconLoader::MatchBest).isEmpty()) {
            found = true;
        }
        d->mIconAvailability.insert(name, found); // remember whether the icon is available or not
    }
    return found;
}

void KIconLoader::setCustomPalette(const QPalette &palette)
{
    d->mCustomPalette = true;
    d->mPalette = palette;
}

QPalette KIconLoader::customPalette() const
{
    return d->mCustomPalette ? d->mPalette : QPalette();
}

void KIconLoader::resetPalette()
{
    d->mCustomPalette = false;
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

#include "kiconloader.moc"
#include "moc_kiconloader.moc"
