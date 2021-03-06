/*  This file is part of the KDE libraries

    Copyright (C) 2015 Christoph Cullmann <cullmann@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QVariant>
#include <QRegularExpression>

#include <KConfig>
#include <KConfigGroup>

int main (int argc, char *argv[])
{
    // construct app and set some version
    QCoreApplication app(argc, argv);
    app.setApplicationVersion(QStringLiteral("1.0"));

    // generic parser config
    QCommandLineParser parser;
    parser.addVersionOption();
    parser.setApplicationDescription(QStringLiteral("Converts .protocol files to json"));
    parser.addHelpOption();

    // -o/--output
    const QCommandLineOption outputOption(QStringList() << QStringLiteral("o") << QStringLiteral("output"), QStringLiteral("Output file name for JSON data."), QStringLiteral("name"));
    parser.addOption(outputOption);

    // files to open
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral(".protocol files to read."), QStringLiteral("[files...]"));

    // parse
    parser.process(app);

    // we need an output file
    const QString output = parser.value(QStringLiteral("output"));
    if (output.isEmpty()) {
        qFatal("No output file given, please add --output <name>.");
    }

    // we need input files
    if (parser.positionalArguments().isEmpty()) {
        qFatal("No input file given, please add <files>.");
    }

    // different attributes to copy
    QStringList stringAttributes;
    stringAttributes << QStringLiteral("protocol") << QStringLiteral("exec")
        << QStringLiteral("fileNameUsedForCopying") << QStringLiteral("defaultMimetype")
        << QStringLiteral("Icon") << QStringLiteral("config") << QStringLiteral("input")
        << QStringLiteral("output") << QStringLiteral("X-DocPath") << QStringLiteral("DocPath")
        << QStringLiteral("Class") << QStringLiteral("ProxiedBy");

    QStringList stringListAttributes;
    stringListAttributes << QStringLiteral("listing") << QStringLiteral("archiveMimetype")
        << QStringLiteral("ExtraTypes") << QStringLiteral("Capabilities");

    QStringList boolAttributes;
    boolAttributes << QStringLiteral("source") << QStringLiteral("helper")
        << QStringLiteral("reading") << QStringLiteral("writing")
        << QStringLiteral("makedir") << QStringLiteral("deleting")
        << QStringLiteral("linking") << QStringLiteral("moving")
        << QStringLiteral("opening") << QStringLiteral("copyFromFile")
        << QStringLiteral("copyToFile") << QStringLiteral("renameFromFile")
        << QStringLiteral("renameToFile") << QStringLiteral("deleteRecursive")
        << QStringLiteral("determineMimetypeFromExtension") << QStringLiteral("ShowPreviews");

    QStringList intAttributes;
    intAttributes << QStringLiteral("maxInstances") << QStringLiteral("maxInstancesPerHost");

    QStringList translatedStringListAttributes;
    translatedStringListAttributes << QStringLiteral("ExtraNames");

    // constructed the json data by parsing all .protocol files
    QVariantMap protocolsData;
    const QRegularExpression localeRegex(QStringLiteral("^\\w+\\[(.*)\\]="));
    Q_FOREACH(const QString &file, parser.positionalArguments()) {
        // get full path
        const QString fullFilePath(QDir::current().absoluteFilePath(file));

        // construct kconfig for protocol file
        KConfig sconfig(fullFilePath);
        sconfig.setLocale(QString());
        KConfigGroup config(&sconfig, "Protocol");

        // name must be set, sanity check that file got read
        const QString name(config.readEntry("protocol"));
        if (name.isEmpty()) {
            qFatal("Failed to read input file %s.", qPrintable(fullFilePath));
        }

        // construct protocol data
        QVariantMap protocolData;

        // convert the different types
        Q_FOREACH(const QString &key, stringAttributes) {
            if (config.hasKey(key)) {
                protocolData.insert(key, config.readEntry(key, QString()));
            }
        }
        Q_FOREACH(const QString &key, stringListAttributes) {
            if (config.hasKey(key)) {
                protocolData.insert(key, config.readEntry(key, QStringList()));
            }
        }
        Q_FOREACH(const QString &key, boolAttributes) {
            if (config.hasKey(key)) {
                protocolData.insert(key, config.readEntry(key, bool(false)));
            }
        }
        Q_FOREACH(const QString& key, intAttributes) {
            if (config.hasKey(key)) {
                protocolData.insert(key, config.readEntry(key, int(0)));
            }
        }

        // handle translated keys
        Q_FOREACH(const QString &key, translatedStringListAttributes) {
            // read untranslated entry first in any case!
            sconfig.setLocale(QString());
            protocolData.insert(key, config.readEntry(key, QStringList()));

            // collect all locales for this key
            QFile f(fullFilePath);
            QStringList foundLocalesForKey;
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qFatal("Failed to open file %s.", qPrintable(fullFilePath));
            }
            while (!f.atEnd()) {
                const QString line = QString::fromUtf8(f.readLine());
                const QRegularExpressionMatch match = localeRegex.match(line);
                if (match.hasMatch()) {
                    foundLocalesForKey.append(match.captured(1));
                }
            }

            // insert all entries for all found locales, reparse config for this
            for (const QString &locale : qAsConst(foundLocalesForKey)) {
                sconfig.setLocale(locale);
                protocolData.insert(QStringLiteral("%1[%2]").arg(key, locale), config.readEntry(key, QStringList()));
            }
        }

        // use basename of protocol for toplevel map in json, like it is done for .protocol files
        const QString baseName(QFileInfo (fullFilePath).baseName());
        protocolsData.insert(baseName, protocolData);
    }

    // pack in our namespace
    QVariantMap jsonData;
    jsonData.insert(QStringLiteral("KDE-KIO-Protocols"), protocolsData);

    // create outfile, after all has worked!
    QFile outFile(output);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qFatal("Failed to open output file %s.", qPrintable(output));
    }

    // write out json
    outFile.write(QJsonDocument::fromVariant(QVariant(jsonData)).toJson());

    // be done
    return 0;
}
