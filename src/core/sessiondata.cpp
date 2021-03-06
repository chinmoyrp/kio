/* This file is part of the KDE project
   Copyright (C) 2000 Dawit Alemayehu <adawit@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License (LGPL) as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any
   later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street,
   Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include "sessiondata_p.h"

#include <QDir>
#include <QList>
#include <QStandardPaths>
#include <QTextCodec>

#include <kconfiggroup.h>
#include <klocalizedstring.h>
#include <kprotocolmanager.h>
#include <ksharedconfig.h>

#include <kio/slaveconfig.h>
#include "http_slave_defaults.h"

namespace KIO
{

/***************************** SessionData::AuthData ************************/
#if 0
struct SessionData::AuthData {

public:
    AuthData() {}

    AuthData(const QByteArray &k, const QByteArray &g, bool p)
    {
        key = k;
        group = g;
        persist = p;
    }

    bool isKeyMatch(const QByteArray &val) const
    {
        return (val == key);
    }

    bool isGroupMatch(const QByteArray &val) const
    {
        return (val == group);
    }

    QByteArray key;
    QByteArray group;
    bool persist;
};
#endif

/********************************* SessionData ****************************/

class SessionData::SessionDataPrivate
{
public:
    SessionDataPrivate()
    {
        useCookie = true;
        initDone = false;
    }

    bool initDone;
    bool useCookie;
    QString charsets;
    QString language;
};

SessionData::SessionData()
    : d(new SessionDataPrivate)
{
//  authData = 0;
}

SessionData::~SessionData()
{
    delete d;
}

void SessionData::configDataFor(MetaData &configData, const QString &proto,
                                const QString &)
{
    if ((proto.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) ||
            (proto.startsWith(QLatin1String("webdav"), Qt::CaseInsensitive))) {
        if (!d->initDone) {
            reset();
        }

        // These might have already been set so check first
        // to make sure that we do not trumpt settings sent
        // by apps or end-user.
        if (configData[QStringLiteral("Cookies")].isEmpty()) {
            configData[QStringLiteral("Cookies")] = d->useCookie ? QStringLiteral("true") : QStringLiteral("false");
        }
        if (configData[QStringLiteral("Languages")].isEmpty()) {
            configData[QStringLiteral("Languages")] = d->language;
        }
        if (configData[QStringLiteral("Charsets")].isEmpty()) {
            configData[QStringLiteral("Charsets")] = d->charsets;
        }
        if (configData[QStringLiteral("CacheDir")].isEmpty()) {
            const QString httpCacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/kio_http");
            QDir().mkpath(httpCacheDir);
            configData[QStringLiteral("CacheDir")] = httpCacheDir;
        }
        if (configData[QStringLiteral("UserAgent")].isEmpty()) {
            configData[QStringLiteral("UserAgent")] = KProtocolManager::defaultUserAgent();
        }
    }
}

void SessionData::reset()
{
    d->initDone = true;
    // Get Cookie settings...
    d->useCookie = KSharedConfig::openConfig(QStringLiteral("kcookiejarrc"), KConfig::NoGlobals)->
                   group("Cookie Policy").
                   readEntry("Cookies", true);

    d->language = KProtocolManager::acceptLanguagesHeader();
    d->charsets = QString::fromLatin1(QTextCodec::codecForLocale()->name()).toLower();
    KProtocolManager::reparseConfiguration();
}

}
