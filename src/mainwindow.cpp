#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "application.h"
#include "db.h"
#include "commands.h"
#include "iomidi.h"
#include "midiio.h"
#include "midiconnectionsmodel.h"
#include "midivaluedelegate.h"
#include "programsmodel.h"
#include "utils.h"

#include <QComboBox>
#include <QDataWidgetMapper>
#include <QFileDialog>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStandardItemModel>
#include <QUndoStack>

#include <QtDebug>

static const QString SETTINGS_KEY_DEFAULT_NAME = "default/name";
static const QString SETTINGS_KEY_DEFAULT_SYSEX = "default/sysex";

MainWindow::MainWindow(Application* app, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    app(app)
{
    Q_CHECK_PTR(app);
    Q_CHECK_PTR(app->midiIO());

    IOMidi* io = new IOMidi(this);

    ui->setupUi(this);
    setStatusBar(Q_NULLPTR);

    auto connectionsModel = new MidiConnectionsModel(io);

    connect(
        ui->connectionsView,
        &QListView::activated,
        connectionsModel,
        QOverload<const QModelIndex &>::of(&MidiConnectionsModel::connectPort)
    );
    ui->connectionsView->setModel(connectionsModel);

    // Undo

    QUndoStack* stack = undoStack();
    Q_CHECK_PTR(stack);
    ui->undoListView->setStack(stack);

    QAction* undoAction = stack->createUndoAction(this, "&Undo");
    undoAction->setShortcuts(QKeySequence::Undo);

    QAction* redoAction = stack->createRedoAction(this, tr("&Redo"));
    redoAction->setShortcuts(QKeySequence::Redo);

    ui->menuEdit->insertAction(ui->actionNewProgram, redoAction);
    ui->menuEdit->insertAction(redoAction, undoAction);
    ui->menuEdit->insertSeparator(ui->actionNewProgram);

    // Client selection combo box

    QComboBox* clientComboBox = new QComboBox(this);
    clientComboBox->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    ui->toolBar->addWidget(clientComboBox);
    ui->toolBar->addAction(ui->actionRescan);

    ui->newProgramButton->setDefaultAction(ui->actionNewProgram);
    ui->deleteProgramButton->setDefaultAction(ui->actionDeleteProgram);

    connect(app,
            &Application::activeProgramIdChanged,
            [=](int) {
                refreshWidgetStack();
            });

    ui->programsView->setModel(app->programs());
    ui->programsView->setModelColumn(programModelColumn());

    QDataWidgetMapper *mapper = new QDataWidgetMapper(this);
    mapper->setModel(app->programs());
    mapper->setItemDelegate(new MidiValueDelegate(this));
    mapper->addMapping(ui->channelSpinBox, 2); // XXX

    ui->padsView->setItemDelegate(new MidiValueDelegate(this));
    ui->padsView->setModel(app->programs());
    ui->padsView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    QHeaderView* headerView = new QHeaderView(Qt::Horizontal, this);
    headerView->setModel(app->myPrograms()->padsHeaderModel());
    ui->padsView->setHorizontalHeader(headerView);

    ui->padsView->hideColumn(0);
    ui->padsView->hideColumn(1);

    ui->knobsView->setItemDelegate(new MidiValueDelegate(this));
    ui->knobsView->setModel(app->programs());
    ui->knobsView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    headerView = new QHeaderView(Qt::Horizontal, this);
    headerView->setModel(app->myPrograms()->knobsHeaderModel());
    ui->knobsView->setHorizontalHeader(headerView);

    ui->knobsView->hideColumn(0);
    ui->knobsView->hideColumn(1);

    // Refresh action delete program

    QItemSelectionModel *sel = ui->programsView->selectionModel();
    Q_CHECK_PTR(sel);
    connect(sel,
            &QItemSelectionModel::selectionChanged,
            this,
            &MainWindow::refreshUiAccordingToSelection
    );
    connect(sel,
            &QItemSelectionModel::currentRowChanged,
            mapper,
            &QDataWidgetMapper::setCurrentModelIndex);

    connect(sel,
            &QItemSelectionModel::currentRowChanged,
            [=](const QModelIndex &current) {
                Q_CHECK_PTR(app->myPrograms());

                ui->padsView->setRootIndex(app->myPrograms()->padsParentIndex(current));
                ui->knobsView->setRootIndex(app->myPrograms()->knobsParentIndex(current));
            }
    );

    connect(app->programs(),
            &QAbstractItemModel::modelReset,
            this,
            &MainWindow::refreshUiAccordingToSelection
    );

    refreshUiAccordingToSelection();
    refreshWidgetStack();

    clientComboBox->setModel(connectionsModel);
    Q_ASSERT(clientComboBox->count() > 0);
    clientComboBox->setEnabled(connectionsModel->connectedPort().isValid());
    connect(connectionsModel,
            &MidiConnectionsModel::connectedPortChanged,
            [=](const QModelIndex &index) {
                clientComboBox->setEnabled(index.isValid());
                ui->actionRescan->setEnabled(index.isValid());
                if (index.isValid()) {
                    clientComboBox->setCurrentIndex(index.row());
                }
            });

    connect(
        clientComboBox,
        QOverload<int>::of(&QComboBox::activated),
        [=](int row) {
            connectionsModel->connectPort(connectionsModel->index(row));
        }
    );

    ui->treeView->setModel(app->programs());
    ui->treeView->setItemDelegate(new MidiValueDelegate(this));

    connect(app->midiIO(),
            &OldMidiIO::programReceived,
            [=](const QByteArray& sysex) {
                Q_CHECK_PTR(ui->programsView->selectionModel());
                Q_CHECK_PTR(app->myPrograms());
                Q_CHECK_PTR(undoStack());

                const int programId = currentSelectedProjectId();
                if (programId == -1) {
                    Q_UNIMPLEMENTED();
                } else {
                    UpdateProgramFromSysexCommand* cmd = new UpdateProgramFromSysexCommand(
                        app->myPrograms(),
                        programId,
                        sysex
                    );
                    undoStack()->push(cmd);
                }
            });

    ProgramIdSelectionRestorer *restorer = new ProgramIdSelectionRestorer(ui->programsView->selectionModel(), this);
    connect(app->myPrograms(),
            &ProgramsModel::modelAboutToBeReset,
            restorer,
            &ProgramIdSelectionRestorer::store);
    connect(app->myPrograms(),
            &ProgramsModel::modelReset,
            restorer,
            &ProgramIdSelectionRestorer::restore);

    // Enable MIDI send / receive action when connected

    QList<QAction *> midiSendActions = {
        ui->actionGetProgram1,
        ui->actionGetProgram2,
        ui->actionGetProgram3,
        ui->actionGetProgram4
    };
    for (int i = 0 ; i < midiSendActions.count() ; ++i) {
        connect(
            midiSendActions[i],
            &QAction::triggered,
            [=]() {
            Q_CHECK_PTR(app->midiIO());

            app->midiIO()->getProgram(i+1);
            }
        );
    }

    QList<QAction *> midiReceiveActions {
        ui->actionSendToProgram1,
        ui->actionSendToProgram2,
        ui->actionSendToProgram3,
        ui->actionSendToProgram4,
    };
    for (int i = 0 ; i < midiReceiveActions.count() ; ++i) {
        connect(
            midiReceiveActions[i],
            &QAction::triggered,
            [=]() {
                this->sendCurrentProgram(i+1);
            }
        );
    }

    QList<QAction *> midiActions = midiSendActions + midiReceiveActions;
    auto midiActionsSetEnabled = [=](){
        for (auto it = midiActions.begin() ; it != midiActions.end() ; ++it) {
            QAction* a = *it;
            Q_CHECK_PTR(a);

            a->setEnabled(connectionsModel->connected() &&
                          ui->programsView->selectionModel()->currentIndex().row() != -1);
        }
    };
    midiActionsSetEnabled();

    connect(
        connectionsModel,
        &MidiConnectionsModel::connectedChanged,
        midiActionsSetEnabled
    );
    connect(
        ui->programsView->selectionModel(),
        &QItemSelectionModel::currentChanged,
        midiActionsSetEnabled
    );
    connect(
        app->myPrograms(),
        &ProgramsModel::modelReset,
        midiActionsSetEnabled
    );
}

MainWindow::~MainWindow()
{
    delete ui;
}

int MainWindow::programModelColumn() const
{
    return 1;
}

void MainWindow::refreshWidgetStack() {
    Q_CHECK_PTR(app);

    QWidget* w = app->activeProgramId() > 0 ? ui->pageEditor : ui->pageDefault;
    ui->stackedWidget->setCurrentWidget(w);
}

QString defaultSysex() {
    return readTextFile(":/default-sysex.sql");
}

void MainWindow::on_actionNewProgram_triggered()
{
    Q_CHECK_PTR(undoStack());

    QUndoCommand* cmd = new CreateProgramCommand(
        app->myPrograms(),
        QSettings().value(SETTINGS_KEY_DEFAULT_NAME).toString(),
        QSettings().value(SETTINGS_KEY_DEFAULT_SYSEX).toByteArray()
    );
    undoStack()->push(cmd);
}

void MainWindow::on_actionDeleteProgram_triggered()
{
    Q_CHECK_PTR(undoStack());

    QItemSelectionModel* selection_model = ui->programsView->selectionModel();
    Q_CHECK_PTR(selection_model);
    Q_ASSERT(selection_model->hasSelection());

    QUndoCommand *cmd = new DeleteProgramCommand(
        app->myPrograms(),
        selectedProgramId(selection_model)
    );
    undoStack()->push(cmd);
}

void MainWindow::on_actionQuit_triggered()
{
    qApp->quit();
}

void MainWindow::on_programsView_activated(const QModelIndex& idx)
{
    Q_ASSERT(idx.isValid());
    Q_CHECK_PTR(idx.model());
    Q_CHECK_PTR(app);

    app->setActiveProgramId(getProgramId(idx.model(), idx.row()));
}

void MainWindow::refreshUiAccordingToSelection()
{
    QItemSelectionModel *sel = ui->programsView->selectionModel();
    Q_CHECK_PTR(sel);

    ui->actionDeleteProgram->setEnabled(sel->hasSelection());

    Q_CHECK_PTR(app);
    QWidget* w = sel->currentIndex().row() != -1 ? ui->pageEditor : ui->pageDefault;
    ui->stackedWidget->setCurrentWidget(w);
}

void MainWindow::on_actionImportProgram_triggered()
{
    Q_CHECK_PTR(undoStack());

    const QString path(
                QFileDialog::getOpenFileName(
                    this,
                    "Import LPD8 program",
                    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));
    if (path.isEmpty()) {
        return;
    }

    QUndoCommand* cmd = new CreateProgramCommand(
        app->myPrograms(),
        QDir(path).dirName(),
        fromSysexTextFile(path)
    );
    undoStack()->push(cmd);
}

void MainWindow::on_actionExportProgram_triggered()
{
    Q_CHECK_PTR(app);

    QItemSelectionModel *m = ui->programsView->selectionModel();
    Q_CHECK_PTR(m);

    const int programId = selectedProgramId(m);
    const QString name = programName(programId);
    const QByteArray sysex = programSysex(programId);

    const QDir defaultDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    const QString defaultPath = defaultDir.absoluteFilePath(name);

    const QString path(
                QFileDialog::getSaveFileName(
                    this,
                    "Export LPD8 program",
                    defaultPath));
    if (path.isEmpty()) {
        return;
    }

    writeProgramFile(sysex, path);
}

void MainWindow::sendCurrentProgram(int deviceProgramId) {
    Q_CHECK_PTR(app->midiIO());
    Q_CHECK_PTR(app->myPrograms());

    const QByteArray sysex = app->myPrograms()->programSysex(currentSelectedProjectId());
    app->midiIO()->sendProgramSysex(sysex, deviceProgramId);
}

void MainWindow::on_actionRescan_triggered() {
    Q_CHECK_PTR(app->midiIO());

    app->midiIO()->rescanPorts();
}

int MainWindow::currentSelectedProjectId() const {
    Q_CHECK_PTR(ui->programsView->selectionModel());

    return selectedProgramId(ui->programsView->selectionModel());
}
