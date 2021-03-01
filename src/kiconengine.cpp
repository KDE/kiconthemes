/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2006 Hamish Rodda <rodda@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kiconengine.h"

#include <kiconloader.h>

#include <KIconTheme>
#include <QPainter>

KIconEngine::KIconEngine(const QString &iconName, KIconLoader *iconLoader, const QStringList &overlays)
    : mIconName(iconName)
    , mOverlays(overlays)
    , mIconLoader(iconLoader)
{
}

KIconEngine::KIconEngine(const QString &iconName, KIconLoader *iconLoader)
    : mIconName(iconName)
    , mIconLoader(iconLoader)
{
}

static inline int qIconModeToKIconState(QIcon::Mode mode)
{
    int kstate;
    switch (mode) {
    case QIcon::Normal:
        kstate = KIconLoader::DefaultState;
        break;
    case QIcon::Active:
        kstate = KIconLoader::ActiveState;
        break;
    case QIcon::Disabled:
        kstate = KIconLoader::DisabledState;
        break;
    case QIcon::Selected:
        kstate = KIconLoader::SelectedState;
        break;
    }
    return kstate;
}

QSize KIconEngine::actualSize(const QSize &size, QIcon::Mode mode, QIcon::State state)
{
    Q_UNUSED(state)
    Q_UNUSED(mode)
    const int iconSize = qMin(size.width(), size.height());
    return QSize(iconSize, iconSize);
}

void KIconEngine::paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state)
{
    if (!mIconLoader) {
        return;
    }

    const qreal dpr = painter->device()->devicePixelRatioF();
    painter->drawPixmap(rect, pixmap(rect.size() * dpr, mode, state));
}

QPixmap KIconEngine::createPixmap(const QSize &size, qreal scale, QIcon::Mode mode, QIcon::State state)
{
    Q_UNUSED(state)

    if (scale < 1) {
        scale = 1;
    }

    if (size.isEmpty()) {
        return QPixmap();
    }

    if (!mIconLoader) {
        QPixmap pm(size);
        pm.setDevicePixelRatio(scale);
        pm.fill(Qt::transparent);
        return pm;
    }

    const QSize scaledSize = size / scale;

    const int kstate = qIconModeToKIconState(mode);
    const int iconSize = qMin(scaledSize.width(), scaledSize.height());
    QPixmap pix = mIconLoader.data()->loadScaledIcon(mIconName, KIconLoader::Desktop, scale, iconSize, kstate, mOverlays);

    if (pix.size() == size) {
        return pix;
    }

    QPixmap pix2(size);
    auto dpr = pix.devicePixelRatio();
    pix2.setDevicePixelRatio(dpr);
    pix2.fill(QColor(0, 0, 0, 0));

    QPainter painter(&pix2);
    const QPoint newTopLeft((pix2.width() - pix.width()) / (2 * dpr), (pix2.height() - pix.height()) / (2 * dpr));
    painter.drawPixmap(newTopLeft, pix);

    return pix2;
}

QPixmap KIconEngine::pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state)
{
    return createPixmap(size, 1 /*scale*/, mode, state);
}

QString KIconEngine::iconName() const
{
    if (!mIconLoader || !mIconLoader->hasIcon(mIconName)) {
        return QString();
    }
    return mIconName;
}

Q_GLOBAL_STATIC_WITH_ARGS(QList<QSize>,
                          sSizes,
                          (QList<QSize>() << QSize(16, 16) << QSize(22, 22) << QSize(32, 32) << QSize(48, 48) << QSize(64, 64) << QSize(128, 128)
                                          << QSize(256, 256)))

QList<QSize> KIconEngine::availableSizes(QIcon::Mode mode, QIcon::State state) const
{
    Q_UNUSED(mode);
    Q_UNUSED(state);

    if (!mIconLoader) {
        return QList<QSize>();
    }

    const bool found = mIconLoader->hasIcon(mIconName);
    return found ? *sSizes : QList<QSize>();
}

QString KIconEngine::key() const
{
    return QStringLiteral("KIconEngine");
}

QIconEngine *KIconEngine::clone() const
{
    return new KIconEngine(mIconName, mIconLoader.data(), mOverlays);
}

bool KIconEngine::read(QDataStream &in)
{
    in >> mIconName >> mOverlays;
    return true;
}

bool KIconEngine::write(QDataStream &out) const
{
    out << mIconName << mOverlays;
    return true;
}

void KIconEngine::virtual_hook(int id, void *data)
{
    if (id == QIconEngine::IsNullHook) {
        *reinterpret_cast<bool *>(data) = !mIconLoader || !mIconLoader->hasIcon(mIconName);
    }
    if (id == QIconEngine::ScaledPixmapHook) {
        auto *info = reinterpret_cast<ScaledPixmapArgument *>(data);
        info->pixmap = createPixmap(info->size, info->scale, info->mode, info->state);
        return;
    }
    QIconEngine::virtual_hook(id, data);
}
