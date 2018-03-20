#include "programsmodel.h"

#include "db.h"
#include "enums.h"
#include "lpd8_sysex.h"

#include <QSortFilterProxyModel>
#include <QStandardItemModel>

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

static const QString pads_group_name = "pads";
static const QString knobs_group_name = "knobs";

static const QString momentary_label = "Momentary";
static const QString toggle_label = "Toggle";


// XXX those are kinda nasty, and not data driven...

static const int control_type_pad = 0;
static const int control_type_knob = 1;
static const int program_id_column_index = 0;


ProgramsModel::ProgramsModel(QObject *parent) :
    QAbstractItemModel(parent),
    m_groups(Q_NULLPTR),
    m_pads(Q_NULLPTR),
    m_knobs(Q_NULLPTR),
    m_programs(Q_NULLPTR),
    m_empty(Q_NULLPTR)
{
    // LUTs

    for (int i = 0 ; i <= 127 ; ++i) {
        m_lut_default << QString::number(i);
    }

    for (int i = 0 ; i <= 15 ; ++i) {
        m_lut_channel<< QString::number(i+1);
    }

    m_lut_toggle << momentary_label;
    m_lut_toggle << toggle_label;

    m_luts[channel_field_name] = m_lut_channel;
    m_luts[note_field_name] = m_lut_default;
    m_luts[pc_field_name] = m_lut_default;
    m_luts[cc_field_name] = m_lut_default;
    m_luts[toggle_field_name] = m_lut_toggle;
    m_luts[low_field_name] = m_lut_default;
    m_luts[high_field_name] = m_lut_default;

    // Models

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

    refresh();
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

    return m->rowCount();
}

QVariant ProgramsModel::data(const QModelIndex &index, int role) const {
    const QAbstractItemModel* m = model(index);
    Q_CHECK_PTR(m);

    if (hasLut(index)) {
        switch (role) {
        case Qt::DisplayRole:
            return lut(index)[index.data(Qt::EditRole).toInt()];
        case MidiDataRole::MidiValues:
            return lut(index);
        default:
            break;
        }
    }

    return m->data(m->index(index.row(), index.column()), role);
}

bool ProgramsModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    Q_ASSERT(index.isValid());

    QAbstractItemModel* m = model(index);
    Q_CHECK_PTR(m);

    return m->setData(m->index(index.row(), index.column()), value, role);
}

Qt::ItemFlags ProgramsModel::flags(const QModelIndex &index) const {
    Q_ASSERT(index.isValid());

    const QAbstractItemModel* m = model(index);
    Q_CHECK_PTR(m);

    Qt::ItemFlags flags = m->flags(m->index(index.row(), index.column()));

    // QSqlTableModel are Qt::ItemNeverHasChildren by default

    const QSortFilterProxyModel *mp = qobject_cast<const QSortFilterProxyModel *>(m);
    QSortFilterProxyModel *cmp = const_cast<QSortFilterProxyModel*>(mp);
    const bool indexIsInLastChildTables = m_pads_proxies.key(cmp) > 0 || m_knobs_proxies.key(cmp) > 0;
    if (!indexIsInLastChildTables) {
        flags &= ~Qt::ItemNeverHasChildren;
    }

    // Do not allow edition of programIds, controlIds and grouping items

    const QString columnName = m->headerData(index.column(), Qt::Horizontal).toString();
    if (columnName == program_id_field_name || columnName == control_id_field_name) {
        flags &= ~Qt::ItemIsEditable;
    }

    return flags;
}

QModelIndex ProgramsModel::index(int row, int column, const QModelIndex &parent) const {
    const QAbstractItemModel* m = modelFromParent(parent);
    Q_CHECK_PTR(m);

    return createIndex(row, column, const_cast<QAbstractItemModel*>(m));
}

QModelIndex ProgramsModel::parent(const QModelIndex &child) const {
    const QAbstractItemModel* m = model(child);
    Q_CHECK_PTR(m);

    const QSortFilterProxyModel* const_program_proxy = qobject_cast<const QSortFilterProxyModel*>(m);
    QSortFilterProxyModel* program_proxy = const_cast<QSortFilterProxyModel*>(const_program_proxy);

    // Child is a knob ? Return knob row of matching group model

    int programId = m_knobs_proxies.key(program_proxy);
    if (programId > 0) {
        Q_ASSERT(m_groups_proxies.contains(programId));
        return createIndex(control_type_knob, 0, m_groups_proxies[programId]);
    }

    // Child is a pad ? Return pad row of matching group model

    programId = m_pads_proxies.key(program_proxy);
    if (programId > 0) {
        Q_ASSERT(m_groups_proxies.contains(programId));
        return createIndex(control_type_pad, 0, m_groups_proxies[programId]);
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
        return createIndex(indices[0].row(), 0, m_programs);
    }

    // Child a program ? Return the root (invalid) index

    Q_ASSERT(m == m_programs);
    return QModelIndex();
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

    QStandardItem *programIdItem = new QStandardItem(QString::number(programId));
    QStandardItem *nameItem = new QStandardItem(pads_group_name);
    programIdItem->setEditable(false);
    nameItem->setEditable(false);
    m_groups->appendRow({programIdItem, nameItem});

    programIdItem = new QStandardItem(QString::number(programId));
    nameItem = new QStandardItem(knobs_group_name);
    programIdItem->setEditable(false);
    nameItem->setEditable(false);
    m_groups->appendRow({programIdItem, nameItem});

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

bool ProgramsModel::hasLut(const QModelIndex &index) const {
    const QAbstractItemModel* m = model(index);
    Q_CHECK_PTR(m);

    const QString name = m->headerData(index.column(), Qt::Horizontal).toString();
    return m_luts.contains(name);
}

QStringList ProgramsModel::lut(const QModelIndex & index) const {
    Q_ASSERT(hasLut(index));

    const QAbstractItemModel* m = model(index);
    Q_CHECK_PTR(m);

    const QString name = m->headerData(index.column(), Qt::Horizontal).toString();
    return m_luts[name];
}
