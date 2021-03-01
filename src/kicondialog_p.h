/* vi: ts=8 sts=4 sw=4

    This file is part of the KDE project, module kfile.
    SPDX-FileCopyrightText: 2000 Geert Jansen <jansen@kde.org>
    SPDX-FileCopyrightText: 2000 Kurt Granroth <granroth@kde.org>
    SPDX-FileCopyrightText: 1997 Christoph Neerfeld <chris@kde.org>
    SPDX-FileCopyrightText: 2002 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KICONDIALOG_P_H
#define KICONDIALOG_P_H

#include <QFileDialog>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QStringList>

class QComboBox;
class QProgressBar;
class QRadioButton;

class KIconCanvasDelegate;
class KListWidgetSearchLine;

/**
 * Icon canvas for KIconDialog.
 */
class KIconCanvas : public QListWidget
{
    Q_OBJECT

public:
    /**
     * Creates a new icon canvas.
     *
     * @param parent The parent widget.
     */
    explicit KIconCanvas(QWidget *parent = nullptr);

    /**
     * Destroys the icon canvas.
     */
    ~KIconCanvas();

    /**
     * Load icons into the canvas.
     */
    void loadFiles(const QStringList &files);

    /**
     * Returns the current icon.
     */
    QString getCurrent() const;

public Q_SLOTS:
    /**
     * Call this slot to stop the loading of the icons.
     */
    void stopLoading();

Q_SIGNALS:
    /**
     * Emitted when the current icon has changed.
     */
    void nameChanged(const QString &);

    /**
     * This signal is emitted when the loading of the icons
     * has started.
     *
     * @param count The number of icons to be loaded.
     */
    void startLoading(int count);

    /**
     * This signal is emitted whenever an icon has been loaded.
     *
     * @param number The number of the currently loaded item.
     */
    void progress(int number);

    /**
     * This signal is emitted when the loading of the icons
     * has been finished.
     */
    void finished();

private Q_SLOTS:
    void loadFiles();
    void currentListItemChanged(QListWidgetItem *item);

private:
    bool m_loading;
    QStringList m_files;
    QTimer *m_timer;
    KIconCanvasDelegate *m_delegate;
};

class KIconDialogPrivate
{
public:
    KIconDialogPrivate(KIconDialog *qq)
    {
        q = qq;
        m_bStrictIconSize = true;
        m_bLockUser = false;
        m_bLockCustomDir = false;
        searchLine = nullptr;
        mNumOfSteps = 1;
    }
    ~KIconDialogPrivate()
    {
    }

    void init();
    void showIcons();
    void setContext(KIconLoader::Context context);

    // slots
    void _k_slotContext(int);
    void _k_slotStartLoading(int);
    void _k_slotProgress(int);
    void _k_slotFinished();
    void _k_slotAcceptIcons();
    void _k_slotBrowse();
    void _k_customFileSelected(const QString &path);
    void _k_slotOtherIconClicked();
    void _k_slotSystemIconClicked();

    KIconDialog *q;

    int mGroupOrSize;
    KIconLoader::Context mContext;

    QComboBox *mpCombo;
    QPushButton *mpBrowseBut;
    QRadioButton *mpSystemIcons, *mpOtherIcons;
    QProgressBar *mpProgress;
    int mNumOfSteps;
    KIconLoader *mpLoader;
    KIconCanvas *mpCanvas;
    int mNumContext;
    KIconLoader::Context mContextMap[10]; // must match KDE::icon::Context size, code has assert

    bool m_bStrictIconSize, m_bLockUser, m_bLockCustomDir;
    QString custom;
    QString customLocation;
    KListWidgetSearchLine *searchLine;
    QPointer<QFileDialog> browseDialog;
};

#endif // KICONDIALOG_P_H
