/* vi: ts=8 sts=4 sw=4

    kicontheme.cpp: Lowlevel icon theme handling.

    This file is part of the KDE project, module kdecore.
    SPDX-FileCopyrightText: 2000 Geert Jansen <jansen@kde.org>
    SPDX-FileCopyrightText: 2000 Antonio Larrosa <larrosa@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kicontheme.h"

#include "debug.h"

#include <KConfigGroup>
#include <KLocalizedString> // KLocalizedString::localizedFilePath. Need such functionality in, hmm, QLocale? QStandardPaths?
#include <KSharedConfig>

#include <QAction>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QResource>
#include <QSet>

#include <qplatformdefs.h>

#include <cmath>

Q_GLOBAL_STATIC(QString, _themeOverride)

// Support for icon themes in RCC files.
// The intended use case is standalone apps on Windows / MacOS / etc.
// For this reason we use AppDataLocation: BINDIR/data on Windows, Resources on OS X
void initRCCIconTheme()
{
    QMap<QString, QString> themeMap;
    themeMap.insert("breeze", "breeze-icons.rcc");
    themeMap.insert("breeze-dark", "breeze-icons-dark.rcc");

    QMapIterator<QString, QString> i(themeMap);
    bool sucess;
    while (i.hasNext()) {
        i.next();
        const QString iconSubdir = "/icons/" + i.key();
        QString themePath = QStandardPaths::locate(QStandardPaths::GenericDataLocation, iconSubdir, QStandardPaths::LocateDirectory);
        if (!themePath.isEmpty()) {
            QDir dir(themePath);
            if (!dir.exists(QStringLiteral("index.theme")) && dir.exists(i.value())) {
                // we do not have icon files directly available,
                // but an *.rcc with them. Try to register...
                const QString iconThemeRcc = themePath + "/" + i.value();
                if (QResource::registerResource(iconThemeRcc, iconSubdir)) {
                    if (QFileInfo::exists(QLatin1Char(':') + iconSubdir + QStringLiteral("/index.theme"))) {
                        sucess = true;
                    } else {
                        qWarning() << "No index.theme found in" << iconThemeRcc;
                        QResource::unregisterResource(themePath, iconSubdir);
                    }
                } else {
                    qWarning() << "Invalid rcc file" << i.key();
                }
            }
        }
    }
#if defined(Q_OS_WIN) || defined (Q_OS_MACOS)
     const QString iconThemeRcc = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("icontheme.rcc"));
#else
     const QString iconThemeRcc = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("icontheme.rcc"));
#endif
    if (!sucess && !iconThemeRcc.isEmpty()) {
        const QString iconThemeName = QStringLiteral("kf5_rcc_theme");
        const QString iconSubdir = QStringLiteral("/icons/") + iconThemeName;
        if (QResource::registerResource(iconThemeRcc, iconSubdir)) {
            if (QFileInfo::exists(QLatin1Char(':') + iconSubdir + QStringLiteral("/index.theme"))) {
                // Tell Qt about the theme
                // Note that since qtbase commit a8621a3f8, this means the QPA (i.e. KIconLoader) will NOT be used.
                QIcon::setThemeName(iconThemeName); // Qt looks under :/icons automatically
                // Tell KIconTheme about the theme, in case KIconLoader is used directly
                *_themeOverride() = iconThemeName;
            } else {
                qWarning() << "No index.theme found in" << iconThemeRcc;
                QResource::unregisterResource(iconThemeRcc, iconSubdir);
            }
        } else {
            qWarning() << "Invalid rcc file" << iconThemeRcc;
        }
    }
}
Q_COREAPP_STARTUP_FUNCTION(initRCCIconTheme)

// Set the icon theme fallback to breeze
// Most of our apps use "lots" of icons that most of the times
// are only available with breeze, we still honour the user icon
// theme but if the icon is not found there, we go to breeze
// since it's almost sure it'll be there
static void setBreezeFallback()
{
    QIcon::setFallbackThemeName(QStringLiteral("breeze"));
}

Q_COREAPP_STARTUP_FUNCTION(setBreezeFallback)
class KIconThemeDir;
class KIconThemePrivate
{
public:
    QString example, screenshot;
    bool hidden;
    KSharedConfig::Ptr sharedConfig;

    int mDefSize[6];
    QList<int> mSizes[6];

    int mDepth;
    QString mDir, mName, mInternalName, mDesc;
    QStringList mInherits;
    QStringList mExtensions;
    QVector<KIconThemeDir *> mDirs;
    QVector<KIconThemeDir *> mScaledDirs;
    bool followsColorScheme : 1;

    /// Searches the given dirs vector for a matching icon
    QString iconPath(const QVector<KIconThemeDir *> &dirs, const QString &name, int size, qreal scale, KIconLoader::MatchType match) const;
};
Q_GLOBAL_STATIC(QString, _theme)
Q_GLOBAL_STATIC(QStringList, _theme_list)

/**
 * A subdirectory in an icon theme.
 */
