#ifndef PLOTWINDOW_H
#define PLOTWINDOW_H

#include <QDialog>
#include "stemworker.h"
#include "qcustomplot.h"


namespace Ui {
class PlotWindow;
}

class PlotWindow : public QDialog
{
    Q_OBJECT

public slots:
    void handlePortVoltageCurrent(int channel, int32_t microVolts, int32_t microAmps);
    void updatePlots();
    void handleAxisDoubleClicked(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*);
    void handleCurrentAxisRangeChange(QCPRange,QCPRange);
    void handleVoltageAxisRangeChange(QCPRange,QCPRange);

public:
    explicit PlotWindow(int port, qint64 appStartTime, QWidget *parent = nullptr);
    void setupVandIplots(int port);
    ~PlotWindow();

protected:
    void keyPressEvent(QKeyEvent *e);

private slots:
    void on_saveCsvButton_clicked();

private:
    Ui::PlotWindow *ui;
    int m_port;
    QCPGraph *m_voltageGraph;
    QCPGraph *m_currentGraph;
    QFileDialog *m_fileSaveDialog;
    qint64 m_startTimeMs;
};

#endif // PLOTWINDOW_H
