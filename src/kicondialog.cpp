/* vi: ts=8 sts=4 sw=4

    This file is part of the KDE project, module kfile.
    SPDX-FileCopyrightText: 2000 Geert Jansen <jansen@kde.org>
    SPDX-FileCopyrightText: 2000 Kurt Granroth <granroth@kde.org>
    SPDX-FileCopyrightText: 1997 Christoph Neerfeld <chris@kde.org>
    SPDX-FileCopyrightText: 2002 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kicondialog.h"
#include "kicondialog_p.h"

#include <KListWidgetSearchLine>
#include <KLocalizedString>
#ifndef _WIN32_WCE
#include <QSvgRenderer>
#endif

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QProgressBar>
#include <QRadioButton>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTimer>

/**
 * Qt allocates very little horizontal space for the icon name,
 * even if the gridSize width is large.  This delegate allocates
 * the gridSize width (minus some padding) for the icon and icon name.
 */
class KIconCanvasDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    KIconCanvasDelegate(KIconCanvas *parent, QAbstractItemDelegate *defaultDelegate);
    ~KIconCanvasDelegate() override
    {
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    KIconCanvas *m_iconCanvas = nullptr;
    QAbstractItemDelegate *m_defaultDelegate = nullptr;
    static const int HORIZONTAL_EDGE_PAD = 3;
};

KIconCanvasDelegate::KIconCanvasDelegate(KIconCanvas *parent, QAbstractItemDelegate *defaultDelegate)
    : QAbstractItemDelegate(parent)
{
    m_iconCanvas = parent;
    m_defaultDelegate = defaultDelegate;
}

void KIconCanvasDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const int GRID_WIDTH = m_iconCanvas->gridSize().width();
    QStyleOptionViewItem newOption = option;
    // Manipulate the width available.
    newOption.rect.setX((option.rect.x() / GRID_WIDTH) * GRID_WIDTH + HORIZONTAL_EDGE_PAD);
    newOption.rect.setWidth(GRID_WIDTH - 2 * HORIZONTAL_EDGE_PAD);

    m_defaultDelegate->paint(painter, newOption, index);
}

QSize KIconCanvasDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize size = m_defaultDelegate->sizeHint(option, index);
    const int GRID_WIDTH = m_iconCanvas->gridSize().width();
    size.setWidth(GRID_WIDTH - 2 * HORIZONTAL_EDGE_PAD);
    return size;
}

/*
 * KIconCanvas: Iconview for the iconloader dialog.
 */

KIconCanvas::KIconCanvas(QWidget *parent)
    : QListWidget(parent)
    , m_loading(false)
    , m_timer(new QTimer(this))
    , m_delegate(new KIconCanvasDelegate(this, itemDelegate()))
{
    setViewMode(IconMode);
    setUniformItemSizes(true);
    setMovement(Static);
    setIconSize(QSize(60, 60));
    connect(m_timer, &QTimer::timeout, this, QOverload<>::of(&KIconCanvas::loadFiles));
    connect(this, &QListWidget::currentItemChanged, this, &KIconCanvas::currentListItemChanged);
    setGridSize(QSize(100, 80));

    setItemDelegate(m_delegate);
}

KIconCanvas::~KIconCanvas()
{
    delete m_timer;
    delete m_delegate;
}

void KIconCanvas::loadFiles(const QStringList &files)
{
    clear();
    m_files = files;
    Q_EMIT startLoading(m_files.count());
    m_timer->setSingleShot(true);
    m_timer->start(10);
    m_loading = false;
}

