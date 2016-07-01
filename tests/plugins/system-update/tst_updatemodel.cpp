/*
 * This file is part of system-settings
 *
 * Copyright (C) 2016 Canonical Ltd.
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

#include <QDate>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTime>
#include <QTimeZone>
#include <QTest>

#include "update.h"
#include "updatemodel.h"

using namespace UpdatePlugin;

class TstUpdateModel : public QObject
{
    Q_OBJECT
private slots:
    void init()
    {
        m_model = new UpdateModel(":memory:");
        m_db = m_model->db();
    }
    void cleanup()
    {
        QSignalSpy destroyedSpy(m_model, SIGNAL(destroyed(QObject*)));
        m_model->deleteLater();
        QTRY_COMPARE(destroyedSpy.count(), 1);
    }
    QSharedPointer<Update> createUpdate()
    {
        return QSharedPointer<Update>(new Update);
    }
    void testNoUpdates()
    {
        QCOMPARE(m_model->count(), 0);
    }
    void testAdd()
    {
        QSharedPointer<Update> m = createUpdate();
        m->setIdentifier("test.app");
        m->setRevision(1);

        QSignalSpy insertedSpy(m_model, SIGNAL(rowsAboutToBeInserted(const QModelIndex&, int, int)));
        m_db->add(m);
        QTRY_COMPARE(insertedSpy.count(), 1);
        QCOMPARE(m_model->count(), 1);
    }
    void testAddMultiple()
    {
        QSharedPointer<Update> a = createUpdate();
        a->setIdentifier("a.app");
        a->setRevision(1);
        QSharedPointer<Update> b = createUpdate();
        b->setIdentifier("b.app");
        b->setRevision(1);
        QSharedPointer<Update> c = createUpdate();
        c->setIdentifier("c.app");
        c->setRevision(1);

        m_db->add(a);
        m_db->add(b);
        m_db->add(c);

        QCOMPARE(m_model->count(), 3);
    }
    void testRemove()
    {
        QSharedPointer<Update> a = createUpdate();
        a->setIdentifier("a.app");
        a->setRevision(1);

        m_db->add(a);
        QCOMPARE(m_model->count(), 1);

        QSignalSpy removeSpy(m_model, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)));
        m_db->remove(a);
        QTRY_COMPARE(removeSpy.count(), 1);
        QCOMPARE(m_model->count(), 0);
    }
    void testMoveUp()
    {
        /* For moving, we need a filter that sorts. Pending updates are sorted
        by title ASC. (See UpdateDb::GET_PENDING) */
        m_model->setFilter(UpdateModel::Filter::Pending);

        QSharedPointer<Update> a = createUpdate();
        a->setIdentifier("first.app");
        a->setRevision(1);
        a->setTitle("ABC");

        QSharedPointer<Update> b = createUpdate();
        b->setIdentifier("second.app");
        b->setRevision(1);
        b->setTitle("CED");

        m_db->add(a);
        m_db->add(b);

        QCOMPARE(m_model->data(m_model->index(0, 0), UpdateModel::Roles::IdRole).toString(),
                 a->identifier());
        QCOMPARE(m_model->data(m_model->index(1, 0), UpdateModel::Roles::IdRole).toString(),
                 b->identifier());

        QSqlQuery q(m_db->db());
        q.prepare("UPDATE updates SET title=:title WHERE id=:id AND revision=:revision");
        q.bindValue(":title", "XYZ");
        q.bindValue(":id", a->identifier());
        q.bindValue(":revision", a->revision());
        q.exec();
        q.finish();

        QSignalSpy moveSpy(
            m_model, SIGNAL(rowsAboutToBeMoved(const QModelIndex&, int, int, const QModelIndex&, int))
        );
        m_model->refresh();

        // Moved and reversed.
        QCOMPARE(m_model->data(m_model->index(1, 0), UpdateModel::Roles::IdRole).toString(),
                 a->identifier());
        QCOMPARE(m_model->data(m_model->index(0, 0), UpdateModel::Roles::IdRole).toString(),
                 b->identifier());

        QTRY_COMPARE(moveSpy.count(), 1);

        QList<QVariant> args = moveSpy.takeFirst();
        // sourceStart == sourceEnd, but destinationRow is 0
        QCOMPARE(args.at(1).toInt(), 1);
        QCOMPARE(args.at(2).toInt(), 1);
        QCOMPARE(args.at(4).toInt(), 0);
    }
    void testChange()
    {
        QSharedPointer<Update> m = createUpdate();
        m->setIdentifier("test.app");
        m->setRevision(1);
        m->setTitle("old");
        m_db->add(m);

        m->setTitle("updated");
        QSignalSpy dataChangedSpy(m_model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)));
        m_db->add(m);
        QTRY_COMPARE(dataChangedSpy.count(), 1);
        QList<QVariant> args = dataChangedSpy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 0);
    }
    void testMultipleChanges()
    {
        QSharedPointer<Update> a = createUpdate();
        a->setIdentifier("a.app");
        a->setRevision(1);
        a->setTitle("a");

        QSharedPointer<Update> b = createUpdate();
        b->setIdentifier("b.app");
        b->setRevision(1);
        b->setTitle("b");

        QSharedPointer<Update> c = createUpdate();
        c->setIdentifier("c.app");
        c->setRevision(1);
        c->setTitle("c");

        m_db->add(a);
        m_db->add(b);
        m_db->add(c);

        QVector<QSharedPointer<Update>> list;
        list << a << b << c;

        // Change three titles.
        Q_FOREACH(const QSharedPointer<Update> u, list) {
            QSqlQuery q(m_db->db());
            q.prepare("UPDATE updates SET title=:title WHERE id=:id AND revision=:revision");
            q.bindValue(":title", u->title() + "-new");
            q.bindValue(":id", u->identifier());
            q.bindValue(":revision", u->revision());
            q.exec();
            q.finish();
        }

        QSignalSpy dataChangedSpy(m_model, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)));
        m_model->refresh();
        QTRY_COMPARE(dataChangedSpy.count(), 3);
        QList<QVariant> args = dataChangedSpy.takeFirst();
    }
    // TODO: move this test to updatedb.
    void testSupersededUpdate()
    {
        QSharedPointer<Update> superseded = createUpdate();
        superseded->setIdentifier("some.app");
        superseded->setRevision(1);
        superseded->setKind(Update::Kind::KindClick);

        QSharedPointer<Update> replacement = createUpdate();
        replacement->setIdentifier("some.app");
        replacement->setRevision(2);
        replacement->setKind(Update::Kind::KindClick);

        m_db->add(superseded);
        m_db->add(replacement);

        // We only want the replacement in our model of pending updates.
        m_model->setFilter(UpdateModel::Filter::PendingClicks);
        QCOMPARE(m_model->count(), 1);
        QCOMPARE(m_model->data(
            m_model->index(0), UpdateModel::IdRole
        ).toString(), replacement->identifier());
    }
    void testRoles()
    {
        QSharedPointer<Update> app = createUpdate();
        QStringList mc;
        mc << "ls" << "la";

        app->setKind(Update::Kind::KindClick);
        app->setIdentifier("com.ubuntu.testapp");
        app->setLocalVersion("0.1");
        app->setRemoteVersion("0.2");
        app->setRevision(42);
        app->setTitle("Test App");
        app->setDownloadHash("987654323456789");
        app->setBinaryFilesize(1000);
        app->setIconUrl("http://example.org/testapp.png");
        app->setDownloadUrl("http://example.org/testapp.click");
        app->setCommand(mc);
        app->setChangelog("* Fixed all bugs * Introduced new bugs");
        app->setToken("token");
        app->setProgress(50);

        m_db->add(app);
        m_model->refresh();

        QModelIndex idx = m_model->index(0);

        QCOMPARE(m_model->data(idx, UpdateModel::KindRole).toUInt(), (uint) Update::Kind::KindClick);
        QCOMPARE(m_model->data(idx, UpdateModel::IdRole).toString(), app->identifier());
        QCOMPARE(m_model->data(idx, UpdateModel::LocalVersionRole).toString(), app->localVersion());
        QCOMPARE(m_model->data(idx, UpdateModel::RemoteVersionRole).toString(), app->remoteVersion());
        QCOMPARE(m_model->data(idx, UpdateModel::RevisionRole).toUInt(), app->revision());
        QCOMPARE(m_model->data(idx, UpdateModel::TitleRole).toString(), app->title());
        QCOMPARE(m_model->data(idx, UpdateModel::DownloadHashRole).toString(), app->downloadHash());
        QCOMPARE(m_model->data(idx, UpdateModel::SizeRole).toUInt(), app->binaryFilesize());
        QCOMPARE(m_model->data(idx, UpdateModel::IconUrlRole).toString(), app->iconUrl());
        QCOMPARE(m_model->data(idx, UpdateModel::DownloadUrlRole).toString(), app->downloadUrl());
        QCOMPARE(m_model->data(idx, UpdateModel::CommandRole).toStringList(), app->command());
        QCOMPARE(m_model->data(idx, UpdateModel::ChangelogRole).toString(), app->changelog());
        QCOMPARE(m_model->data(idx, UpdateModel::InstalledRole).toBool(), false);
        QCOMPARE(m_model->data(idx, UpdateModel::AutomaticRole).toBool(), app->automatic());
        QCOMPARE(m_model->data(idx, UpdateModel::ErrorRole).toString(), QString(""));
        QCOMPARE(m_model->data(idx, UpdateModel::ProgressRole).toInt(), 50);

        // Verify that the date ain't empty.
        QVERIFY(!m_model->data(idx, UpdateModel::CreatedAtRole).toString().isEmpty());
    }
    void testFilter_data()
    {
        QTest::addColumn<uint>("filter");
        QTest::newRow("Pending") << (uint) UpdateModel::Filter::Pending;
        QTest::newRow("PendingClicks") << (uint) UpdateModel::Filter::PendingClicks;
        QTest::newRow("PendingImage") << (uint) UpdateModel::Filter::PendingImage;
        QTest::newRow("InstalledClicks") << (uint) UpdateModel::Filter::InstalledClicks;
        QTest::newRow("PendingImage") << (uint) UpdateModel::Filter::PendingImage;
        QTest::newRow("Installed") << (uint) UpdateModel::Filter::Installed;
    }
    void testFilter()
    {
        QFETCH(uint, filter);

        QSignalSpy filterChangedSpy(m_model, SIGNAL(filterChanged()));
        m_model->setFilter((UpdateModel::Filter) filter);
        QTRY_COMPARE(filterChangedSpy.count(), 1);
        QCOMPARE((uint) m_model->filter(), filter);
    }
    void testManualImageUpdate()
    {
        m_model->setImageUpdate("ubuntu", "350", 400000, 0);
        m_model->setFilter(UpdateModel::Filter::PendingImage);
        QCOMPARE(m_model->count(), 1);
        QCOMPARE(m_model->data(
            m_model->index(0), UpdateModel::IdRole
        ).toString(), QString("ubuntu"));
        QCOMPARE(m_model->data(
            m_model->index(0), UpdateModel::RevisionRole
        ).toUInt(), (uint) 350);
        QCOMPARE(m_model->data(
            m_model->index(0), UpdateModel::AutomaticRole
        ).toBool(), false);

    }
    void testAutomaticImageUpdate()
    {
        m_model->setImageUpdate("ubuntu", "350", 400000, 1);
        m_model->setFilter(UpdateModel::Filter::PendingImage);
        QCOMPARE(m_model->count(), 1);
        QCOMPARE(m_model->data(
            m_model->index(0), UpdateModel::IdRole
        ).toString(), QString("ubuntu"));
        QCOMPARE(m_model->data(
            m_model->index(0), UpdateModel::RevisionRole
        ).toUInt(), (uint) 350);
        QCOMPARE(m_model->data(
            m_model->index(0), UpdateModel::AutomaticRole
        ).toBool(), true);
    }
private:
    UpdateDb *m_db;
    UpdateModel *m_model;
};

QTEST_MAIN(TstUpdateModel)

#include "tst_updatemodel.moc"
