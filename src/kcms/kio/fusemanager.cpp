/***
    Copyright (C) 2019 by Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
***/


#include <QDebug>
#include <QCheckBox>
#include <QPushButton>
#include <QTreeWidget>

#include <KConfigGroup>
#include <KSharedConfig>
#include <KPluginFactory>
#include <KMessageBox>

#include "fusemanager.h"
#include "mountserviced_interface.h"

K_PLUGIN_FACTORY_DECLARATION(KioConfigFactory)

FuseServiceSelectorDlg::FuseServiceSelectorDlg (QWidget* parent, const QList<QString> &fuseServices)
    : QDialog (parent)
    , mButtonBox(nullptr)
{
    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(mainWidget);

    mUi.setupUi(mainWidget);
    mUi.cbServices->setMinimumWidth(mUi.cbServices->fontMetrics().maxWidth() * 15);
    mUi.cbServices->addItems(fuseServices);
    mUi.leProtocol->setFocus();

    mButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(mButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(mButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(mButtonBox);

    connect(mUi.leProtocol, &QLineEdit::textEdited, this, &FuseServiceSelectorDlg::slotTextChanged);
    connect(mUi.cbServices, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FuseServiceSelectorDlg::slotServiceChanged);
}

FuseServiceSelectorDlg::~FuseServiceSelectorDlg() = default;

int FuseServiceSelectorDlg::service() const
{
    return mUi.cbServices->currentIndex();
}

QString FuseServiceSelectorDlg::protocol() const
{
    return mUi.leProtocol->text();
}

void FuseServiceSelectorDlg::setProtocolService(const QString &protocol, int serviceIndex)
{
    mUi.leProtocol->setText(protocol);
    mUi.leProtocol->setReadOnly(true);
    mUi.cbServices->setCurrentIndex(serviceIndex);
}

void FuseServiceSelectorDlg::slotTextChanged(const QString &text)
{
    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(mUi.cbServices->currentIndex() > 0 && text.length() > 1);
}

void FuseServiceSelectorDlg::slotServiceChanged(int serviceIndex)
{
    mButtonBox->button(QDialogButtonBox::Ok)->setEnabled(serviceIndex > 0 && mUi.leProtocol->text().length() > 1);
}

FuseManager::FuseManager(QWidget *parent, const QVariantList &)
    : KCModule(parent), mSelectedItemsCount(0)
{
    mUi.setupUi(this);
    mAvailableServices.append(i18n("No Service"));
    org::kde::kio::MountServiceManager kded(QStringLiteral("org.kde.kded5"), QStringLiteral("/modules/mountservicemanager"), QDBusConnection::sessionBus());
    QDBusConnection::sessionBus().connect(QStringLiteral("org.kde.kded5"), QStringLiteral("/modules/mountservicemanager"),
                                          QStringLiteral("org.kde.kio.MountServiceManager"), QStringLiteral("availableServicesChanged"), this, SLOT(availableServicesChanged));
    mAvailableServices.append(kded.availableServices());


    mUi.bAdd->setIcon (QIcon::fromTheme(QStringLiteral("list-add")));
    mUi.bChange->setIcon (QIcon::fromTheme(QStringLiteral("edit-rename")));
    mUi.bDelete->setIcon (QIcon::fromTheme(QStringLiteral("list-remove")));
    mUi.bDeleteAll->setIcon (QIcon::fromTheme(QStringLiteral("edit-delete")));

    connect(mUi.bAdd, &QAbstractButton::clicked, this, &FuseManager::addPressed);
    connect(mUi.bChange, &QAbstractButton::clicked, this, &FuseManager::changePressed);
    connect(mUi.bDelete, &QAbstractButton::clicked, this, &FuseManager::deletePressed);
    connect(mUi.bDeleteAll, &QAbstractButton::clicked, this, &FuseManager::deleteAllPressed);
    connect(mUi.serviceTreeWidget, &QTreeWidget::itemSelectionChanged, this, &FuseManager::selectionChanged);
}

FuseManager::~FuseManager() = default;

void FuseManager::load()
{
    KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("fusemanagerrc")), QStringLiteral("Fuse Services"));
    const QStringList handledProtocols = group.keyList();
    for (const QString &protocol : handledProtocols) {
        const QString service = group.readEntry(protocol);
        if (mAvailableServices.contains(service)) {
            auto item = new QTreeWidgetItem({protocol, service});
            mUi.serviceTreeWidget->addTopLevelItem(item);
            mManagedProtocols.insert(protocol, service);
        } else {
            mProtocolsToRemove.append(protocol);
        }
    }
    mUi.serviceTreeWidget->sortItems(0, Qt::AscendingOrder);
    updateButtons();
}

