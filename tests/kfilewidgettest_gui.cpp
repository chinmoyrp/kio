/* This file is part of the KDE libraries
    Copyright (C) 2015 David Faure <faure@kde.org>

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

#include <QApplication>
#include <KFileWidget>
#include <QUrl>
#include <QDebug>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    KFileWidget* fileWidget = new KFileWidget(QUrl(QStringLiteral("kfiledialog:///OpenDialog")), nullptr);
    fileWidget->setMode(KFile::Files | KFile::ExistingOnly);
    fileWidget->setAttribute(Qt::WA_DeleteOnClose);
    fileWidget->show();

    QObject::connect(fileWidget, &KFileWidget::destroyed, &app, &QApplication::quit);

    return app.exec();
}

