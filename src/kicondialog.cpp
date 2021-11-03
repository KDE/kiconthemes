/* vi: ts=8 sts=4 sw=4

    This file is part of the KDE project, module kfile.
    SPDX-FileCopyrightText: 2000 Geert Jansen <jansen@kde.org>
    SPDX-FileCopyrightText: 2000 Kurt Granroth <granroth@kde.org>
    SPDX-FileCopyrightText: 1997 Christoph Neerfeld <chris@kde.org>
    SPDX-FileCopyrightText: 2002 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2021 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kicondialog.h"
#include "kicondialog_p.h"
#include "kicondialogmodel_p.h"

#include <KLocalizedString>

#include <QAbstractListModel>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPainter>
#include <QScrollBar>
#include <QStandardItemModel> // for manipulatig QComboBox
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QVector>

#include <algorithm>
#include <math.h>

static const int s_edgePad = 3;

KIconDialogModel::KIconDialogModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

KIconDialogModel::~KIconDialogModel() = default;

qreal KIconDialogModel::devicePixelRatio() const
{
    return m_dpr;
}

void KIconDialogModel::setDevicePixelRatio(qreal dpr)
{
    m_dpr = dpr;
}

QSize KIconDialogModel::iconSize() const
{
    return m_iconSize;
}

void KIconDialogModel::setIconSize(const QSize &iconSize)
{
    m_iconSize = iconSize;
}

void KIconDialogModel::load(const QStringList &paths)
{
    beginResetModel();

    m_data.clear();
    m_data.reserve(paths.count());

    for (const QString &path : paths) {
        const QFileInfo fi(path);

        KIconDialogModelData item;
        item.name = fi.completeBaseName();
        item.path = path;
        // pixmap is created on demand

        m_data.append(item);
    }

    endResetModel();
}

int KIconDialogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_data.count();
}

QVariant KIconDialogModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, QAbstractItemModel::CheckIndexOption::IndexIsValid)) {
        return QVariant();
    }

    const auto &item = m_data.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        return item.name;
    case Qt::DecorationRole:
        if (item.pixmap.isNull()) {
            const_cast<KIconDialogModel *>(this)->loadPixmap(index);
        }
        return item.pixmap;
    case Qt::ToolTipRole:
        return item.name;
    case PathRole:
        return item.path;
    }

    return QVariant();
}

void KIconDialogModel::loadPixmap(const QModelIndex &index)
{
    Q_ASSERT(index.isValid());

    auto &item = m_data[index.row()];
    Q_ASSERT(item.pixmap.isNull());

    const auto dpr = devicePixelRatio();
    const auto canvasIconWidth = iconSize().width();
    const auto canvasIconHeight = iconSize().height();

    QImage img;

    if (item.path.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive) || item.path.endsWith(QLatin1String(".svgz"), Qt::CaseInsensitive)) {
        QSvgRenderer renderer(item.path);
        if (renderer.isValid()) {
            img = QImage(canvasIconWidth * dpr, canvasIconHeight * dpr, QImage::Format_ARGB32_Premultiplied);
            img.setDevicePixelRatio(dpr);
            img.fill(Qt::transparent);

            QPainter p(&img);
            // FIXME why do I need to specify bounds for it to not crop the SVG on dpr > 1?
            renderer.render(&p, QRect(0, 0, canvasIconWidth, canvasIconHeight));
        }
    } else {
        img.load(item.path);

        if (!img.isNull()) {
            if (img.width() > canvasIconWidth || img.height() > canvasIconHeight) {
                if (img.width() / (float)canvasIconWidth > img.height() / (float)canvasIconHeight) {
                    int height = (int)(((float)canvasIconWidth / img.width()) * img.height());
                    img = img.scaled(canvasIconWidth, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                } else {
                    int width = (int)(((float)canvasIconHeight / img.height()) * img.width());
                    img = img.scaled(width, canvasIconHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                }
            }

            if (/*uniformIconSize &&*/ (img.width() != canvasIconWidth || img.height() != canvasIconHeight)) {
                // Image is smaller than desired.  Draw onto a transparent QImage of the required dimensions.
                // (Unpleasant glitches occur if we break the uniformIconSizes() contract).
                QImage paddedImage =
                    QImage(canvasIconWidth * img.devicePixelRatioF(), canvasIconHeight * img.devicePixelRatioF(), QImage::Format_ARGB32_Premultiplied);
                paddedImage.setDevicePixelRatio(img.devicePixelRatioF());
                paddedImage.fill(0);
                QPainter painter(&paddedImage);
                painter.drawImage((canvasIconWidth - img.width()) / 2, (canvasIconHeight - img.height()) / 2, img);
                img = paddedImage;
            }
        }
    }

    if (!img.isNull()) {
        item.pixmap = QPixmap::fromImage(img);
    }
}

