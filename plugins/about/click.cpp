/*
 * Copyright (C) 2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Iain Lane <iain.lane@canonical.com>
 *
*/

#include "click.h"

#include <glib.h>
#include <libintl.h>

#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

const char * try_get_desktop_file_key (GKeyFile *keyfile,
                                       const char * key)
{
    GError *err = NULL;
    char *value = g_key_file_get_string (
                keyfile,
                G_KEY_FILE_DESKTOP_GROUP,
                key,
                &err
                );
    if (err) {
        g_warning (
            "Desktop file doesn't have a key %s", key);
        g_error_free (err);
        return NULL;
    }

    return value;
}

ClickModel::ClickModel(QObject *parent):
    QAbstractTableModel(parent),
    m_totalClickSize(0)
{
    m_clickPackages = buildClickList();
}

QList<ClickModel::Click> ClickModel::buildClickList()
{
    QFile clickBinary("/usr/bin/click");
    if (!clickBinary.exists()) {
        return QList<ClickModel::Click>();
    }

    QProcess clickProcess;
    clickProcess.start("/usr/bin/click",
                       QStringList() << "list" << "--all" << "--manifest");

    if (!clickProcess.waitForFinished(5000)) {
        qWarning() << "Timeout retrieving the list of click packages";
        return QList<ClickModel::Click>();
    }

    if (clickProcess.exitStatus() == QProcess::CrashExit) {
        qWarning() << "The click utility exited abnormally" <<
                      clickProcess.readAllStandardError();
        return QList<ClickModel::Click>();
    }

    QJsonParseError error;

    QJsonDocument jsond =
            QJsonDocument::fromJson(clickProcess.readAllStandardOutput(),
                                    &error);

    if (error.error != QJsonParseError::NoError) {
        qWarning() << error.errorString();
        return QList<ClickModel::Click>();
    }

    QJsonArray data(jsond.array());

    QJsonArray::ConstIterator begin(data.constBegin());
    QJsonArray::ConstIterator end(data.constEnd());

    QList<ClickModel::Click> clickPackages;

    while (begin != end) {
        QVariantMap val = (*begin++).toObject().toVariantMap();
        Click newClick;
        QDir directory;

        newClick.name = val.value("title",
                                  gettext("Unknown title")).toString();

        if (val.contains("_directory")) {
            directory = val.value("_directory", "/undefined").toString();
            // Set the icon from the click package
            QString iconFile(val.value("icon", "undefined").toString());
            if (directory.exists()) {
                QString icon(directory.filePath(iconFile.simplified()));
                newClick.icon = icon;
            }
        }

        // "hooks" → title → "desktop" / "icon"
        QVariant hooks(val.value("hooks"));

        if (hooks.isValid()) {
            QVariantMap allHooks(hooks.toMap());

            if (allHooks.contains(newClick.name)) {
                QVariantMap appHooks(allHooks.value(newClick.name).toMap());
                if (!appHooks.isEmpty() &&
                    appHooks.contains("desktop") &&
                    directory.exists()) {
                    QFile desktopFile(
                                directory.absoluteFilePath(
                                    appHooks.value("desktop",
                                                   "undefined").toString()));
                    const char * desktopFileName =
                        desktopFile.fileName().toLocal8Bit().constData();
                    g_debug ("Desktop file: %s", desktopFileName);
                    if (desktopFile.exists()) {
                        GError *err = NULL;
                        GKeyFile *keyfile = g_key_file_new();

                        g_key_file_load_from_file (keyfile,
                                                   desktopFileName,
                                                   G_KEY_FILE_NONE,
                                                   &err);
                        if (err) {
                            g_warning ("Couldn't parse desktop file %s: %s",
                                       desktopFileName,
                                       err->message);
                            g_error_free (err);
                        } else {
                            const char * name = try_get_desktop_file_key(
                                        keyfile,
                                        G_KEY_FILE_DESKTOP_KEY_NAME);
                            if (name) {
                                g_debug ("Name is %s", name);
                                newClick.displayName = name;
                            }

                            // Overwrite the icon with the .desktop file's one
                            // if we have it. This one is more reliable.
                            const char * icon = try_get_desktop_file_key(
                                        keyfile,
                                        G_KEY_FILE_DESKTOP_KEY_ICON
                                        );
                            if (icon) {
                                g_debug ("Icon is %s", icon);
                                newClick.icon = directory.absoluteFilePath(
                                            QString::fromLocal8Bit(icon));
                            }
                        }
                    }
                }
            }
        }

        newClick.installSize = val.value("installed-size",
                                         "0").toString().toUInt()*1024;

        m_totalClickSize += newClick.installSize;

        clickPackages.append(newClick);
    }

    return clickPackages;
}

int ClickModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_clickPackages.count();
}

int ClickModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 3; //Display, size, icon
}

QHash<int, QByteArray> ClickModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;

    roleNames[Qt::DisplayRole] = "displayName";
    roleNames[InstalledSizeRole] = "installedSize";
    roleNames[IconRole] = "iconPath";

    return roleNames;
}

QVariant ClickModel::data(const QModelIndex &index, int role) const
{
    if (index.row() > m_clickPackages.count() ||
            index.row() < 0)
        return QVariant();

    Click click = m_clickPackages[index.row()];

    switch (role) {
    case Qt::DisplayRole:
        if (click.displayName.isEmpty() || click.displayName.isNull())
            return click.name;
        else
            return click.displayName;
    case InstalledSizeRole:
        return click.installSize;
    case IconRole:
        return click.icon;
    default:
        qWarning() << "Unknown role requested";
        return QVariant();
    }
}

quint64 ClickModel::getClickSize() const
{
    return m_totalClickSize;
}

ClickModel::~ClickModel()
{
}

ClickFilterProxy::ClickFilterProxy(ClickModel *parent)
    : QSortFilterProxyModel(parent)
{
    this->setSourceModel(parent);
    this->setDynamicSortFilter(false);
    this->setSortCaseSensitivity(Qt::CaseInsensitive);
    this->sort(0);
}
