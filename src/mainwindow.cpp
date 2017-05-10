#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "application.h"

#include <QtDebug>

MainWindow::MainWindow(Application* app, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    app(app)
{
    Q_CHECK_PTR(app);

    ui->setupUi(this);

    ui->programsView->setModel(app->programs());
    ui->programsView->setModelColumn(programModelColumn());
    ui->padsView->setModel(app->pads());
    ui->knobsView->setModel(app->knobs());
}

MainWindow::~MainWindow()
{
    delete ui;
}

int MainWindow::programModelColumn() const
{
    return 1;
}

void MainWindow::on_actionNewProgram_triggered()
{
    Q_CHECK_PTR(app);

    app->newProgram("New program");
}

void MainWindow::on_programsView_clicked(const QModelIndex& idx)
{
    Q_ASSERT(idx.isValid());
    Q_CHECK_PTR(idx.model());
    Q_CHECK_PTR(app);

    // Because 0 is the programId column
    // This kind of int finding based on the name should be extracted
    // to another funtion
    const QModelIndex programIdIndex = idx.model()->index(idx.row(), 0);
    const int programId = idx.model()->data(programIdIndex).toInt();

    app->setActiveProgramId(programId);
}