/**
 * Qt allocates very little horizontal space for the icon name,
 * even if the gridSize width is large.  This delegate allocates
 * the gridSize width (minus some padding) for the icon and icon name.
 */
class KIconCanvasDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    KIconCanvasDelegate(QListView *parent, QAbstractItemDelegate *defaultDelegate);
    ~KIconCanvasDelegate() override
    {
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QAbstractItemDelegate *m_defaultDelegate = nullptr;
};

KIconCanvasDelegate::KIconCanvasDelegate(QListView *parent, QAbstractItemDelegate *defaultDelegate)
    : QAbstractItemDelegate(parent)
{
    m_defaultDelegate = defaultDelegate;
}

void KIconCanvasDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto *canvas = static_cast<QListView *>(parent());
    const int gridWidth = canvas->gridSize().width();
    QStyleOptionViewItem newOption = option;
    newOption.displayAlignment = Qt::AlignHCenter | Qt::AlignTop;
    newOption.features.setFlag(QStyleOptionViewItem::WrapText);
    // Manipulate the width available.
    newOption.rect.setX((option.rect.x() / gridWidth) * gridWidth + s_edgePad);
    newOption.rect.setY(option.rect.y() + s_edgePad);
    newOption.rect.setWidth(gridWidth - 2 * s_edgePad);
    newOption.rect.setHeight(option.rect.height() - 2 * s_edgePad);
    m_defaultDelegate->paint(painter, newOption, index);
}

QSize KIconCanvasDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto *canvas = static_cast<QListView *>(parent());

    // TODO can we set wrap text and display alignment somewhere globally?
    QStyleOptionViewItem newOption = option;
    newOption.displayAlignment = Qt::AlignHCenter | Qt::AlignTop;
    newOption.features.setFlag(QStyleOptionViewItem::WrapText);

    QSize size = m_defaultDelegate->sizeHint(newOption, index);
    const int gridWidth = canvas->gridSize().width();
    const int gridHeight = canvas->gridSize().height();
    size.setWidth(gridWidth - 2 * s_edgePad);
    size.setHeight(gridHeight - 2 * s_edgePad);
    QFontMetrics metrics(option.font);
    size.setHeight(gridHeight + metrics.height() * 3);
    return size;
}

// TODO KF6 remove and override KIconDialog::showEvent()
class ShowEventFilter : public QObject
{
public:
    explicit ShowEventFilter(QObject *parent)
        : QObject(parent)
    {
    }
    ~ShowEventFilter() override
    {
    }

private:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Show) {
            KIconDialog *q = static_cast<KIconDialog *>(parent());
            q->d->showIcons();
            q->d->ui.searchLine->setFocus();
        }

        return QObject::eventFilter(watched, event);
    }
};

/*
 * KIconDialog: Dialog for selecting icons. Both system and user
 * specified icons can be chosen.
 */

KIconDialog::KIconDialog(QWidget *parent)
    : QDialog(parent)
    , d(new KIconDialogPrivate(this))
{
    setModal(true);

    d->mpLoader = KIconLoader::global();
    d->init();

    installEventFilter(new ShowEventFilter(this));
}

KIconDialog::KIconDialog(KIconLoader *loader, QWidget *parent)
    : QDialog(parent)
    , d(new KIconDialogPrivate(this))
{
    setModal(true);

    d->mpLoader = loader;
    d->init();

    installEventFilter(new ShowEventFilter(this));
}