class KIconThemeDir
{
public:
    KIconThemeDir(const QString &basedir, const QString &themedir, const KConfigGroup &config);

    bool isValid() const
    {
        return mbValid;
    }
    QString iconPath(const QString &name) const;
    QStringList iconList() const;
    QString constructFileName(const QString &file) const
    {
        return mBaseDir + mThemeDir + QLatin1Char('/') + file;
    }

    KIconLoader::Context context() const
    {
        return mContext;
    }
    KIconLoader::Type type() const
    {
        return mType;
    }
    int size() const
    {
        return mSize;
    }
    int scale() const
    {
        return mScale;
    }
    int minSize() const
    {
        return mMinSize;
    }
    int maxSize() const
    {
        return mMaxSize;
    }
    int threshold() const
    {
        return mThreshold;
    }

private:
    bool mbValid;
    KIconLoader::Type mType;
    KIconLoader::Context mContext;
    int mSize, mScale, mMinSize, mMaxSize;
    int mThreshold;

    const QString mBaseDir;
    const QString mThemeDir;
};

QString KIconThemePrivate::iconPath(const QVector<KIconThemeDir *> &dirs, const QString &name, int size, qreal scale, KIconLoader::MatchType match) const
{
    QString path;
    QString tempPath; // used to cache icon path if it exists

    int delta = -INT_MAX; // current icon size delta of 'icon'
    int dw = INT_MAX; // icon size delta of current directory

    // Rather downsample than upsample
    int integerScale = std::ceil(scale);

    // Search the directory that contains the icon which matches best to the requested
    // size. If there is no directory which matches exactly to the requested size, the
    // following criteria get applied:
    // - Take a directory having icons with a minimum difference to the requested size.
    // - Prefer directories that allow a downscaling even if the difference to
    //   the requested size is bigger than a directory where an upscaling is required.
    for (KIconThemeDir *dir : dirs) {
        if (dir->scale() != integerScale) {
            continue;
        }

        if (match == KIconLoader::MatchExact) {
            if ((dir->type() == KIconLoader::Fixed) && (dir->size() != size)) {
                continue;
            }
            if ((dir->type() == KIconLoader::Scalable) //
                && ((size < dir->minSize()) || (size > dir->maxSize()))) {
                continue;
            }
            if ((dir->type() == KIconLoader::Threshold) //
                && (abs(dir->size() - size) > dir->threshold())) {
                continue;
            }
        } else {
            // dw < 0 means need to scale up to get an icon of the requested size.
            // Upscaling should only be done if no larger icon is available.
            if (dir->type() == KIconLoader::Fixed) {
                dw = dir->size() - size;
            } else if (dir->type() == KIconLoader::Scalable) {
                if (size < dir->minSize()) {
                    dw = dir->minSize() - size;
                } else if (size > dir->maxSize()) {
                    dw = dir->maxSize() - size;
                } else {
                    dw = 0;
                }
            } else if (dir->type() == KIconLoader::Threshold) {
                if (size < dir->size() - dir->threshold()) {
                    dw = dir->size() - dir->threshold() - size;
                } else if (size > dir->size() + dir->threshold()) {
                    dw = dir->size() + dir->threshold() - size;
                } else {
                    dw = 0;
                }
            }
            // Usually if the delta (= 'dw') of the current directory is
            // not smaller than the delta (= 'delta') of the currently best
            // matching icon, this candidate can be skipped. But skipping
            // the candidate may only be done, if this does not imply
            // in an upscaling of the icon (it is OK to use a directory with
            // smaller icons that what we've already found, however).
            if ((abs(dw) >= abs(delta)) && ((dw < 0) || (delta > 0))) {
                continue;
            }
        }

        // cache the result of iconPath() call which checks if file exists
        tempPath = dir->iconPath(name);

        if (tempPath.isEmpty()) {
            continue;
        }

        path = tempPath;

        // if we got in MatchExact that far, we find no better
        if (match == KIconLoader::MatchExact) {
            return path;
        }
        delta = dw;
        if (delta == 0) {
            return path; // We won't find a better match anyway
        }
    }
    return path;
}

