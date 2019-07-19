/* This file is part of the KDE libraries
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
*/

#ifndef MOUNTSERVICEMANAGER_H
#define MOUNTSERVICEMANAGER_H

#include <KDEDModule>
#include <QDBusAbstractAdaptor>

class Private;
class KPluginMetaData;

class MountServiceManager : public KDEDModule
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kio.MountServiceManager")

    public:
        MountServiceManager(QObject* parent, const QList<QVariant> &parameters);
        ~MountServiceManager();

    public Q_SLOTS:
        Q_SCRIPTABLE QStringList availableServices() const;
        Q_SCRIPTABLE void mountURL(const QString &remoteURL);
        Q_SCRIPTABLE QString localURL(const QString &remoteURL);
        Q_SCRIPTABLE void setAuthority(const QString &remoteURL, const QString &auth);

    Q_SIGNALS:
       Q_SCRIPTABLE void availableServicesChanged(const QStringList &services);

    private Q_SLOTS:
        void dirty(const QString &path);

    private:
        QString getMountService(const QUrl &url);
        void loadModule(const QString &mountService);

    private:
        Private *d;
};

#endif
