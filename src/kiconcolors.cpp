/* vi: ts=8 sts=4 sw=4

    This file is part of the KDE project, module kdecore.
    SPDX-FileCopyrightText: 2000 Geert Jansen <jansen@kde.org>
    SPDX-FileCopyrightText: 2000 Antonio Larrosa <larrosa@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kiconcolors.h"
#include <KColorScheme>

static QString STYLESHEET_TEMPLATE()
{
    /* clang-format off */
    return QStringLiteral(".ColorScheme-Text { color:%1; }\
.ColorScheme-Background{ color:%2; }\
.ColorScheme-Highlight{ color:%3; }\
.ColorScheme-HighlightedText{ color:%4; }\
.ColorScheme-PositiveText{ color:%5; }\
.ColorScheme-NeutralText{ color:%6; }\
.ColorScheme-NegativeText{ color:%7; }");
    /* clang-format on */
}

class KIconColorsPrivate : public QSharedData
{
public:
    KIconColorsPrivate()
    {
    }
    KIconColorsPrivate(const KIconColorsPrivate &other)
        : QSharedData(other)
        , text(other.text)
        , background(other.background)
        , highlight(other.highlight)
        , highlightedText(other.highlightedText)
        , positiveText(other.positiveText)
        , neutralText(other.neutralText)
        , negativeText(other.negativeText)
    {
    }
    ~KIconColorsPrivate()
    {
    }

    QColor text;
    QColor background;
    QColor highlight;
    QColor highlightedText;
    QColor positiveText;
    QColor neutralText;
    QColor negativeText;
};

KIconColors::KIconColors()
    : KIconColors(QPalette())
{
}

KIconColors::KIconColors(const KIconColors &other)
    : d_ptr(other.d_ptr)
{
}

KIconColors KIconColors::operator=(const KIconColors &other)
{
    if (d_ptr != other.d_ptr) {
        d_ptr = other.d_ptr;
    }
    return *this;
}

KIconColors::KIconColors(const QColor &colors)
    : d_ptr(new KIconColorsPrivate)
{
    Q_D(KIconColors);
    d->text = colors;
    d->background = colors;
    d->highlight = colors;
    d->highlightedText = colors;
    d->positiveText = colors;
    d->neutralText = colors;
    d->negativeText = colors;
}

KIconColors::KIconColors(const QPalette &palette)
    : d_ptr(new KIconColorsPrivate)
{
    Q_D(KIconColors);
    d->text = palette.text().color();
    d->background = palette.window().color();
    d->highlight = palette.highlight().color();
    d->highlightedText = palette.highlightedText().color();

    KColorScheme scheme(QPalette::Active, KColorScheme::Window);
    d->positiveText = scheme.foreground(KColorScheme::PositiveText).color().name();
    d->neutralText = scheme.foreground(KColorScheme::NeutralText).color().name();
    d->negativeText = scheme.foreground(KColorScheme::NegativeText).color().name();
}

KIconColors::~KIconColors()
{
}

QString KIconColors::stylesheet(KIconLoader::States state) const
{
    Q_D(const KIconColors);
    return STYLESHEET_TEMPLATE().arg(state == KIconLoader::SelectedState ? d->highlightedText.name() : d->text.name(),
                                     state == KIconLoader::SelectedState ? d->highlight.name() : d->background.name(),
                                     state == KIconLoader::SelectedState ? d->highlightedText.name() : d->highlight.name(),
                                     state == KIconLoader::SelectedState ? d->highlight.name() : d->highlightedText.name(),
                                     d->positiveText.name(),
                                     d->neutralText.name(),
                                     d->negativeText.name());
}

QColor KIconColors::highlight() const
{
    Q_D(const KIconColors);
    return d->highlight;
}

QColor KIconColors::highlightedText() const
{
    Q_D(const KIconColors);
    return d->highlightedText;
}

QColor KIconColors::background() const
{
    Q_D(const KIconColors);
    return d->background;
}

QColor KIconColors::text() const
{
    Q_D(const KIconColors);
    return d->text;
}

QColor KIconColors::negativeText() const
{
    Q_D(const KIconColors);
    return d->negativeText;
}

QColor KIconColors::positiveText() const
{
    Q_D(const KIconColors);
    return d->positiveText;
}

QColor KIconColors::neutralText() const
{
    Q_D(const KIconColors);
    return d->neutralText;
}

void KIconColors::setText(const QColor &color)
{
    Q_D(KIconColors);
    d->text = color;
}

void KIconColors::setBackground(const QColor &color)
{
    Q_D(KIconColors);
    d->background = color;
}

void KIconColors::setHighlight(const QColor &color)
{
    Q_D(KIconColors);
    d->highlight = color;
}

void KIconColors::setHighlightedText(const QColor &color)
{
    Q_D(KIconColors);
    d->highlightedText = color;
}

void KIconColors::setNegativeText(const QColor &color)
{
    Q_D(KIconColors);
    d->negativeText = color;
}

void KIconColors::setNeutralText(const QColor &color)
{
    Q_D(KIconColors);
    d->neutralText = color;
}

void KIconColors::setPositiveText(const QColor &color)
{
    Q_D(KIconColors);
    d->positiveText = color;
}