KIconTheme::KIconTheme(const QString &name, const QString &appName, const QString &basePathHint)
    : d(new KIconThemePrivate)
{
    d->mInternalName = name;

    QStringList themeDirs;
    QSet<QString> addedDirs; // Used for avoiding duplicates.

    // Applications can have local additions to the global "locolor" and
    // "hicolor" icon themes. For these, the _global_ theme description
    // files are used..

    /* clang-format off */
    if (!appName.isEmpty()
        && (name == defaultThemeName()
            || name == QLatin1String("hicolor")
            || name == QLatin1String("locolor"))) { /* clang-format on */
        const QStringList icnlibs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
        for (QStringList::ConstIterator it = icnlibs.constBegin(), total = icnlibs.constEnd(); it != total; ++it) {
            const QString cDir = *it + QLatin1Char('/') + appName + QStringLiteral("/icons/") + name + QLatin1Char('/');
            if (QFileInfo::exists(cDir)) {
                themeDirs += cDir;
            }
        }

        if (!basePathHint.isEmpty()) {
            // Checks for dir existing are done below
            themeDirs += basePathHint + QLatin1Char('/') + name + QLatin1Char('/');
        }
    }

    // Find the theme description file. These are either locally in the :/icons resource path or global.
    QStringList icnlibs;

    // local embedded icons have preference
    icnlibs << QStringLiteral(":/icons");

    // global icons
    icnlibs += QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons"), QStandardPaths::LocateDirectory);

    // These are not in the icon spec, but e.g. GNOME puts some icons there anyway.
    icnlibs += QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("pixmaps"), QStandardPaths::LocateDirectory);

    QString fileName;
    QString mainSection;
    for (QStringList::ConstIterator it = icnlibs.constBegin(), total = icnlibs.constEnd(); it != total; ++it) {
        const QString cDir = *it + QLatin1Char('/') + name + QLatin1Char('/');
        if (QDir(cDir).exists()) {
            themeDirs += cDir;
            if (d->mDir.isEmpty()) {
                if (QFileInfo::exists(cDir + QStringLiteral("index.theme"))) {
                    d->mDir = cDir;
                    fileName = d->mDir + QStringLiteral("index.theme");
                    mainSection = QStringLiteral("Icon Theme");
                } else if (QFileInfo::exists(cDir + QStringLiteral("index.desktop"))) {
                    d->mDir = cDir;
                    fileName = d->mDir + QStringLiteral("index.desktop");
                    mainSection = QStringLiteral("KDE Icon Theme");
                }
            }
        }
    }

    if (d->mDir.isEmpty()) {
        qCDebug(KICONTHEMES) << "Icon theme" << name << "not found.";
        return;
    }

    // Use KSharedConfig to avoid parsing the file many times, from each component.
    // Need to keep a ref to it to make this useful
    d->sharedConfig = KSharedConfig::openConfig(fileName, KConfig::NoGlobals);

    KConfigGroup cfg(d->sharedConfig, mainSection);
    d->mName = cfg.readEntry("Name");
    d->mDesc = cfg.readEntry("Comment");
    d->mDepth = cfg.readEntry("DisplayDepth", 32);
    d->mInherits = cfg.readEntry("Inherits", QStringList());
    if (name != defaultThemeName()) {
        for (QStringList::Iterator it = d->mInherits.begin(), total = d->mInherits.end(); it != total; ++it) {
            if (*it == QLatin1String("default")) {
                *it = defaultThemeName();
            }
        }
    }

    d->hidden = cfg.readEntry("Hidden", false);
    d->followsColorScheme = cfg.readEntry("FollowsColorScheme", false);
    d->example = cfg.readPathEntry("Example", QString());
    d->screenshot = cfg.readPathEntry("ScreenShot", QString());
    d->mExtensions =
        cfg.readEntry("KDE-Extensions", QStringList{QStringLiteral(".png"), QStringLiteral(".svgz"), QStringLiteral(".svg"), QStringLiteral(".xpm")});

    const QStringList dirs = cfg.readPathEntry("Directories", QStringList()) + cfg.readPathEntry("ScaledDirectories", QStringList());
    for (QStringList::ConstIterator it = dirs.begin(); it != dirs.end(); ++it) {
        KConfigGroup cg(d->sharedConfig, *it);
        for (QStringList::ConstIterator itDir = themeDirs.constBegin(); itDir != themeDirs.constEnd(); ++itDir) {
            const QString currentDir(*itDir + *it + QLatin1Char('/'));
            if (!addedDirs.contains(currentDir) && QDir(currentDir).exists()) {
                addedDirs.insert(currentDir);
                KIconThemeDir *dir = new KIconThemeDir(*itDir, *it, cg);
                if (dir->isValid()) {
                    if (dir->scale() > 1) {
                        d->mScaledDirs.append(dir);
                    } else {
                        d->mDirs.append(dir);
                    }
                } else {
                    delete dir;
                }
            }
        }
    }

    QStringList groups;
    groups += QStringLiteral("Desktop");
    groups += QStringLiteral("Toolbar");
    groups += QStringLiteral("MainToolbar");
    groups += QStringLiteral("Small");
    groups += QStringLiteral("Panel");
    groups += QStringLiteral("Dialog");
    const int defDefSizes[] = {32, 22, 22, 16, 48, 32};
    KConfigGroup cg(d->sharedConfig, mainSection);
    for (int i = 0; i < groups.size(); ++i) {
        const QString group = groups.at(i);
        d->mDefSize[i] = cg.readEntry(group + QStringLiteral("Default"), defDefSizes[i]);
        d->mSizes[i] = cg.readEntry(group + QStringLiteral("Sizes"), QList<int>());
    }
}

