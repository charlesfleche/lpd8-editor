#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class Application;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(Application*, QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_actionNewProgram_triggered();

    void on_programsView_clicked(const QModelIndex&);

private:
    int programModelColumn() const;

    Ui::MainWindow *ui;
    Application* app;
};

#endif // MAINWINDOW_H
