/* vi: ts=8 sts=4 sw=4
 *
 * This file is part of the KDE project, module kfile.
 * Copyright (C) 2000 Geert Jansen <jansen@kde.org>
 *           (C) 2000 Kurt Granroth <granroth@kde.org>
 *           (C) 1997 Christoph Neerfeld <chris@kde.org>
 *           (C) 2002 Carsten Pfeiffer <pfeiffer@kde.org>
 *
 * This is free software; it comes under the GNU Library General
 * Public License, version 2. See the file "COPYING.LIB" for the
 * exact licensing terms.
 */

#include "kiconbutton.h"

#include <QtCore/QFileInfo>

#include "kicondialog.h"

class Q_DECL_HIDDEN KIconButton::KIconButtonPrivate
{
public:
    KIconButtonPrivate(KIconButton *qq, KIconLoader *loader);
    ~KIconButtonPrivate();

    // slots
    void _k_slotChangeIcon();
    void _k_newIconName(const QString &);

    KIconButton *q;

    int iconSize;
    int buttonIconSize;
    bool m_bStrictIconSize;

    bool mbUser;
    KIconLoader::Group mGroup;
    KIconLoader::Context mContext;

    QString mIcon;
    KIconDialog *mpDialog;
    KIconLoader *mpLoader;
};

/*
 * KIconButton: A "choose icon" pushbutton.
 */

KIconButton::KIconButton(QWidget *parent)
    : QPushButton(parent), d(new KIconButtonPrivate(this, KIconLoader::global()))
{
    QPushButton::setIconSize(QSize(48, 48));
}

KIconButton::KIconButton(KIconLoader *loader, QWidget *parent)
    : QPushButton(parent), d(new KIconButtonPrivate(this, loader))
{
    QPushButton::setIconSize(QSize(48, 48));
}

KIconButton::KIconButtonPrivate::KIconButtonPrivate(KIconButton *qq, KIconLoader *loader)
    : q(qq)
{
    m_bStrictIconSize = false;
    iconSize = 0; // let KIconLoader choose the default
    buttonIconSize = -1; //When buttonIconSize is -1, iconSize will be used for the button

    mGroup = KIconLoader::Desktop;
    mContext = KIconLoader::Application;
    mbUser = false;

    mpLoader = loader;
    mpDialog = nullptr;
    connect(q, &KIconButton::clicked, q, [this]() {_k_slotChangeIcon();});
}

KIconButton::KIconButtonPrivate::~KIconButtonPrivate()
{
    delete mpDialog;
}

KIconButton::~KIconButton()
{
    delete d;
}

void KIconButton::setStrictIconSize(bool b)
{
    d->m_bStrictIconSize = b;
}

bool KIconButton::strictIconSize() const
{
    return d->m_bStrictIconSize;
}

void KIconButton::setIconSize(int size)
{
    if (d->buttonIconSize == -1) {
        QPushButton::setIconSize(QSize(size, size));
    }

    d->iconSize = size;
}

int KIconButton::iconSize() const
{
    return d->iconSize;
}

void KIconButton::setButtonIconSize(int size)
{
    QPushButton::setIconSize(QSize(size, size));
    d->buttonIconSize = size;
}

int KIconButton::buttonIconSize() const
{
    return QPushButton::iconSize().height();
}

void KIconButton::setIconType(KIconLoader::Group group, KIconLoader::Context context, bool user)
{
    d->mGroup = group;
    d->mContext = context;
    d->mbUser = user;
}

void KIconButton::setIcon(const QString &icon)
{
    d->mIcon = icon;
    setIcon(QIcon::fromTheme(d->mIcon));

    if (!d->mpDialog) {
        d->mpDialog = new KIconDialog(d->mpLoader, this);
        connect(d->mpDialog, &KIconDialog::newIconName, this, [this](const QString &iconName) { d->_k_newIconName(iconName); });
    }

    if (d->mbUser) {
        d->mpDialog->setCustomLocation(QFileInfo(d->mpLoader->iconPath(d->mIcon, d->mGroup, true)).absolutePath());
    }
}

void KIconButton::setIcon(const QIcon &icon)
{
    QPushButton::setIcon(icon);
}

void KIconButton::resetIcon()
{
    d->mIcon.clear();
    setIcon(QIcon());
}

const QString &KIconButton::icon() const
{
    return d->mIcon;
}

void KIconButton::KIconButtonPrivate::_k_slotChangeIcon()
{
    if (!mpDialog) {
        mpDialog = new KIconDialog(mpLoader, q);
        connect(mpDialog, &KIconDialog::newIconName, q, [this](const QString &iconName) { _k_newIconName(iconName); });
    }

    mpDialog->setup(mGroup, mContext, m_bStrictIconSize, iconSize, mbUser);
    mpDialog->showDialog();
}

void KIconButton::KIconButtonPrivate::_k_newIconName(const QString &name)
{
    if (name.isEmpty()) {
        return;
    }

    q->setIcon(QIcon::fromTheme(name));
    mIcon = name;

    if (mbUser) {
        mpDialog->setCustomLocation(QFileInfo(mpLoader->iconPath(mIcon, mGroup, true)).absolutePath());
    }

    emit q->iconChanged(name);
}

#include "moc_kiconbutton.cpp"