KIconTheme::~KIconTheme()
{
    qDeleteAll(d->mDirs);
    qDeleteAll(d->mScaledDirs);
}

QString KIconTheme::name() const
{
    return d->mName;
}

QString KIconTheme::internalName() const
{
    return d->mInternalName;
}

QString KIconTheme::description() const
{
    return d->mDesc;
}

QString KIconTheme::example() const
{
    return d->example;
}

QString KIconTheme::screenshot() const
{
    return d->screenshot;
}

QString KIconTheme::dir() const
{
    return d->mDir;
}

QStringList KIconTheme::inherits() const
{
    return d->mInherits;
}

bool KIconTheme::isValid() const
{
    return !d->mDirs.isEmpty() || !d->mScaledDirs.isEmpty();
}

bool KIconTheme::isHidden() const
{
    return d->hidden;
}

int KIconTheme::depth() const
{
    return d->mDepth;
}

int KIconTheme::defaultSize(KIconLoader::Group group) const
{
    if ((group < 0) || (group >= KIconLoader::LastGroup)) {
        qWarning() << "Illegal icon group: " << group;
        return -1;
    }
    return d->mDefSize[group];
}

QList<int> KIconTheme::querySizes(KIconLoader::Group group) const
{
    if ((group < 0) || (group >= KIconLoader::LastGroup)) {
        qWarning() << "Illegal icon group: " << group;
        return QList<int>();
    }
    return d->mSizes[group];
}