void FuseManager::save()
{
    KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("fusemanagerrc")), QStringLiteral("Fuse Services"));
    for (const QString &protocol : mManagedProtocols.keys()) {
        group.writeEntry(protocol, mManagedProtocols[protocol]);
    }
    for (const QString &protocol : mProtocolsToRemove) {
        group.deleteEntry(protocol);
    }
    group.sync();
    emit changed(false);
}

void FuseManager::addPressed()
{
    FuseServiceSelectorDlg pdlg (this, mAvailableServices);
    pdlg.setWindowTitle (i18nc ("@title:window", "Assign Mount Service"));
    if (pdlg.exec()) {
        const QString protocol = pdlg.protocol();
        const QString service = mAvailableServices.at(pdlg.service());
        if (!handleDuplicate(protocol, service)) {
            auto item = new QTreeWidgetItem({protocol, service});
            mUi.serviceTreeWidget->addTopLevelItem(item);
            mManagedProtocols.insert(protocol, service);
            updateButtons();
            emit changed(true);
        }
    }
}

void FuseManager::changePressed()
{
    QTreeWidgetItem *item = mUi.serviceTreeWidget->currentItem();
    Q_ASSERT(item);
    FuseServiceSelectorDlg pdlg (this, mAvailableServices);
    pdlg.setWindowTitle (i18nc ("@title:window", "Change Mount Service"));
    const QString oldProtocol = item->text(0);
    const int oldService = mAvailableServices.indexOf(item->text(1));
    pdlg.setProtocolService(oldProtocol, oldService);
    if (pdlg.exec() && !pdlg.protocol().isEmpty()) {
            item->setText(0, pdlg.protocol());
            item->setText(1, mAvailableServices.at(pdlg.service()));
    }
}

bool FuseManager::handleDuplicate(const QString& protocol, const QString &service)
{
    QTreeWidgetItem* item = mUi.serviceTreeWidget->topLevelItem(0);
    while (item != nullptr) {
        if (item->text(0) == protocol) {
            const int res = KMessageBox::warningContinueCancel (this,
                            i18n ("<qt>A service is already set for"
                                  "<center><b>%1</b></center>"
                                  "Do you want to update it?</qt>", protocol),
                            i18nc ("@title:window", "Update Service"),
                            KGuiItem (i18n ("Replace")));
            if (res == KMessageBox::Continue) {
                mManagedProtocols[protocol] = service;
                item->setText(0, protocol);
                item->setText(1, service);
                emit changed(true);
            }
            return true;
        }
        item = mUi.serviceTreeWidget->itemBelow(item);
    }
    return false;
}

void FuseManager::deletePressed()
{
    QTreeWidgetItem* nextItem = nullptr;
    for (QTreeWidgetItem * item : mUi.serviceTreeWidget->selectedItems()) {
        nextItem = mUi.serviceTreeWidget->itemBelow(item);
        if (!nextItem)
            nextItem = mUi.serviceTreeWidget->itemAbove(item);

        mManagedProtocols.remove(item->text(0));
        mProtocolsToRemove.append(item->text(0));
        delete item;
    }
    if (nextItem) {
        nextItem->setSelected(true);
    }
    updateButtons();
    emit changed(true);
}

void FuseManager::deleteAllPressed()
{
    mProtocolsToRemove.append(mManagedProtocols.keys());
    mManagedProtocols.clear();
    mUi.serviceTreeWidget->clear();
    updateButtons();
    emit changed(true);
}

void FuseManager::updateButtons()
{
    bool hasItems = mUi.serviceTreeWidget->topLevelItemCount() > 0;

    mUi.bChange->setEnabled((hasItems && mSelectedItemsCount == 1));
    mUi.bDelete->setEnabled((hasItems && mSelectedItemsCount > 0));
    mUi.bDeleteAll->setEnabled(hasItems);
}

void FuseManager::selectionChanged()
{
    mSelectedItemsCount = mUi.serviceTreeWidget->selectedItems().count();
    updateButtons();
}

void FuseManager::availableServicesChanged(const QStringList &services)
{
    qDebug() << "available service changed";
    mAvailableServices.clear();
    mAvailableServices.append(i18n("No Service"));
    mAvailableServices.append(services);
}

QString FuseManager::quickHelp() const
{
    return i18n( "<h1>Fuse Manager</h1><p>This module lets you configure a FUSE mount service for KIO Slaves.</p>"
                "<p>The remote KIO slave can make use of the service to provide a local mount." );
}
