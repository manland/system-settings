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
 * Authors:
 * Ken VanDine <ken.vandine@canonical.com>
 *
*/

#include "connman.h"

ConnMan::ConnMan()
{
    m = new OfonoConnMan(OfonoModem::AutomaticSelect, QString(), NULL);
    QObject::connect(m, SIGNAL(roamingAllowedChanged(bool)), this, SLOT(onRoamingAllowedChanged(bool)));
    m_roam = m->roamingAllowed();

    qDebug() << "ROAMING ALLOWED: " << m_roam;

    /*
    if (m->modem()->isValid())
    {
        qDebug() << "Found valid modem";
        qDebug() << m->name();

    } else
    {
        qDebug() << "No modem found";
    }
    */
}

bool ConnMan::roamingAllowed() const
{
    return m_roam;
}

void ConnMan::setRoamingAllowed(const bool &st)
{
    m->setRoamingAllowed(st);
}

void ConnMan::onRoamingAllowedChanged(bool st)
{
    m_roam = st;
    emit roamingAllowedChanged();
    qDebug() << "ROAMING ALLOWED: " << m_roam;
}