void KIconCanvas::loadFiles()
{
    setResizeMode(QListWidget::Fixed);
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // disable updates to not trigger paint events when adding child items,
    // but force an initial paint so that we do not get garbage
    repaint();
    setUpdatesEnabled(false);

    // Cache these as we will call them frequently.
    const int canvasIconWidth = iconSize().width();
    const int canvasIconHeight = iconSize().width();
    const bool uniformIconSize = uniformItemSizes();
    const qreal dpr = devicePixelRatioF();

    m_loading = true;
    int i;
    QStringList::ConstIterator it;
    uint emitProgress = 10; // so we will emit it once in the beginning
    QStringList::ConstIterator end(m_files.constEnd());
    for (it = m_files.constBegin(), i = 0; it != end; ++it, ++i) {
        if (emitProgress >= 10) {
            Q_EMIT progress(i);
            emitProgress = 0;
        }

        emitProgress++;

        if (!m_loading) { // user clicked on a button that will load another set of icons
            break;
        }
        QImage img;

        // Use the extension as the format. Works for XPM and PNG, but not for SVG
        QString path = *it;
        QString ext = path.right(3).toUpper();

        if (ext != QLatin1String("SVG") && ext != QLatin1String("VGZ")) {
            img.load(*it);

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

                if (uniformIconSize && (img.width() != canvasIconWidth || img.height() != canvasIconHeight)) {
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
        } else {
            // Special stuff for SVG icons
            img = QImage(canvasIconWidth * dpr, canvasIconHeight * dpr, QImage::Format_ARGB32_Premultiplied);
            img.setDevicePixelRatio(dpr);
            img.fill(0);
            QSvgRenderer renderer(*it);
            if (renderer.isValid()) {
                QPainter p(&img);
                // FIXME why do I need to specify bounds for it to not crop the SVG on dpr > 1?
                renderer.render(&p, QRect(0, 0, canvasIconWidth, canvasIconHeight));
            }
        }

        QPixmap pm = QPixmap::fromImage(img);
        QFileInfo fi(*it);
        QListWidgetItem *item = new QListWidgetItem(pm, fi.completeBaseName(), this);
        item->setData(Qt::UserRole, *it);
        item->setToolTip(fi.completeBaseName());
    }

    // enable updates since we have to draw the whole view now
    setUpdatesEnabled(true);

    QApplication::restoreOverrideCursor();
    m_loading = false;
    Q_EMIT finished();
    setResizeMode(QListWidget::Adjust);
}

QString KIconCanvas::getCurrent() const
{
    if (!currentItem()) {
        return QString();
    }

    return currentItem()->data(Qt::UserRole).toString();
}

void KIconCanvas::stopLoading()
{
    m_loading = false;
}

void KIconCanvas::currentListItemChanged(QListWidgetItem *item)
{
    Q_EMIT nameChanged((item != nullptr) ? item->text() : QString());
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
            q->d->searchLine->setFocus();
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
    setWindowTitle(i18n("Select Icon"));

    d->mpLoader = KIconLoader::global();
    d->init();

    installEventFilter(new ShowEventFilter(this));
}

KIconDialog::KIconDialog(KIconLoader *loader, QWidget *parent)
    : QDialog(parent)
    , d(new KIconDialogPrivate(this))
{
    setModal(true);
    setWindowTitle(i18n("Select Icon"));

    d->mpLoader = loader;
    d->init();

    installEventFilter(new ShowEventFilter(this));
}

void KIconDialogPrivate::init()
{
    mGroupOrSize = KIconLoader::Desktop;
    mContext = KIconLoader::Any;

    QVBoxLayout *top = new QVBoxLayout(q);

    QGroupBox *bgroup = new QGroupBox(q);
    bgroup->setTitle(i18n("Icon Source"));

    QVBoxLayout *vbox = new QVBoxLayout(bgroup);

    top->addWidget(bgroup);

    QGridLayout *grid = new QGridLayout();
    vbox->addLayout(grid);

    mpSystemIcons = new QRadioButton(i18n("S&ystem icons:"), bgroup);
    QObject::connect(mpSystemIcons, &QAbstractButton::clicked, q, [this]() {
        _k_slotSystemIconClicked();
    });
    grid->addWidget(mpSystemIcons, 1, 0);
    mpCombo = new QComboBox(bgroup);
    QObject::connect(mpCombo, QOverload<int>::of(&QComboBox::activated), q, [this](int index) {
        _k_slotContext(index);
    });
    grid->addWidget(mpCombo, 1, 1);
    mpOtherIcons = new QRadioButton(i18n("O&ther icons:"), bgroup);
    QObject::connect(mpOtherIcons, &QAbstractButton::clicked, q, [this]() {
        _k_slotOtherIconClicked();
    });
    grid->addWidget(mpOtherIcons, 2, 0);
    mpBrowseBut = new QPushButton(i18n("&Browse..."), bgroup);
    QObject::connect(mpBrowseBut, &QPushButton::clicked, q, [this]() {
        _k_slotBrowse();
    });
    grid->addWidget(mpBrowseBut, 2, 1);

    //
    // ADD SEARCHLINE
    //
    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchLayout->setContentsMargins(0, 0, 0, 0);
    top->addLayout(searchLayout);

    QLabel *searchLabel = new QLabel(i18n("&Search:"), q);
    searchLayout->addWidget(searchLabel);

    searchLine = new KListWidgetSearchLine(q);
    searchLayout->addWidget(searchLine);
    searchLabel->setBuddy(searchLine);

    QString wtstr = i18n("Search interactively for icon names (e.g. folder).");
    searchLabel->setWhatsThis(wtstr);
    searchLine->setWhatsThis(wtstr);

    mpCanvas = new KIconCanvas(q);
    QObject::connect(mpCanvas, &QListWidget::itemActivated, q, [this]() {
        _k_slotAcceptIcons();
    });
    top->addWidget(mpCanvas);
    searchLine->setListWidget(mpCanvas);

    // Compute width of canvas with 4 icons displayed in a row
    QStyleOption opt;
    opt.initFrom(mpCanvas);
    int width = 4 * mpCanvas->gridSize().width() + 1;
    width += mpCanvas->verticalScrollBar()->sizeHint().width();
    width += 2 * mpCanvas->frameWidth();
    if (mpCanvas->style()->styleHint(QStyle::SH_ScrollView_FrameOnlyAroundContents, &opt, mpCanvas)) {
        width += mpCanvas->style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarSpacing, &opt, mpCanvas);
    }
    mpCanvas->setMinimumSize(width, 125);

    mpProgress = new QProgressBar(q);
    top->addWidget(mpProgress);
    QObject::connect(mpCanvas, &KIconCanvas::startLoading, q, [this](int count) {
        _k_slotStartLoading(count);
    });
    QObject::connect(mpCanvas, &KIconCanvas::progress, q, [this](int step) {
        _k_slotProgress(step);
    });
    QObject::connect(mpCanvas, &KIconCanvas::finished, q, [this]() {
        _k_slotFinished();
    });

    // When pressing Ok or Cancel, stop loading icons
    QObject::connect(q, &QDialog::finished, mpCanvas, &KIconCanvas::stopLoading);

    static const char *const context_text[] = {I18N_NOOP("Actions"),
                                               I18N_NOOP("Applications"),
                                               I18N_NOOP("Categories"),
                                               I18N_NOOP("Devices"),
                                               I18N_NOOP("Emblems"),
                                               I18N_NOOP("Emotes"),
                                               I18N_NOOP("Mimetypes"),
                                               I18N_NOOP("Places"),
                                               I18N_NOOP("Status"),
                                               I18N_NOOP("All")};
    static const KIconLoader::Context context_id[] = {KIconLoader::Action,
                                                      KIconLoader::Application,
                                                      KIconLoader::Category,
                                                      KIconLoader::Device,
                                                      KIconLoader::Emblem,
                                                      KIconLoader::Emote,
                                                      KIconLoader::MimeType,
                                                      KIconLoader::Place,
                                                      KIconLoader::StatusIcon,
                                                      KIconLoader::Any};
    mNumContext = 0;
    int cnt = sizeof(context_text) / sizeof(context_text[0]);
    // check all 3 arrays have same sizes
    Q_ASSERT(cnt == sizeof(context_id) / sizeof(context_id[0]) && cnt == sizeof(mContextMap) / sizeof(mContextMap[0]));
    for (int i = 0; i < cnt; ++i) {
        if (mpLoader->hasContext(context_id[i])) {
            mpCombo->addItem(i18n(context_text[i]));
            mContextMap[mNumContext++] = context_id[i];
        }
    }
    mpCombo->setMaxVisibleItems(cnt);
    mpCombo->setFixedSize(mpCombo->sizeHint());

    mpBrowseBut->setFixedWidth(mpCombo->width());

    QDialogButtonBox *buttonBox = new QDialogButtonBox(q);
    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, q, &KIconDialog::slotOk);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, q, &QDialog::reject);
    top->addWidget(buttonBox);

    // Make the dialog a little taller
    q->resize(q->sizeHint() + QSize(0, 100));
    q->adjustSize();
}

