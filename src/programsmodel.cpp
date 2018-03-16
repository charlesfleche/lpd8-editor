#include "programsmodel.h"

#include "db.h"
#include "lpd8_sysex.h"

#include <QDataStream>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QSqlError>
#include <QSqlField>
#include <QSqlRecord>
#include <QSqlQuery>

#include <QtDebug>

static const QString cc_field_name("cc");
static const QString channel_field_name("channel");
static const QString control_id_field_name("controlId");
static const QString high_field_name("high");
static const QString low_field_name("low");
static const QString name_field_name("name");
static const QString note_field_name("note");
static const QString pc_field_name("pc");
static const QString program_id_field_name("programId");
static const QString toggle_field_name("toggle");

// XXX those are kinda nasty, and not data driven...

static const int control_type_pad = 0;
static const int control_type_knob = 1;
static const int program_id_column_index = 0;
static const int program_pads_count = 8;
static const int program_knobs_count = 8;


QString programIdFilter(int projectId) {
    return QString("%1='%2'").arg(program_id_field_name).arg(projectId);
}

ProgramsModel::ProgramsModel(QObject *parent) :
    QAbstractItemModel(parent),
    m_groups(Q_NULLPTR),
    m_pads(Q_NULLPTR),
    m_knobs(Q_NULLPTR),
    m_programs(Q_NULLPTR),
    m_empty(Q_NULLPTR)
{
    m_empty = new QStandardItemModel(this);

    m_knobs = new QSqlTableModel(this);
    m_knobs->setEditStrategy(QSqlTableModel::OnFieldChange);
    m_knobs->setTable("knobs");

    m_pads = new QSqlTableModel(this);
    m_pads->setEditStrategy(QSqlTableModel::OnFieldChange);
    m_pads->setTable("pads");

    m_groups = new QStandardItemModel(this);
    m_groups->setHorizontalHeaderLabels({program_id_field_name, name_field_name});

    m_programs = new QSqlTableModel(this);
    m_programs->setTable("programs");
    m_programs->setEditStrategy(QSqlTableModel::OnFieldChange);

    refresh();

    qDebug() << "this" << this;
    qDebug() << "m_programs" << m_programs;
    qDebug() << "m_groups" << m_groups;
    for (auto it = m_groups_proxies.keyBegin() ; it != m_groups_proxies.keyEnd() ; ++it) {
        QSortFilterProxyModel* m = m_groups_proxies[*it];
        qDebug() << "m_groups_proxies" << *it << m << m->rowCount() ;
    }
    for (auto it = m_pads_proxies.keyBegin() ; it != m_pads_proxies.keyEnd() ; ++it) {
        QSortFilterProxyModel* m = m_pads_proxies[*it];
        qDebug() << "m_pads_proxies" << *it << m << m->rowCount();
    }
    for (auto it = m_knobs_proxies.keyBegin() ; it != m_knobs_proxies.keyEnd() ; ++it) {
        QSortFilterProxyModel* m = m_knobs_proxies[*it];
        qDebug() << "m_knobs_proxies" << *it << m << m->rowCount();
    }
    qDebug() << "m_empty" << m_empty;

    connect(
        m_programs,
        &QSqlTableModel::modelAboutToBeReset,
        this,
        &ProgramsModel::modelAboutToBeReset
    );

    connect(
        m_programs,
        &QSqlTableModel::modelReset,
        this,
        &ProgramsModel::modelReset
    );

    QSqlDriver *driver = QSqlDatabase::database().driver();
    Q_CHECK_PTR(driver);

    driver->subscribeToNotification("programs");
    connect(
        driver,
        QOverload<const QString&>::of(&QSqlDriver::notification),
        this,
        &ProgramsModel::refresh
    );
}

