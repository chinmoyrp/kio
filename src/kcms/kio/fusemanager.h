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

#ifndef FUSEMANAGER_H
#define FUSEMANAGER_H

#include <QDialog>

#include <KCModule>

#include "ui_fusemanager.h"
#include "ui_fuseserviceselector.h"

class QDialogButtonBox;

class FuseManager : public KCModule
{
  Q_OBJECT

public:
  FuseManager(QWidget *parent, const QVariantList &args);
  ~FuseManager();

  void load() override;
  void save() override;
  QString quickHelp() const override;

private Q_SLOTS:
  void addPressed();
  void changePressed();
  void deletePressed();
  void deleteAllPressed();

  void updateButtons();
  void selectionChanged();
  void availableServicesChanged(const QStringList &services);
  bool handleDuplicate(const QString &protocol, const QString &handler);

private:
  int mSelectedItemsCount;
  QStringList mAvailableServices;
  QStringList mProtocolsToRemove;
  QMap<QString, QString> mManagedProtocols;
  Ui::FuseManagerUI mUi;
};

class FuseServiceSelectorDlg : public QDialog
{
    Q_OBJECT

public:
    explicit FuseServiceSelectorDlg(QWidget* parent, const QList<QString> &fuseHandlers);
    ~FuseServiceSelectorDlg();

    int service() const;
    QString protocol() const;

    void setProtocolService(const QString &protocol, int serviceIndex);

protected Q_SLOTS:
    void slotTextChanged (const QString &text);
    void slotServiceChanged(int serviceIndex);

private:
    QDialogButtonBox *mButtonBox;
    Ui::FuseServiceSelectorDlgUI mUi;
};

#endif // FUSEMANAGER_H

