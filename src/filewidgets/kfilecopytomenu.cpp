/* Copyright 2008, 2009, 2015 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2 of the License or
   ( at your option ) version 3 or, at the discretion of KDE e.V.
   ( which shall act as a proxy as in section 14 of the GPLv3 ), any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kfilecopytomenu.h"
#include "kfilecopytomenu_p.h"
#include <QAction>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <QFileDialog>
#include <QMimeDatabase>
#include <QMimeType>

#include <KIO/FileUndoManager>
#include <KIO/CopyJob>
#include <KIO/JobUiDelegate>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KStringHandler>
#include <KJobWidgets>

#ifdef Q_OS_WIN
#include "windows.h"
#endif

KFileCopyToMenuPrivate::KFileCopyToMenuPrivate(KFileCopyToMenu *qq, QWidget *parentWidget)
    : q(qq),
      m_urls(),
      m_parentWidget(parentWidget),
      m_readOnly(false),
      m_autoErrorHandling(false)
{
}

////

KFileCopyToMenu::KFileCopyToMenu(QWidget *parentWidget)
    : QObject(parentWidget), d(new KFileCopyToMenuPrivate(this, parentWidget))
{
}

KFileCopyToMenu::~KFileCopyToMenu()
{
    delete d;
}

void KFileCopyToMenu::setUrls(const QList<QUrl> &urls)
{
    d->m_urls = urls;
}

void KFileCopyToMenu::setReadOnly(bool ro)
{
    d->m_readOnly = ro;
}

void KFileCopyToMenu::setAutoErrorHandlingEnabled(bool b)
{
    d->m_autoErrorHandling = b;
}

void KFileCopyToMenu::addActionsTo(QMenu *menu) const
{
    QMenu *mainCopyMenu = new KFileCopyToMainMenu(menu, d, Copy);
    mainCopyMenu->setTitle(i18nc("@title:menu", "Copy To"));
    mainCopyMenu->menuAction()->setObjectName(QStringLiteral("copyTo_submenu"));   // for the unittest
    menu->addMenu(mainCopyMenu);

    if (!d->m_readOnly) {
        QMenu *mainMoveMenu = new KFileCopyToMainMenu(menu, d, Move);
        mainMoveMenu->setTitle(i18nc("@title:menu", "Move To"));
        mainMoveMenu->menuAction()->setObjectName(QStringLiteral("moveTo_submenu"));   // for the unittest
        menu->addMenu(mainMoveMenu);
    }
}

////

KFileCopyToMainMenu::KFileCopyToMainMenu(QMenu *parent, KFileCopyToMenuPrivate *_d, MenuType menuType)
    : QMenu(parent), m_menuType(menuType),
      m_actionGroup(static_cast<QWidget *>(nullptr)),
      d(_d),
      m_recentDirsGroup(KSharedConfig::openConfig(), m_menuType == Copy ? "kuick-copy" : "kuick-move")
{
    connect(this, &KFileCopyToMainMenu::aboutToShow, this, &KFileCopyToMainMenu::slotAboutToShow);
    connect(&m_actionGroup, &QActionGroup::triggered, this, &KFileCopyToMainMenu::slotTriggered);
}

void KFileCopyToMainMenu::slotAboutToShow()
{
    clear();
    KFileCopyToDirectoryMenu *subMenu;
    // Home Folder
    subMenu = new KFileCopyToDirectoryMenu(this, this, QDir::homePath());
    subMenu->setTitle(i18nc("@title:menu", "Home Folder"));
    subMenu->setIcon(QIcon::fromTheme(QStringLiteral("go-home")));
    QAction *act = addMenu(subMenu);
    act->setObjectName(QStringLiteral("home"));

    // Root Folder
#ifndef Q_OS_WIN
    subMenu = new KFileCopyToDirectoryMenu(this, this, QDir::rootPath());
    subMenu->setTitle(i18nc("@title:menu", "Root Folder"));
    subMenu->setIcon(QIcon::fromTheme(QStringLiteral("folder-red")));
    act = addMenu(subMenu);
    act->setObjectName(QStringLiteral("root"));
#else
    foreach (const QFileInfo &info, QDir::drives()) {
        QString driveIcon = QStringLiteral("drive-harddisk");
        const uint type = GetDriveTypeW((wchar_t *)info.absoluteFilePath().utf16());
        switch (type) {
        case DRIVE_REMOVABLE:
            driveIcon = QStringLiteral("drive-removable-media");
            break;
        case DRIVE_FIXED:
            driveIcon = QStringLiteral("drive-harddisk");
            break;
        case DRIVE_REMOTE:
            driveIcon = QStringLiteral("network-server");
            break;
        case DRIVE_CDROM:
            driveIcon = QStringLiteral("drive-optical");
            break;
        case DRIVE_RAMDISK:
        case DRIVE_UNKNOWN:
        case DRIVE_NO_ROOT_DIR:
        default:
            driveIcon = QStringLiteral("drive-harddisk");
        }
        subMenu = new KFileCopyToDirectoryMenu(this, this, info.absoluteFilePath());
        subMenu->setTitle(info.absoluteFilePath());
        subMenu->setIcon(QIcon::fromTheme(driveIcon));
        addMenu(subMenu);
    }
#endif

    // Browse... action, shows a file dialog
    QAction *browseAction = new QAction(i18nc("@title:menu in Copy To or Move To submenu", "Browse..."), this);
    browseAction->setObjectName(QStringLiteral("browse"));
    connect(browseAction, &QAction::triggered, this, &KFileCopyToMainMenu::slotBrowse);
    addAction(browseAction);

    addSeparator(); // Qt handles removing it automatically if it's last in the menu, nice.

    // Recent Destinations
    const QStringList recentDirs = m_recentDirsGroup.readPathEntry("Paths", QStringList());
    for (const QString &recentDir : recentDirs) {
        const QUrl url = QUrl::fromLocalFile(recentDir);
        const QString text = KStringHandler::csqueeze(url.toDisplayString(QUrl::PreferLocalFile), 60); // shorten very long paths (#61386)
        QAction *act = new QAction(text, this);
        act->setObjectName(recentDir);
        act->setData(url);
        m_actionGroup.addAction(act);
        addAction(act);
    }
}

void KFileCopyToMainMenu::slotBrowse()
{
    const QUrl dest = QFileDialog::getExistingDirectoryUrl(d->m_parentWidget ? d->m_parentWidget : this);
    if (!dest.isEmpty()) {
        copyOrMoveTo(dest);
    }
}

void KFileCopyToMainMenu::slotTriggered(QAction *action)
{
    const QUrl url = action->data().toUrl();
    Q_ASSERT(!url.isEmpty());
    copyOrMoveTo(url);
}

void KFileCopyToMainMenu::copyOrMoveTo(const QUrl &dest)
{
    // Insert into the recent destinations list
    QStringList recentDirs = m_recentDirsGroup.readPathEntry("Paths", QStringList());
    const QString niceDest = dest.toDisplayString(QUrl::PreferLocalFile);
    if (!recentDirs.contains(niceDest)) { // don't change position if already there, moving stuff is bad usability
        recentDirs.prepend(niceDest);
        while (recentDirs.size() > 10) { // hardcoded max size
            recentDirs.removeLast();
        }
        m_recentDirsGroup.writePathEntry("Paths", recentDirs);
    }

    // #199549: add a trailing slash to avoid unexpected results when the
    // dest doesn't exist anymore: it was creating a file with the name of
    // the now non-existing dest.
    QUrl dirDest = dest;
    if (!dirDest.path().endsWith(QLatin1Char('/'))) {
        dirDest.setPath(dirDest.path() + QLatin1Char('/'));
    }

    // And now let's do the copy or move -- with undo/redo support.
    KIO::CopyJob *job = m_menuType == Copy ? KIO::copy(d->m_urls, dirDest) : KIO::move(d->m_urls, dirDest);
    KIO::FileUndoManager::self()->recordCopyJob(job);
    KJobWidgets::setWindow(job, d->m_parentWidget ? d->m_parentWidget : this);
    job->uiDelegate()->setAutoErrorHandlingEnabled(d->m_autoErrorHandling);
    connect(job, &KIO::CopyJob::result, this, [this](KJob * job) {
        emit d->q->error(job->error(), job->errorString());
    });
}

////

KFileCopyToDirectoryMenu::KFileCopyToDirectoryMenu(QMenu *parent, KFileCopyToMainMenu *mainMenu, const QString &path)
    : QMenu(parent), m_mainMenu(mainMenu), m_path(path)
{
    if (!m_path.endsWith(QLatin1Char('/'))) {
        m_path.append(QLatin1Char('/'));
    }
    connect(this, &KFileCopyToDirectoryMenu::aboutToShow, this, &KFileCopyToDirectoryMenu::slotAboutToShow);
}

void KFileCopyToDirectoryMenu::slotAboutToShow()
{
    clear();
    QAction *act = new QAction(m_mainMenu->menuType() == Copy
                               ? i18nc("@title:menu", "Copy Here")
                               : i18nc("@title:menu", "Move Here"), this);
    act->setData(QUrl::fromLocalFile(m_path));
    act->setEnabled(QFileInfo(m_path).isWritable());
    m_mainMenu->actionGroup().addAction(act);
    addAction(act);

    addSeparator(); // Qt handles removing it automatically if it's last in the menu, nice.

    // List directory
    // All we need is sub folder names, their permissions, their icon.
    // KDirLister or KIO::listDir would fetch much more info, and would be async,
    // and we only care about local directories so we use QDir directly.
    QDir dir(m_path);
    const QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::LocaleAware);
    const QMimeDatabase db;
    const QMimeType dirMime = db.mimeTypeForName(QStringLiteral("inode/directory"));
    for (const QString &subDir : entries) {
        QString subPath = m_path + subDir;
        KFileCopyToDirectoryMenu *subMenu = new KFileCopyToDirectoryMenu(this, m_mainMenu, subPath);
        QString menuTitle(subDir);
        // Replace '&' by "&&" to make sure that '&' inside the directory name is displayed
        // correctly and not misinterpreted as an indicator for a keyboard shortcut
        subMenu->setTitle(menuTitle.replace(QLatin1Char('&'), QLatin1String("&&")));
        const QString iconName = dirMime.iconName();
        subMenu->setIcon(QIcon::fromTheme(iconName));
        if (QFileInfo(subPath).isSymLink()) {
            QFont font = subMenu->menuAction()->font();
            font.setItalic(true);
            subMenu->menuAction()->setFont(font);
        }
        addMenu(subMenu);
    }
}