void KIconDialogPrivate::init()
{
    mGroupOrSize = KIconLoader::Desktop;
    mContext = KIconLoader::Any;

    ui.setupUi(q);

    auto updatePlaceholder = [this] {
        updatePlaceholderLabel();
    };
    QObject::connect(proxyModel, &QSortFilterProxyModel::modelReset, q, updatePlaceholder);
    QObject::connect(proxyModel, &QSortFilterProxyModel::rowsInserted, q, updatePlaceholder);
    QObject::connect(proxyModel, &QSortFilterProxyModel::rowsRemoved, q, updatePlaceholder);

    // TODO set Ctrl+F shortcut
    QObject::connect(ui.searchLine, &QLineEdit::textChanged, proxyModel, &QSortFilterProxyModel::setFilterFixedString);

    static const char *const context_text[] = {
        I18N_NOOP("All"),
        I18N_NOOP("Actions"),
        I18N_NOOP("Applications"),
        I18N_NOOP("Categories"),
        I18N_NOOP("Devices"),
        I18N_NOOP("Emblems"),
        I18N_NOOP("Emotes"),
        I18N_NOOP("Mimetypes"),
        I18N_NOOP("Places"),
        I18N_NOOP("Status"),
    };
    static const KIconLoader::Context context_id[] = {
        KIconLoader::Any,
        KIconLoader::Action,
        KIconLoader::Application,
        KIconLoader::Category,
        KIconLoader::Device,
        KIconLoader::Emblem,
        KIconLoader::Emote,
        KIconLoader::MimeType,
        KIconLoader::Place,
        KIconLoader::StatusIcon,
    };
    const int cnt = sizeof(context_text) / sizeof(context_text[0]);
    for (int i = 0; i < cnt; ++i) {
        if (mpLoader->hasContext(context_id[i])) {
            ui.contextCombo->addItem(i18n(context_text[i]), context_id[i]);
            if (i == 0) {
                ui.contextCombo->insertSeparator(i + 1);
            }
        }
    }
    ui.contextCombo->insertSeparator(ui.contextCombo->count());
    ui.contextCombo->addItem(i18nc("Other icons", "Other"));
    ui.contextCombo->setMaxVisibleItems(ui.contextCombo->count());
    ui.contextCombo->setFixedSize(ui.contextCombo->sizeHint());

    QObject::connect(ui.contextCombo, qOverload<int>(&QComboBox::activated), q, [this]() {
        const auto currentData = ui.contextCombo->currentData();
        if (currentData.isValid()) {
            mContext = static_cast<KIconLoader::Context>(ui.contextCombo->currentData().toInt());
        } else {
            mContext = static_cast<KIconLoader::Context>(-1);
        }
        showIcons();
    });

    auto *delegate = new KIconCanvasDelegate(ui.canvas, ui.canvas->itemDelegate());
    ui.canvas->setItemDelegate(delegate);

    ui.canvas->setModel(proxyModel);

    QObject::connect(ui.canvas, &QAbstractItemView::activated, q, [this]() {
        custom.clear();
        q->slotOk();
    });

    // You can't just stack widgets on top of each other in Qt Designer
    auto *placeholderLayout = new QVBoxLayout(ui.canvas);

    placeholderLabel = new QLabel();
    QFont placeholderLabelFont;
    // To match the size of a level 2 Heading/KTitleWidget
    placeholderLabelFont.setPointSize(qRound(placeholderLabelFont.pointSize() * 1.3));
    placeholderLabel->setFont(placeholderLabelFont);
    placeholderLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    placeholderLabel->setWordWrap(true);
    placeholderLabel->setAlignment(Qt::AlignCenter);

    // Match opacity of QML placeholder label component
    auto *effect = new QGraphicsOpacityEffect(placeholderLabel);
    effect->setOpacity(0.5);
    placeholderLabel->setGraphicsEffect(effect);

    updatePlaceholderLabel();

    placeholderLayout->addWidget(placeholderLabel);
    placeholderLayout->setAlignment(placeholderLabel, Qt::AlignCenter);

    // TODO I bet there is a KStandardAction for that?
    browseButton = new QPushButton(QIcon::fromTheme(QStringLiteral("folder-open")), i18n("Browse…"));
    // TODO does this have implicatons? I just want the "Browse" button on the left side :)
    ui.buttonBox->addButton(browseButton, QDialogButtonBox::HelpRole);
    QObject::connect(browseButton, &QPushButton::clicked, q, [this] {
        browse();
    });

    QObject::connect(ui.buttonBox, &QDialogButtonBox::accepted, q, &KIconDialog::slotOk);
    QObject::connect(ui.buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);

    q->adjustSize();
}

KIconDialog::~KIconDialog() = default;

static bool sortByFileName(const QString &path1, const QString &path2)
{
    const QString fileName1 = path1.mid(path1.lastIndexOf(QLatin1Char('/')) + 1);
    const QString fileName2 = path2.mid(path2.lastIndexOf(QLatin1Char('/')) + 1);
    return QString::compare(fileName1, fileName2, Qt::CaseInsensitive) < 0;
}

