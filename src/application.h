#ifndef APPLICATION_H
#define APPLICATION_H

#include <QObject>

class ProgramsModel;

class QAbstractItemModel;
class QSqlTableModel;

class Application : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int activeProgramId READ activeProgramId WRITE setActiveProgramId NOTIFY activeProgramIdChanged)

public:
    explicit Application(QObject *parent = 0);

    int activeProgramId() const;

    bool isValidActiveProgram() const;

    QAbstractItemModel* programs() const;
    ProgramsModel* myPrograms() const;

public slots:
    void setActiveProgramId(int);
    void setActiveProgramChannel(int);

signals:
    void activeProgramIdChanged(int);
    void activeProgramChannelChanged(int);
    void connectedChanged(bool);

private:
    ProgramsModel* m_my_programs;
};

#endif // APPLICATION_H