QStringList KIconTheme::queryIcons(int size, KIconLoader::Context context) const
{
    // Try to find exact match
    QStringList result;
    const auto listDirs = d->mDirs + d->mScaledDirs;
    for (KIconThemeDir *dir : listDirs) {
        if ((context != KIconLoader::Any) && (context != dir->context())) {
            continue;
        }
        if ((dir->type() == KIconLoader::Fixed) && (dir->size() == size)) {
            result += dir->iconList();
            continue;
        }
        if (dir->type() == KIconLoader::Scalable //
            && size >= dir->minSize() //
            && size <= dir->maxSize()) {
            result += dir->iconList();
            continue;
        }
        if ((dir->type() == KIconLoader::Threshold) //
            && (abs(size - dir->size()) < dir->threshold())) {
            result += dir->iconList();
        }
    }

    return result;

    /*
        int delta = 1000, dw;

        // Find close match
        KIconThemeDir *best = 0L;
        for(int i=0; i<d->mDirs.size(); ++i) {
            dir = d->mDirs.at(i);
            if ((context != KIconLoader::Any) && (context != dir->context())) {
                continue;
            }
            dw = dir->size() - size;
            if ((dw > 6) || (abs(dw) >= abs(delta)))
                continue;
            delta = dw;
            best = dir;
        }
        if (best == 0L) {
            return QStringList();
        }

        return best->iconList();
        */
}

QStringList KIconTheme::queryIconsByContext(int size, KIconLoader::Context context) const
{
    int dw;

    // We want all the icons for a given context, but we prefer icons
    // of size size . Note that this may (will) include duplicate icons
    // QStringList iconlist[34]; // 33 == 48-16+1
    QStringList iconlist[128]; // 33 == 48-16+1
    // Usually, only the 0, 6 (22-16), 10 (32-22), 16 (48-32 or 32-16),
    // 26 (48-22) and 32 (48-16) will be used, but who knows if someone
    // will make icon themes with different icon sizes.
    const auto listDirs = d->mDirs + d->mScaledDirs;
    for (KIconThemeDir *dir : listDirs) {
        if ((context != KIconLoader::Any) && (context != dir->context())) {
            continue;
        }
        dw = abs(dir->size() - size);
        iconlist[(dw < 127) ? dw : 127] += dir->iconList();
    }

    QStringList iconlistResult;
    for (int i = 0; i < 128; i++) {
        iconlistResult += iconlist[i];
    }

    return iconlistResult;
}

bool KIconTheme::hasContext(KIconLoader::Context context) const
{
    const auto listDirs = d->mDirs + d->mScaledDirs;
    for (KIconThemeDir *dir : listDirs) {
        if ((context == KIconLoader::Any) || (context == dir->context())) {
            return true;
        }
    }
    return false;
}

QString KIconTheme::iconPathByName(const QString &iconName, int size, KIconLoader::MatchType match) const
{
    return iconPathByName(iconName, size, match, 1 /*scale*/);
}

QString KIconTheme::iconPathByName(const QString &iconName, int size, KIconLoader::MatchType match, qreal scale) const
{
    for (const QString &current : std::as_const(d->mExtensions)) {
        const QString path = iconPath(iconName + current, size, match, scale);
        if (!path.isEmpty()) {
            return path;
        }
    }
    return QString();
}

bool KIconTheme::followsColorScheme() const
{
    return d->followsColorScheme;
}

QString KIconTheme::iconPath(const QString &name, int size, KIconLoader::MatchType match) const
{
    return iconPath(name, size, match, 1 /*scale*/);
}

QString KIconTheme::iconPath(const QString &name, int size, KIconLoader::MatchType match, qreal scale) const
{
    // first look for a scaled image at exactly the requested size
    QString path = d->iconPath(d->mScaledDirs, name, size, scale, KIconLoader::MatchExact);

    // then look for an unscaled one but request it at larger size so it doesn't become blurry
    if (path.isEmpty()) {
        path = d->iconPath(d->mDirs, name, size * scale, 1, match);
    }
    return path;
}