void KIconDialogPrivate::showIcons()
{
    QStringList filelist;
    if (isSystemIconsContext()) {
        if (m_bStrictIconSize) {
            filelist = mpLoader->queryIcons(mGroupOrSize, mContext);
        } else {
            filelist = mpLoader->queryIconsByContext(mGroupOrSize, mContext);
        }
    } else if (!customLocation.isEmpty()) {
        filelist = mpLoader->queryIconsByDir(customLocation);
    } else {
        // List PNG files found directly in the kiconload search paths.
        const QStringList pngNameFilter(QStringLiteral("*.png"));
        for (const QString &relDir : KIconLoader::global()->searchPaths()) {
            const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, relDir, QStandardPaths::LocateDirectory);
            for (const QString &dir : dirs) {
                const auto files = QDir(dir).entryList(pngNameFilter);
                for (const QString &fileName : files) {
                    filelist << dir + QLatin1Char('/') + fileName;
                }
            }
        }
    }

    std::sort(filelist.begin(), filelist.end(), sortByFileName);

    // The KIconCanvas has uniformItemSizes set which really expects
    // all added icons to be the same size, otherwise weirdness ensues :)
    // Ensure all SVGs are scaled to the desired size and that as few icons
    // need to be padded as possible by specifying a sensible size.
    if (mGroupOrSize < -1) {
        // mGroupOrSize can be -1 if NoGroup is chosen.
        // Explicit size.
        ui.canvas->setIconSize(QSize(-mGroupOrSize, -mGroupOrSize));
    } else {
        // Icon group.
        int groupSize = mpLoader->currentSize(static_cast<KIconLoader::Group>(mGroupOrSize));
        ui.canvas->setIconSize(QSize(groupSize, groupSize));
    }

    // Try to make room for three lines of text...
    QFontMetrics metrics(ui.canvas->font());
    const int frameHMargin = ui.canvas->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, ui.canvas) + 1;
    const int lineCount = 3;
    ui.canvas->setGridSize(QSize(100, ui.canvas->iconSize().height() + lineCount * metrics.height() + 3 * frameHMargin));

    // Set a minimum size of 6x3 icons
    const int columnCount = 6;
    const int rowCount = 3;
    QStyleOption opt;
    opt.initFrom(ui.canvas);
    int width = columnCount * ui.canvas->gridSize().width();
    width += ui.canvas->verticalScrollBar()->sizeHint().width() + 1;
    width += 2 * ui.canvas->frameWidth();
    if (ui.canvas->style()->styleHint(QStyle::SH_ScrollView_FrameOnlyAroundContents, &opt, ui.canvas)) {
        width += ui.canvas->style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarSpacing, &opt, ui.canvas);
    }
    int height = rowCount * ui.canvas->gridSize().height() + 1;
    height += 2 * ui.canvas->frameWidth();

    ui.canvas->setMinimumSize(QSize(width, height));

    model->setIconSize(ui.canvas->iconSize());
    model->setDevicePixelRatio(q->devicePixelRatioF());
    model->load(filelist);
}

void KIconDialog::setStrictIconSize(bool b)
{
    d->m_bStrictIconSize = b;
}

bool KIconDialog::strictIconSize() const
{
    return d->m_bStrictIconSize;
}

void KIconDialog::setIconSize(int size)
{
    // see KIconLoader, if you think this is weird
    if (size == 0) {
        d->mGroupOrSize = KIconLoader::Desktop; // default Group
    } else {
        d->mGroupOrSize = -size; // yes, KIconLoader::queryIconsByContext is weird
    }
}

int KIconDialog::iconSize() const
{
    // 0 or any other value ==> mGroupOrSize is a group, so we return 0
    return (d->mGroupOrSize < 0) ? -d->mGroupOrSize : 0;
}

