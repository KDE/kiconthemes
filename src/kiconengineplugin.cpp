/*
 * kiconengineplugin.cpp: Qt plugin providing the ability to create a KIconEngine
 *
 * This file is part of the KDE project, module kdeui.
 * Copyright (C) 2018 Fabian Vogt <fabian@ritter-vogt.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <QIconEnginePlugin>

#include <KIconEngine>
#include <KIconLoader>

QT_BEGIN_NAMESPACE

class KIconEnginePlugin : public QIconEnginePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QIconEngineFactoryInterface" FILE "kiconengineplugin.json")

public:
    QIconEngine *create(const QString &file) override
    {
        return new KIconEngine(file, KIconLoader::global());
    }
};

QT_END_NAMESPACE

#include "kiconengineplugin.moc"