KIconDialog::~KIconDialog() = default;

void KIconDialogPrivate::_k_slotAcceptIcons()
{
    custom.clear();
    q->slotOk();
}

static bool sortByFileName(const QString &path1, const QString &path2)
{
    const QString fileName1 = path1.mid(path1.lastIndexOf(QLatin1Char('/')) + 1);
    const QString fileName2 = path2.mid(path2.lastIndexOf(QLatin1Char('/')) + 1);
    return QString::compare(fileName1, fileName2, Qt::CaseInsensitive) < 0;
}

void KIconDialogPrivate::showIcons()
{
    mpCanvas->clear();
    QStringList filelist;
    if (mpSystemIcons->isChecked()) {
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
        mpCanvas->setIconSize(QSize(-mGroupOrSize, -mGroupOrSize));
    } else {
        // Icon group.
        int groupSize = mpLoader->currentSize(static_cast<KIconLoader::Group>(mGroupOrSize));
        mpCanvas->setIconSize(QSize(groupSize, groupSize));
    }

    mpCanvas->loadFiles(filelist);
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

    d->mpSystemIcons->setChecked(!user);
    d->mpSystemIcons->setEnabled(!lockUser || !user);
    d->mpOtherIcons->setChecked(user);
    d->mpOtherIcons->setEnabled(!lockUser || user);
    d->mpCombo->setEnabled(!user);
    d->mpBrowseBut->setEnabled(user && !lockCustomDir);
    d->setContext(context);
}