// static
QString KIconTheme::current()
{
    // Static pointers because of unloading problems wrt DSO's.
    if (_themeOverride && !_themeOverride->isEmpty()) {
        *_theme() = *_themeOverride();
    }
    if (!_theme()->isEmpty()) {
        return *_theme();
    }

    QString theme;
    // Check application specific config for a theme setting.
    KConfigGroup app_cg(KSharedConfig::openConfig(QString(), KConfig::NoGlobals), "Icons");
    theme = app_cg.readEntry("Theme", QString());
    if (theme.isEmpty() || theme == QLatin1String("hicolor")) {
        // No theme, try to use Qt's. A Platform plugin might have set
        // a good theme there.
        theme = QIcon::themeName();
    }
    if (theme.isEmpty() || theme == QLatin1String("hicolor")) {
        // Still no theme, try config with kdeglobals.
        KConfigGroup cg(KSharedConfig::openConfig(), "Icons");
        theme = cg.readEntry("Theme", QStringLiteral("breeze"));
    }
    if (theme.isEmpty() || theme == QLatin1String("hicolor")) {
        // Still no good theme, use default.
        theme = defaultThemeName();
    }
    *_theme() = theme;
    return *_theme();
}

void KIconTheme::forceThemeForTests(const QString &themeName)
{
    *_themeOverride() = themeName;
    _theme()->clear(); // ::current sets this again based on conditions
}

// static
QStringList KIconTheme::list()
{
    // Static pointer because of unloading problems wrt DSO's.
    if (!_theme_list()->isEmpty()) {
        return *_theme_list();
    }

    // Find the theme description file. These are either locally in the :/icons resource path or global.
    QStringList icnlibs;

    // local embedded icons have preference
    icnlibs << QStringLiteral(":/icons");

    // global icons
    icnlibs += QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("icons"), QStandardPaths::LocateDirectory);

    // These are not in the icon spec, but e.g. GNOME puts some icons there anyway.
    icnlibs += QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("pixmaps"), QStandardPaths::LocateDirectory);

    for (const QString &it : std::as_const(icnlibs)) {
        QDir dir(it);
        const QStringList lst = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (QStringList::ConstIterator it2 = lst.begin(), total = lst.end(); it2 != total; ++it2) {
            if ((*it2).startsWith(QLatin1String("default."))) {
                continue;
            }
            if (!QFileInfo::exists(it + QLatin1Char('/') + *it2 + QLatin1String("/index.desktop"))
                && !QFileInfo::exists(it + QLatin1Char('/') + *it2 + QLatin1String("/index.theme"))) {
                continue;
            }
            KIconTheme oink(*it2);
            if (!oink.isValid()) {
                continue;
            }

            if (!_theme_list()->contains(*it2)) {
                _theme_list()->append(*it2);
            }
        }
    }
    return *_theme_list();
}

// static
void KIconTheme::reconfigure()
{
    _theme()->clear();
    _theme_list()->clear();
}

// static
QString KIconTheme::defaultThemeName()
{
    return QStringLiteral("hicolor");
}

#if KICONTHEMES_BUILD_DEPRECATED_SINCE(5, 64)
void KIconTheme::assignIconsToContextMenu(ContextMenus type, QList<QAction *> actions)
{
    switch (type) {
    // FIXME: This code depends on Qt's action ordering.
    case TextEditor:
        enum {
            UndoAct,
            RedoAct,
            Separator1,
            CutAct,
            CopyAct,
            PasteAct,
            DeleteAct,
            ClearAct,
            Separator2,
            SelectAllAct,
            NCountActs,
        };

        if (actions.count() < NCountActs) {
            return;
        }

        actions[UndoAct]->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo")));
        actions[RedoAct]->setIcon(QIcon::fromTheme(QStringLiteral("edit-redo")));
        actions[CutAct]->setIcon(QIcon::fromTheme(QStringLiteral("edit-cut")));
        actions[CopyAct]->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
        actions[PasteAct]->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
        actions[ClearAct]->setIcon(QIcon::fromTheme(QStringLiteral("edit-clear")));
        actions[DeleteAct]->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
        actions[SelectAllAct]->setIcon(QIcon::fromTheme(QStringLiteral("edit-select-all")));
        break;

    case ReadOnlyText:
        if (actions.count() < 1) {
            return;
        }

        actions[0]->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
        break;
    }
}
#endif

/*** KIconThemeDir ***/