void KIconDialog::setup(KIconLoader::Group group, KIconLoader::Context context, bool strictIconSize, int iconSize, bool user, bool lockUser, bool lockCustomDir)
{
    d->m_bStrictIconSize = strictIconSize;
    d->m_bLockUser = lockUser;
    d->m_bLockCustomDir = lockCustomDir;
    if (iconSize == 0) {
        if (group == KIconLoader::NoGroup) {
            // NoGroup has numeric value -1, which should
            // not really be used with KIconLoader::queryIcons*(...);
            // pick a proper group.
            d->mGroupOrSize = KIconLoader::Small;
        } else {
            d->mGroupOrSize = group;
        }
    } else {
        d->mGroupOrSize = -iconSize;
    }

    if (user) {
        d->ui.contextCombo->setCurrentIndex(d->ui.contextCombo->count() - 1);
    } else {
        d->setContext(context);
    }

    d->ui.contextCombo->setEnabled(!user || !lockUser);

    // Disable "Other" entry when user is locked
    auto *model = qobject_cast<QStandardItemModel *>(d->ui.contextCombo->model());
    auto *otherItem = model->item(model->rowCount() - 1);
    auto flags = otherItem->flags();
    flags.setFlag(Qt::ItemIsEnabled, !lockUser);
    otherItem->setFlags(flags);

    // Only allow browsing when explicitly allowed and user icons are allowed
    // An app may not expect a path when asking only about system icons
    d->browseButton->setVisible(!lockCustomDir && (!user || !lockUser));
}

void KIconDialogPrivate::setContext(KIconLoader::Context context)
{
    mContext = context;
    const int index = ui.contextCombo->findData(context);
    if (index > -1) {
        ui.contextCombo->setCurrentIndex(index);
    }
}

void KIconDialogPrivate::updatePlaceholderLabel()
{
    if (proxyModel->rowCount() > 0) {
        placeholderLabel->hide();
        return;
    }

    if (!ui.searchLine->text().isEmpty()) {
        placeholderLabel->setText(i18n("No icons matching the search"));
    } else {
        placeholderLabel->setText(i18n("No icons in this category"));
    }

    placeholderLabel->show();
}

void KIconDialog::setCustomLocation(const QString &location)
{
    d->customLocation = location;
}

QString KIconDialog::openDialog()
{
    if (exec() == Accepted) {
        if (!d->custom.isEmpty()) {
            return d->custom;
        }

        const QString name = d->ui.canvas->currentIndex().data(KIconDialogModel::PathRole).toString();
        if (name.isEmpty() || !d->ui.contextCombo->currentData().isValid()) {
            return name;
        }

        QFileInfo fi(name);
        return fi.completeBaseName();
    }

    return QString();
}

void KIconDialog::showDialog()
{
    setModal(false);
    show();
}

void KIconDialog::slotOk()
{
    QString name;
    if (!d->custom.isEmpty()) {
        name = d->custom;
    } else {
        name = d->ui.canvas->currentIndex().data(KIconDialogModel::PathRole).toString();
        if (!name.isEmpty() && d->isSystemIconsContext()) {
            const QFileInfo fi(name);
            name = fi.completeBaseName();
        }
    }

    Q_EMIT newIconName(name);
    QDialog::accept();
}

QString KIconDialog::getIcon(KIconLoader::Group group,
                             KIconLoader::Context context,
                             bool strictIconSize,
                             int iconSize,
                             bool user,
                             QWidget *parent,
                             const QString &caption)
{
    KIconDialog dlg(parent);
    dlg.setup(group, context, strictIconSize, iconSize, user);
    if (!caption.isEmpty()) {
        dlg.setWindowTitle(caption);
    }

    return dlg.openDialog();
}

void KIconDialogPrivate::browse()
{
    if (browseDialog) {
        browseDialog.data()->show();
        browseDialog.data()->raise();
        return;
    }

    // Create a file dialog to select an ICO, PNG, XPM or SVG file,
    // with the image previewer shown.
    QFileDialog *dlg = new QFileDialog(q, i18n("Select Icon"), QString(), i18n("*.ico *.png *.xpm *.svg *.svgz|Icon Files (*.ico *.png *.xpm *.svg *.svgz)"));
    // TODO This was deliberately modal before, why? Or just because "window modal" wasn't a thing?
    dlg->setWindowModality(Qt::WindowModal);
    dlg->setFileMode(QFileDialog::ExistingFile);
    QObject::connect(dlg, &QFileDialog::fileSelected, q, [this](const QString &path) {
        if (!path.isEmpty()) {
            custom = path;
            if (isSystemIconsContext()) {
                customLocation = QFileInfo(custom).absolutePath();
            }
            q->slotOk();
        }
    });
    browseDialog = dlg;
    dlg->show();
}

bool KIconDialogPrivate::isSystemIconsContext() const
{
    return ui.contextCombo->currentData().isValid();
}

#include "kicondialog.moc"
#include "moc_kicondialog.cpp"
#include "moc_kicondialogmodel_p.cpp"
