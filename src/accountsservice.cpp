/*
 * This file is part of system-settings
 *
 * Copyright (C) 2013 Canonical Ltd.
 *
 * Contact: Iain Lane <iain.lane@canonical.com>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "accountsservice.h"

#include <QDBusReply>

#include <unistd.h>
#include <sys/types.h>

#define AS_SERVICE "org.freedesktop.Accounts"
#define AS_PATH "/org/freedesktop/Accounts"
#define AS_IFACE "org.freedesktop.Accounts"

AccountsService::AccountsService(QObject *parent)
    : QObject(parent),
      m_systemBusConnection(QDBusConnection::systemBus()),
      m_serviceWatcher(AS_SERVICE,
                       m_systemBusConnection,
                       QDBusServiceWatcher::WatchForOwnerChange),
      m_accountsserviceIface(AS_SERVICE,
                             AS_PATH,
                             AS_IFACE,
                             m_systemBusConnection)
{
    connect (&m_serviceWatcher,
             SIGNAL (serviceOwnerChanged (QString, QString, QString)),
             this,
             SLOT (slotNameOwnerChanged (QString, QString, QString)));

    if (m_accountsserviceIface.isValid()) {
        setUpInterface();
    }
}

void AccountsService::slotChanged(QString interface,
                                  QVariantMap changed_properties,
                                  QStringList invalidated_properties)
{
    Q_UNUSED (changed_properties);

    Q_FOREACH (const QString prop, invalidated_properties)
        Q_EMIT propertyChanged(interface, prop);
}


void AccountsService::slotNameOwnerChanged(QString name,
                                           QString oldOwner,
                                           QString newOwner)
{
    Q_UNUSED (oldOwner);
    Q_UNUSED (newOwner);
    if (name != "org.freedesktop.Accounts")
        return;

    setUpInterface();
    Q_EMIT (nameOwnerChanged());
}

void AccountsService::setUpInterface()
{
    QDBusReply<QDBusObjectPath> qObjectPath = m_accountsserviceIface.call(
                "FindUserById", qlonglong(getuid()));

    if (qObjectPath.isValid()) {
        m_objectPath = qObjectPath.value().path();
        m_accountsserviceIface.connection().connect(
            m_accountsserviceIface.service(),
            m_objectPath,
            "org.freedesktop.DBus.Properties",
            "PropertiesChanged",
            this,
            SLOT(slotChanged(QString, QVariantMap, QStringList)));
    }
}

QVariant AccountsService::getUserProperty(const QString &interface,
                                          const QString &property)
{
    if (!m_accountsserviceIface.isValid())
        return QVariant();

    QDBusInterface iface (
                "org.freedesktop.Accounts",
                m_objectPath,
                "org.freedesktop.DBus.Properties",
                m_systemBusConnection,
                this);

    if (iface.isValid()) {
        QDBusReply<QDBusVariant> answer = iface.call(
                    "Get",
                    interface,
                    property);
        if (answer.isValid()) {
            return answer.value().variant();
        }
    }
    return QVariant();
}

void AccountsService::setUserProperty(const QString &interface,
                                      const QString &property,
                                      const QVariant &value)
{
    QDBusInterface iface (
                "org.freedesktop.Accounts",
                m_objectPath,
                "org.freedesktop.DBus.Properties",
                m_systemBusConnection,
                this);
    if (iface.isValid()) {
        // The value needs to be carefully wrapped
        iface.call("Set",
                   interface,
                   property,
                   QVariant::fromValue(QDBusVariant(value)));
    }
}