KIconThemeDir::KIconThemeDir(const QString &basedir, const QString &themedir, const KConfigGroup &config)
    : mbValid(false)
    , mType(KIconLoader::Fixed)
    , mSize(config.readEntry("Size", 0))
    , mScale(config.readEntry("Scale", 1))
    , mMinSize(1) // just set the variables to something
    , mMaxSize(50) // meaningful in case someone calls minSize or maxSize
    , mThreshold(2)
    , mBaseDir(basedir)
    , mThemeDir(themedir)
{
    if (mSize == 0) {
        return;
    }

    QString tmp = config.readEntry(QStringLiteral("Context"));
    if (tmp == QLatin1String("Devices")) {
        mContext = KIconLoader::Device;
    } else if (tmp == QLatin1String("MimeTypes")) {
        mContext = KIconLoader::MimeType;
#if KICONTHEMES_BUILD_DEPRECATED_SINCE(4, 8)
    } else if (tmp == QLatin1String("FileSystems")) {
        mContext = KIconLoader::FileSystem;
#endif
    } else if (tmp == QLatin1String("Applications")) {
        mContext = KIconLoader::Application;
    } else if (tmp == QLatin1String("Actions")) {
        mContext = KIconLoader::Action;
    } else if (tmp == QLatin1String("Animations")) {
        mContext = KIconLoader::Animation;
    } else if (tmp == QLatin1String("Categories")) {
        mContext = KIconLoader::Category;
    } else if (tmp == QLatin1String("Emblems")) {
        mContext = KIconLoader::Emblem;
    } else if (tmp == QLatin1String("Emotes")) {
        mContext = KIconLoader::Emote;
    } else if (tmp == QLatin1String("International")) {
        mContext = KIconLoader::International;
    } else if (tmp == QLatin1String("Places")) {
        mContext = KIconLoader::Place;
    } else if (tmp == QLatin1String("Status")) {
        mContext = KIconLoader::StatusIcon;
    } else if (tmp == QLatin1String("Stock")) { // invalid, but often present context, skip warning
        return;
    } else if (tmp == QLatin1String("Legacy")) { // invalid, but often present context for Adwaita, skip warning
        return;
    } else if (tmp == QLatin1String("UI")) { // invalid, but often present context for Adwaita, skip warning
        return;
    } else if (tmp.isEmpty()) {
        // do nothing. key not required
    } else {
        qCDebug(KICONTHEMES) << "Invalid Context=" << tmp << "line for icon theme: " << constructFileName(QString());
        return;
    }
    tmp = config.readEntry(QStringLiteral("Type"), QStringLiteral("Threshold"));
    if (tmp == QLatin1String("Fixed")) {
        mType = KIconLoader::Fixed;
    } else if (tmp == QLatin1String("Scalable")) {
        mType = KIconLoader::Scalable;
    } else if (tmp == QLatin1String("Threshold")) {
        mType = KIconLoader::Threshold;
    } else {
        qCDebug(KICONTHEMES) << "Invalid Type=" << tmp << "line for icon theme: " << constructFileName(QString());
        return;
    }
    if (mType == KIconLoader::Scalable) {
        mMinSize = config.readEntry(QStringLiteral("MinSize"), mSize);
        mMaxSize = config.readEntry(QStringLiteral("MaxSize"), mSize);
    } else if (mType == KIconLoader::Threshold) {
        mThreshold = config.readEntry(QStringLiteral("Threshold"), 2);
    }
    mbValid = true;
}

QString KIconThemeDir::iconPath(const QString &name) const
{
    if (!mbValid) {
        return QString();
    }

    const QString file = constructFileName(name);
    if (QFileInfo::exists(file)) {
        return KLocalizedString::localizedFilePath(file);
    }

    return QString();
}

QStringList KIconThemeDir::iconList() const
{
    const QDir icondir = constructFileName(QString());

    const QStringList formats = QStringList() << QStringLiteral("*.png") << QStringLiteral("*.svg") << QStringLiteral("*.svgz") << QStringLiteral("*.xpm");
    const QStringList lst = icondir.entryList(formats, QDir::Files);

    QStringList result;
    result.reserve(lst.size());
    for (const QString &file : lst) {
        result += constructFileName(file);
    }
    return result;
}
