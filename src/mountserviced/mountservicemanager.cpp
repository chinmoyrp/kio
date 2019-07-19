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

#include "mountservicemanager.h"
#include "kded_interface.h"

#include <QUrl>
#include <QDebug>
#include <QFileInfo>
#include <QDirIterator>
#include <QDBusInterface>

#include <KDirWatch>
#include <KPluginFactory>
#include <KPluginLoader>
#include <KPluginMetaData>
#include <KConfigGroup>
#include <KSharedConfig>


K_PLUGIN_FACTORY_WITH_JSON(MountServiceManagerFactory,
                           "mountservicemanager.json",
                           registerPlugin<MountServiceManager>();)

class Private
{
    public:
        Private(MountServiceManager *qq) : q(qq) {}

        void addMountService(const QString &pluginPath)
        {
            if (QLibrary::isLibrary(pluginPath)) {
                KPluginMetaData metadata(pluginPath);
                if (metadata.isValid()) {
                    const QStringList category = metadata.category().split(QLatin1Char('/')); // For e.g, MountService/smb or MountService/all
                    if (category.first() == QStringLiteral("MountService")) {
                        mountServices.insert(pluginPath, metadata);
                        emit q->availableServicesChanged(q->availableServices());
                        KConfigGroup cfg = KConfigGroup(KSharedConfig::openConfig(QStringLiteral("fusemanagerrc")), QStringLiteral("Fuse Services"));
                        cfg.writeEntry(category.last(), metadata.pluginId());
                        cfg.sync();
                    }
                }
            }
        }

        void removeMountService(const QString &pluginPath)
        {
            if (mountServices.remove(pluginPath) == 1) {
                emit q->availableServicesChanged(q->availableServices());
            }
        }

        KDirWatch *dirWatch;
        MountServiceManager *q;
        QMap<QString, KPluginMetaData> mountServices;
};

MountServiceManager::MountServiceManager(QObject *parent, const QList<QVariant> &parameters)
                    : KDEDModule(parent)
                    , d(new Private(this))
{
    Q_UNUSED(parameters);

    d->dirWatch = new KDirWatch(this);
    QSet<QString> dirsToWatch;
    const QString subdir = QLatin1String("kf5/kded");
    const QStringList listPaths = QCoreApplication::libraryPaths();
    for (const QString &libDir : listPaths) {
        dirsToWatch << libDir + QLatin1Char('/') + subdir;
    }

    for (const QString &dir : dirsToWatch) {
        d->dirWatch->addDir(dir);
        QDirIterator it(dir, QDir::Files);
        while (it.hasNext()) {
            it.next();
            d->addMountService(it.fileInfo().absoluteFilePath());
        }
    }
    connect(d->dirWatch, &KDirWatch::dirty, this, &MountServiceManager::dirty);
}

MountServiceManager::~MountServiceManager()
{
    delete d;
}

QStringList MountServiceManager::availableServices() const
{
    QStringList mountServices;
    for (KPluginMetaData metadata : d->mountServices) {
        mountServices << metadata.pluginId();
    }
    return mountServices;
}

void MountServiceManager::mountURL(const QString &remoteURL)
{
    const QUrl url = QUrl::fromUserInput(remoteURL);
    const QString mountService = getMountService(url);
    loadModule(mountService);
    QDBusInterface iface(QStringLiteral("org.kde.kded5"), QStringLiteral("/modules/%1").arg(mountService));
    iface.call(QDBus::NoBlock, QStringLiteral("mountUrl"), remoteURL);
}

QString MountServiceManager::localURL(const QString &remoteURL)
{
    const QUrl url = QUrl::fromUserInput(remoteURL);
    const QString mountService = getMountService(url);
    QDBusInterface iface(QStringLiteral("org.kde.kded5"), QStringLiteral("/modules/%1").arg(mountService));
    QDBusReply<QString> reply = iface.call(QStringLiteral("localUrl"), remoteURL);
    return reply.value();
}

void MountServiceManager::setAuthority(const QString &remoteURL, const QString &auth)
{
    const QUrl url = QUrl::fromUserInput(remoteURL);
    const QString mountService = getMountService(url);
    loadModule(mountService);
    QDBusInterface iface(QStringLiteral("org.kde.kded5"), QStringLiteral("/modules/%1").arg(mountService));
    iface.call(QDBus::NoBlock, QStringLiteral("setAuthority"),remoteURL, auth);
}

void MountServiceManager::dirty(const QString &path)
{
    if (QFile::exists(path)) {
        d->addMountService(path);
    } else {
        d->removeMountService(path);
    }
}

QString MountServiceManager::getMountService(const QUrl &url)
{
    KConfigGroup cfg = KConfigGroup(KSharedConfig::openConfig(QStringLiteral("fusemanagerrc")), QStringLiteral("Fuse Services"));
    QString mountService = cfg.readEntry(url.scheme(), QString());
    if (mountService.isEmpty()) {
        mountService = cfg.readEntry(QStringLiteral("all"), QString());
    }
    return mountService;
}

void MountServiceManager::loadModule(const QString &mountService)
{
    org::kde::kded5 kded(QStringLiteral("org.kde.kded5"), QStringLiteral("/kded"), QDBusConnection::sessionBus());
    kded.loadModule(mountService).waitForFinished();
}


#include "mountservicemanager.moc"
