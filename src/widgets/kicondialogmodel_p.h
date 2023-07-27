/*
    SPDX-FileCopyrightText: 2021 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KICONDIALOGMODEL_P_H
#define KICONDIALOGMODEL_P_H

#include <QAbstractListModel>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

struct KIconDialogModelData {
    QString name;
    QString path;
    QPixmap pixmap;
};
Q_DECLARE_TYPEINFO(KIconDialogModelData, Q_RELOCATABLE_TYPE);

class KIconDialogModel : public QAbstractListModel
{
    Q_OBJECT

public:
    KIconDialogModel(QObject *parent);
    ~KIconDialogModel() override;

    enum Roles { PathRole = Qt::UserRole };

    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal dpr);

    QSize iconSize() const;
    void setIconSize(const QSize &iconSize);

    void load(const QStringList &paths);

    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    void loadPixmap(const QModelIndex &index);

    QVector<KIconDialogModelData> m_data;

    qreal m_dpr = 1;
    QSize m_iconSize;
};

#endif // KICONDIALOGMODEL_P_H
