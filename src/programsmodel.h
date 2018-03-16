#ifndef PROGRAMSMODEL_H
#define PROGRAMSMODEL_H

#include <QRegExp>
#include <QSqlTableModel>

class QAbstractItemModel;
class QSortFilterProxyModel;
class QSqlTableModel;
class QStandardItemModel;

class ProgramsModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    ProgramsModel(QObject* parent = Q_NULLPTR);

    int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    QModelIndex parent(const QModelIndex &child) const Q_DECL_OVERRIDE;
    int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;

private slots:
    void refresh();

private:
    QAbstractItemModel* model(const QModelIndex &idx);
    const QAbstractItemModel* model(const QModelIndex &idx) const;
    const QAbstractItemModel* modelFromParent(const QModelIndex &parent) const;

    void addFilters(int programId);
    void removeFilters(int programId);

    QHash<int, QSortFilterProxyModel*> m_groups_proxies;
    QHash<int, QSortFilterProxyModel*> m_pads_proxies;
    QHash<int, QSortFilterProxyModel*> m_knobs_proxies;

    QStandardItemModel* m_groups;
    QSqlTableModel* m_pads;
    QSqlTableModel* m_knobs;
    QSqlTableModel* m_programs;
    QStandardItemModel* m_empty;
};

#endif // PROGRAMSMODEL_H