void KIconDialogPrivate::setContext(KIconLoader::Context context)
{
    mContext = context;
    for (int i = 0; i < mNumContext; ++i) {
        if (mContextMap[i] == context) {
            mpCombo->setCurrentIndex(i);
            return;
        }
    }
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

        const QString name = d->mpCanvas->getCurrent();
        if (name.isEmpty() || d->mpOtherIcons->isChecked()) {
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
        name = d->mpCanvas->getCurrent();
        if (!name.isEmpty() && d->mpSystemIcons->isChecked()) {
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

void KIconDialogPrivate::_k_slotBrowse()
{
    if (browseDialog) {
        browseDialog.data()->show();
        browseDialog.data()->raise();
        return;
    }

    // Create a file dialog to select an ICO, PNG, XPM or SVG file,
    // with the image previewer shown.
    QFileDialog *dlg = new QFileDialog(q, i18n("Select Icon"), QString(), i18n("*.ico *.png *.xpm *.svg *.svgz|Icon Files (*.ico *.png *.xpm *.svg *.svgz)"));
    dlg->setModal(false);
    dlg->setFileMode(QFileDialog::ExistingFile);
    QObject::connect(dlg, &QFileDialog::fileSelected, q, [this](const QString &file) {
        _k_customFileSelected(file);
    });
    browseDialog = dlg;
    dlg->show();
}

void KIconDialogPrivate::_k_customFileSelected(const QString &path)
{
    if (!path.isEmpty()) {
        custom = path;
        if (mpSystemIcons->isChecked()) {
            customLocation = QFileInfo(custom).absolutePath();
        }
        q->slotOk();
    }
}

void KIconDialogPrivate::_k_slotSystemIconClicked()
{
    mpBrowseBut->setEnabled(false);
    mpCombo->setEnabled(true);
    showIcons();
}

void KIconDialogPrivate::_k_slotOtherIconClicked()
{
    mpBrowseBut->setEnabled(!m_bLockCustomDir);
    mpCombo->setEnabled(false);
    showIcons();
}

void KIconDialogPrivate::_k_slotContext(int id)
{
    mContext = static_cast<KIconLoader::Context>(mContextMap[id]);
    showIcons();
}

void KIconDialogPrivate::_k_slotStartLoading(int steps)
{
    if (steps < 10) {
        mpProgress->hide();
    } else {
        mNumOfSteps = steps;
        mpProgress->setValue(0);
        mpProgress->show();
    }
}

void KIconDialogPrivate::_k_slotProgress(int p)
{
    mpProgress->setValue(static_cast<int>(100.0 * (double)p / (double)mNumOfSteps));
}

void KIconDialogPrivate::_k_slotFinished()
{
    mNumOfSteps = 1;
    mpProgress->hide();
}

#include "kicondialog.moc"
#include "moc_kicondialog.cpp"
#include "moc_kicondialog_p.cpp"