void ProgramsModel::refresh() {
    Q_CHECK_PTR(m_programs);
    Q_CHECK_PTR(m_pads);
    Q_CHECK_PTR(m_knobs);

    const QSet<int> ids(QSet<int>::fromList(programIds()));
    for (auto it = ids.begin() ; it != ids.end() ; ++it) {
        const int id = *it;
        if (!m_groups_proxies.contains(id)) {
            addFilters(id);
        }
    }

    const QSet<int> toDeleteIds(QSet<int>::fromList(m_groups_proxies.keys()).subtract(ids));
    for (auto it = toDeleteIds.begin() ; it != toDeleteIds.end() ; ++it) {
        const int id = *it;
        removeFilters(id);
    }

    m_knobs->select();
    m_pads->select();

#ifndef QT_NO_DEBUG
    Q_ASSERT(m_groups_proxies.count() == ids.count());
    Q_ASSERT(m_pads_proxies.count() == ids.count());
    Q_ASSERT(m_knobs_proxies.count() == ids.count());

    for (auto it = ids.begin() ; it != ids.end() ; ++it) {
        const int id = *it;
        Q_ASSERT(m_groups_proxies.contains(id));
        Q_ASSERT(m_pads_proxies.contains(id));
        Q_ASSERT(m_knobs_proxies.contains(id));
        Q_ASSERT(m_groups_proxies[id]->rowCount() == 2);
        Q_ASSERT(m_pads_proxies[id]->rowCount() == sysex::padsCount());
        Q_ASSERT(m_knobs_proxies[id]->rowCount() == sysex::knobsCount());
    }
#endif

    m_programs->select();
}

const QAbstractItemModel* ProgramsModel::modelFromParent(const QModelIndex &parent) const {
    QModelIndex idx = parent;
    int lvl = 0;
    while (idx.isValid()) {
        ++lvl;
        idx = idx.parent();
    }

    // parent is the top level invalid index

    if (lvl == 0) {
        return m_programs;
    }

    // Only first column can have children

    if (parent.column() != 0) {
        return m_empty;
    }

    const int programId = parent.data().toInt();

    // parent is in m_programs: returns a group proxy

    if (lvl == 1) {
        Q_ASSERT(m_groups_proxies.contains(programId));
        return m_groups_proxies[programId];
    }

    // parent is a group proxy

    if (lvl == 2) {
        switch (parent.row()) {
        case control_type_pad:
            Q_ASSERT(m_pads_proxies.contains(programId));
            return m_pads_proxies[programId];
        case control_type_knob:
            Q_ASSERT(m_knobs_proxies.contains(programId));
            return m_knobs_proxies[programId];
        default:
            break;
        }
    }

    return m_empty;
}

int ProgramsModel::columnCount(const QModelIndex &) const {
    Q_CHECK_PTR(m_programs);
    Q_CHECK_PTR(m_groups);
    Q_CHECK_PTR(m_pads);
    Q_CHECK_PTR(m_knobs);

    return std::max({
        m_programs->columnCount(),
        m_groups->columnCount(),
        m_pads->columnCount(),
        m_knobs->columnCount()
    });
}

int ProgramsModel::rowCount(const QModelIndex &parent) const {
    const QAbstractItemModel* m = modelFromParent(parent);
    Q_CHECK_PTR(m);

    int ret = m->rowCount();
    qDebug() << "rowCount" << parent << m << ret;
    return ret;
}

QVariant ProgramsModel::data(const QModelIndex &index, int role) const {
    const QAbstractItemModel* m = model(index);
    Q_CHECK_PTR(m);

    QModelIndex idx = m->index(index.row(), index.column());
    QVariant ret = m->data(idx, role);

    if (role == Qt::DisplayRole) {
        qDebug() << "data" << index << role << m << ret;
    }
    return ret;
}

QModelIndex ProgramsModel::index(int row, int column, const QModelIndex &parent) const {
    const QAbstractItemModel* m = modelFromParent(parent);
    Q_CHECK_PTR(m);

//    qDebug() << "index" << row << column << parent << m->index(row, column, parent);
    return createIndex(row, column, const_cast<QAbstractItemModel*>(m));
}


