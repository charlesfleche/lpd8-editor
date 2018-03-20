#ifndef MIDIVALUEDELEGATE_H
#define MIDIVALUEDELEGATE_H

#include <QSpinBox>
#include <QStyledItemDelegate>


class MidiValueDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    MidiValueDelegate(QObject* parent = Q_NULLPTR);

    void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const Q_DECL_OVERRIDE;
    QWidget* createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const Q_DECL_OVERRIDE;
    void setEditorData(QWidget*, const QModelIndex&) const Q_DECL_OVERRIDE;
    void setModelData(QWidget*, QAbstractItemModel* , const QModelIndex&) const Q_DECL_OVERRIDE;
    void updateEditorGeometry(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const Q_DECL_OVERRIDE;

    bool editorEvent(QEvent*, QAbstractItemModel*, const QStyleOptionViewItem&, const QModelIndex&) Q_DECL_OVERRIDE;
};


class LutSpinBox : public QSpinBox {
    Q_OBJECT

public:
    LutSpinBox(const QStringList& lut, QWidget *parent = Q_NULLPTR);

protected:
    QString textFromValue(int value) const Q_DECL_OVERRIDE;
    int valueFromText(const QString &text) const Q_DECL_OVERRIDE;

private:
    const QStringList m_lut;
};

#endif // MIDIVALUEDELEGATE_H
