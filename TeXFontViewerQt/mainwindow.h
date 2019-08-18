#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QScrollArea>
#include <QLabel>
#include <QColor>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QString>
#include "drawingarea.h"
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void resizeEvent(QResizeEvent*event) override;
    void keyPressEvent(QKeyEvent* keyevent) override;
    void UpdateStatusbar();
    void InitStatusbar();
    ~MainWindow();
    bool eventFilter(QObject *obj, QEvent *event) override;
private slots:
    void on_actionExit_triggered();

    void on_actionOpen_triggered();

    void on_actionnext_triggered();

    void on_actionprev_triggered();

    void on_actionzoomin_triggered();

    void on_actionzoomout_triggered();

    void on_actionright_triggered();

    void on_actionup_triggered();

    void on_actiondown_triggered();

    void on_actionleft_triggered();

    void on_actionref_triggered();

    void on_actiongrid_triggered();

    void on_actionchardx_triggered();

    void on_actioncharwd_triggered();

    void on_actionAbout_triggered();

    void on_actionbkcolor_triggered();

    void on_actionfgcolor_triggered();

    void dragEnterEvent(QDragEnterEvent *e);

    void dropEvent(QDropEvent* e);

    void do_open_filename(QString the_filename);


private:
    Ui::MainWindow *ui;
    QScrollArea *scrollarea;
    DrawingArea *drawingarea;
    QLabel *labelZoom;
    QLabel *labelCharIndex;
    QLabel *labelWidth;
    QLabel *labelRes;
    QLabel *labelDsgSz;
    QLabel *labelOffset;
    QColor curBkColor;
};

#endif // MAINWINDOW_H