QModelIndex ProgramsModel::parent(const QModelIndex &child) const {
    const QAbstractItemModel* m = model(child);
    Q_CHECK_PTR(m);

    QModelIndex ret;

    const QSortFilterProxyModel* const_program_proxy = qobject_cast<const QSortFilterProxyModel*>(m);
    QSortFilterProxyModel* program_proxy = const_cast<QSortFilterProxyModel*>(const_program_proxy);

    // Child is a know ? Return knob row of matching group model

    int programId = m_knobs_proxies.key(program_proxy);
    if (programId > 0) {
        Q_ASSERT(m_groups_proxies.contains(programId));
        ret = createIndex(control_type_knob, 0, m_groups_proxies[programId]);
        goto end;
    }

    // Child is a pad ? Return pad row of matching group model

    programId = m_pads_proxies.key(program_proxy);
    if (programId > 0) {
        Q_ASSERT(m_groups_proxies.contains(programId));
        ret = createIndex(control_type_pad, 0, m_groups_proxies[programId]);
        goto end;
    }

    // Child is group ? Return program row

    programId = m_groups_proxies.key(program_proxy);
    if (programId > 0) {
        QModelIndexList indices = m_programs->match(
            m_programs->index(0, 0),
            Qt::DisplayRole,
            programId
        );
        Q_ASSERT(indices.count() == 1);
        ret = createIndex(indices[0].row(), 0, m_programs);
        goto end;
    }

end:
//    qDebug() << "parent" << child << ret;
    return ret;
}

QAbstractItemModel* ProgramsModel::model(const QModelIndex &index)  {
    return qobject_cast<QAbstractItemModel*>(static_cast<QObject*>(index.internalPointer()));
}

const QAbstractItemModel* ProgramsModel::model(const QModelIndex &index) const {
    return qobject_cast<QAbstractItemModel*>(static_cast<QObject*>(index.internalPointer()));
}

void ProgramsModel::addFilters(int programId) {
    Q_CHECK_PTR(m_groups);
    Q_ASSERT(!m_groups_proxies.contains(programId));

    // Regexes

    const QRegExp regex(QString("^%1$").arg(programId));

    // Groups

    m_groups->appendRow({new QStandardItem(QString::number(programId)), new QStandardItem(QString("pads%1").arg(programId))});
    m_groups->appendRow({new QStandardItem(QString::number(programId)), new QStandardItem(QString("knobs%1").arg(programId))});

    QSortFilterProxyModel* proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(m_groups);
    proxy->setFilterKeyColumn(program_id_column_index);
    proxy->setFilterRegExp(regex);
    m_groups_proxies[programId] = proxy;

    // Pads

    proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(m_pads);
    proxy->setFilterKeyColumn(program_id_column_index);
    proxy->setFilterRegExp(regex);
    m_pads_proxies[programId] = proxy;

    // Knobs

    proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(m_knobs);
    proxy->setFilterKeyColumn(program_id_column_index);
    proxy->setFilterRegExp(regex);
    m_knobs_proxies[programId] = proxy;
}

void ProgramsModel::removeFilters(int programId) {
    Q_CHECK_PTR(m_groups);
    Q_ASSERT(m_groups_proxies.contains(programId));

    const QString findItemsText(QString::number(programId));

    // Groups

    m_groups_proxies.take(programId)->deleteLater();

    QList<QStandardItem*> items = m_groups->findItems(findItemsText);
    Q_ASSERT(items.count() == 2);
    Q_CHECK_PTR(items[0]);
    Q_CHECK_PTR(items[1]);
    Q_ASSERT((items[1]->row() - items[0]->row()) == 1); // The two indices are contiguous
    m_groups->removeRows(items[0]->row(), items.count());

    // Pads

    m_pads_proxies.take(programId)->deleteLater();

    // Knobs

    m_knobs_proxies.take(programId)->deleteLater();
}
