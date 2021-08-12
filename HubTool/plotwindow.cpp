#include "plotwindow.h"
#include "ui_plotwindow.h"
#include <QTime>
#include <QFile>
#include <QTextStream>

PlotWindow::PlotWindow(int port, qint64 appStartTime, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PlotWindow),
    m_port(port)
{
    m_startTimeMs = appStartTime;

    ui->setupUi(this);
}

PlotWindow::~PlotWindow()
{
    delete ui;
}

void PlotWindow::keyPressEvent(QKeyEvent *e) {
    // swallow escape key
    if(e->key() == Qt::Key_Escape)
        return;
    // hide the windows on com-W and alt-F4...
    else if(e->matches(QKeySequence::Close))
        PlotWindow::setVisible(false);
    else
        QDialog::keyPressEvent(e);
}

void PlotWindow::setupVandIplots(int port){
    // ignore signals inteneded for other port's windows
    if(port != m_port)
        return;
    //qDebug() << "setting up plot for port " << m_port;

    this->setWindowTitle(QString("HubTool: Port %1").arg(port));
    ui->titleText->setText(QString("<html><head/><body><p><span style=\" font-weight:600;\">USB Voltage and Current Port %1</span></p></body></html>").arg(port));

    // instead of using two qcustomplot widgets, we will use one widget
    // with two embedded AxisRects in a MarginGroup
    // this allows us to visually align the y-axes and ensure the x-axis
    // are in lock-step
    // http://qcustomplot.com/index.php/demos/advancedaxesdemo

    // create two axes rects (for voltage and current
    QCPAxisRect *voltageAxisRect = new QCPAxisRect(ui->plotWidget);
    QCPAxisRect *currentAxisRect = new QCPAxisRect(ui->plotWidget);
    ui->plotWidget->setToolTip("Scroll to zoom vertical axis.\nDrag to adjust vertical axis.\nDouble click axis to set log/linear scale.");
    ui->plotWidget->plotLayout()->clear();
    ui->plotWidget->plotLayout()->addElement(0, 0, voltageAxisRect);
    ui->plotWidget->plotLayout()->addElement(1, 0, currentAxisRect);
    ui->plotWidget->plotLayout()->setRowStretchFactor(0, 9);
    ui->plotWidget->plotLayout()->setRowStretchFactor(1, 12);

    // synchronize the left and right margins of the top and bottom axis rects:
    QCPMarginGroup *marginGroup = new QCPMarginGroup(ui->plotWidget);
    voltageAxisRect->setMarginGroup(QCP::msLeft | QCP::msRight, marginGroup);
    currentAxisRect->setMarginGroup(QCP::msLeft | QCP::msRight, marginGroup);
    // move newly created axes on "axes" layer and grids on "grid" layer:
    foreach (QCPAxisRect *rect, ui->plotWidget->axisRects())
    {
      foreach (QCPAxis *axis, rect->axes())
      {
        axis->setLayer("axes");
        axis->grid()->setLayer("grid");
      }
    }

    voltageAxisRect->setupFullAxesBox(true);
    currentAxisRect->setupFullAxesBox(true);

    // add the plots and store to member variables
    // these don't need to be member variable since the graphs should be index locked,
    // but it's easier to track when they have pretty names
    m_voltageGraph = ui->plotWidget->addGraph(voltageAxisRect->axis(QCPAxis::atBottom), voltageAxisRect->axis(QCPAxis::atLeft));
    m_currentGraph = ui->plotWidget->addGraph(currentAxisRect->axis(QCPAxis::atBottom), currentAxisRect->axis(QCPAxis::atLeft));
    m_voltageGraph->setPen(QPen(Qt::blue));
    m_currentGraph->setPen(QPen(Qt::red));

    ui->plotWidget->setBackground(this->palette().window().color());
    ui->plotWidget->setInteraction(QCP::iRangeZoom);
    ui->plotWidget->setInteraction(QCP::iRangeDrag);
    voltageAxisRect->setMargins(QMargins(-1,20,-1,0));
    voltageAxisRect->setAutoMargins(QCP::msLeft|QCP::msRight);
    voltageAxisRect->axis(QCPAxis::atBottom)->setTickLabels(false);

    voltageAxisRect->axis(QCPAxis::atLeft)->setRange(4.5,5.55); // a bit higher so 5.5 shows in the axis
    voltageAxisRect->axis(QCPAxis::atLeft)->setNumberFormat("f");
    voltageAxisRect->axis(QCPAxis::atLeft)->setNumberPrecision(2);
    voltageAxisRect->axis(QCPAxis::atLeft)->setLabel("Volts");
    voltageAxisRect->axis(QCPAxis::atLeft)->setLabelPadding(0);
    voltageAxisRect->setRangeZoom(Qt::Orientation::Vertical);
    voltageAxisRect->setRangeDrag(Qt::Orientation::Vertical);

    currentAxisRect->setMargins(QMargins(-1,0,-1,0));
    currentAxisRect->setAutoMargins(QCP::msLeft|QCP::msRight|QCP::msBottom);
    QSharedPointer<QCPAxisTickerTime> currentPlotTimeTicker(new QCPAxisTickerTime);
    currentPlotTimeTicker->setTimeFormat("%h:%m:%s");
    currentAxisRect->axis(QCPAxis::atBottom)->setTicker(currentPlotTimeTicker);
    currentAxisRect->axis(QCPAxis::atBottom)->setLabel("Time");
    currentAxisRect->axis(QCPAxis::atBottom)->setLabelPadding(0);
    currentAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(-45.0);

    currentAxisRect->axis(QCPAxis::atLeft)->setLabel("Amps");
    currentAxisRect->axis(QCPAxis::atLeft)->setLabelPadding(0);
    currentAxisRect->axis(QCPAxis::atLeft)->setRange(0.01, 5.5);
    QSharedPointer<QCPAxisTicker> linearTicker(new QCPAxisTicker);
    linearTicker->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssMeetTickCount);
    linearTicker->setTickCount(4);
    currentAxisRect->axis(QCPAxis::atLeft)->setTicker(linearTicker);
    currentAxisRect->axis(QCPAxis::atLeft)->setScaleType(QCPAxis::stLinear);
    currentAxisRect->axis(QCPAxis::atLeft)->grid()->setSubGridVisible(true);
    currentAxisRect->setRangeZoom(Qt::Orientation::Vertical);
    currentAxisRect->setRangeDrag(Qt::Orientation::Vertical);
    connect(ui->plotWidget,SIGNAL(axisDoubleClick(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)),
            this, SLOT(handleAxisDoubleClicked(QCPAxis*,QCPAxis::SelectablePart,QMouseEvent*)));

    connect(currentAxisRect->axis(QCPAxis::atLeft), SIGNAL(rangeChanged(QCPRange,QCPRange)),
            this, SLOT(handleCurrentAxisRangeChange(QCPRange,QCPRange)));
    connect(voltageAxisRect->axis(QCPAxis::atLeft), SIGNAL(rangeChanged(QCPRange,QCPRange)),
            this, SLOT(handleVoltageAxisRangeChange(QCPRange,QCPRange)));

}

// Always add the voltage and current in pairs so the plots have the same time keys
void PlotWindow::handlePortVoltageCurrent(int port, int32_t microVolts, int32_t microAmps){
    // ignore signals inteneded for other port's windows
    if(port != m_port)
        return;
    double currentTimeKey = (QDateTime::currentMSecsSinceEpoch() - m_startTimeMs)/1000.0;
    m_voltageGraph->addData(currentTimeKey, microVolts/1000000.0);
    m_currentGraph->addData(currentTimeKey, microAmps/1000000.0);
}


void PlotWindow::updatePlots(){
    double currentTimeKey = (QDateTime::currentMSecsSinceEpoch() - m_startTimeMs)/1000.0;
    const double range_size = 32;

    // if we're autoscrolling, truncate the date
    if(ui->logDataCheckBox->checkState() != Qt::Checked){
        m_voltageGraph->data()->removeBefore(currentTimeKey-range_size);
        m_currentGraph->data()->removeBefore(currentTimeKey-range_size);

        if(this->isVisible()) {
            ui->plotWidget->axisRect(0)->axis(QCPAxis::atBottom)->setRange(currentTimeKey+0.25, range_size, Qt::AlignRight);
            ui->plotWidget->axisRect(1)->axis(QCPAxis::atBottom)->setRange(currentTimeKey+0.25, range_size, Qt::AlignRight);
        }
    }
    else {
        if(this->isVisible()) {
            ui->plotWidget->axisRect(0)->axis(QCPAxis::atBottom)->rescale();
            ui->plotWidget->axisRect(1)->axis(QCPAxis::atBottom)->rescale();
        }
    }

    if(this->isVisible()) {
        ui->plotWidget->replot(QCustomPlot::rpQueuedReplot);
        ui->sampleCountLabel->setText(QString("%1 samples").arg(m_voltageGraph->dataCount()));
    }
}

void PlotWindow::handleAxisDoubleClicked(QCPAxis* axis,QCPAxis::SelectablePart,QMouseEvent*){
    QCPAxis* currentGraphVerticalAxis = ui->plotWidget->axisRect(1)->axis(QCPAxis::atLeft);
    if(axis == currentGraphVerticalAxis){
        if(currentGraphVerticalAxis->scaleType() == QCPAxis::stLogarithmic){
            QSharedPointer<QCPAxisTicker> linearTicker(new QCPAxisTicker);
            linearTicker->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssMeetTickCount);
            linearTicker->setTickCount(4);
            currentGraphVerticalAxis->setTicker(linearTicker);
            currentGraphVerticalAxis->setScaleType(QCPAxis::stLinear);
        }
        else{
            QSharedPointer<QCPAxisTickerLog> logTicker(new QCPAxisTickerLog);
            logTicker->setTickStepStrategy(QCPAxisTicker::TickStepStrategy::tssMeetTickCount);
            logTicker->setTickCount(4);
            currentGraphVerticalAxis->setTicker(logTicker);
            currentGraphVerticalAxis->setScaleType(QCPAxis::stLogarithmic);
        }
        QCPRange trash(0,0);
        handleCurrentAxisRangeChange(trash, trash);
    }
    else{
        qDebug()<<"double click not on Current measurement axis";
    }
}

void PlotWindow::handleCurrentAxisRangeChange(QCPRange, QCPRange){
    // if we're using log scale, keep the lower bound just below the 1b resolution of 1mA
    double minLower = ui->plotWidget->axisRect(1)->axis(QCPAxis::atLeft)->scaleType() == QCPAxis::stLogarithmic ?
                0.0005 : -0.0009;
    double minUpper = 0.007;
    double maxUpper = 6.0;
    double minSpan = 0.008;

    // set the current vertical axis limits to something reasonable
    QCPRange axisRange;
    axisRange = ui->plotWidget->axisRect(1)->axis(QCPAxis::atLeft)->range();

    // sanitize the new range
    axisRange.lower = axisRange.lower < minLower ? minLower : axisRange.lower;
    axisRange.upper = axisRange.upper > maxUpper ? maxUpper : axisRange.upper;
    axisRange.upper = axisRange.upper < minUpper ? minUpper : axisRange.upper;
    axisRange.normalize();

    if(axisRange.size() < minSpan){
        axisRange.lower = axisRange.center()-minSpan*.5;
        axisRange.upper = axisRange.center()+minSpan*.5;
    }

    //qDebug() << "using new range: " << minLower << " " << axisRange.lower << " " << axisRange.upper;
    ui->plotWidget->axisRect(1)->axis(QCPAxis::atLeft)->setRange(axisRange);
}

void PlotWindow::handleVoltageAxisRangeChange(QCPRange,QCPRange){
    double minLower = -0.01;
    double maxUpper = 5.55;
    double minSpan = 0.1;

    // set the voltage vertical axis limits to something reasonable
    QCPRange axisRange;
    axisRange = ui->plotWidget->axisRect(0)->axis(QCPAxis::atLeft)->range();
    axisRange.lower = axisRange.lower < minLower ? minLower : axisRange.lower;
    axisRange.upper = axisRange.upper > maxUpper ? maxUpper : axisRange.upper;
    axisRange.normalize();

    if(axisRange.size() < minSpan){
        axisRange.lower = axisRange.center()-minSpan*.5;
        axisRange.upper = axisRange.center()+minSpan*.5;
    }

    ui->plotWidget->axisRect(0)->axis(QCPAxis::atLeft)->setRange(axisRange);
}

void PlotWindow::on_saveCsvButton_clicked()
{
    QString csvFileName = QFileDialog::getSaveFileName(this, tr("Save file as:"),
                                                       QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                                       "Comma Separated Value (*.csv);;All files (*.*)" );
    //QMessageBox::information(this, tr("File name is"), csvFileName);

    QFile csvFile(csvFileName);

    if(csvFileName.isEmpty()){
        // no file was spec'd or the user canceled the save so bail
        return;
    }

    csvFile.open(QIODevice::WriteOnly | QIODevice::Text);
    if(!csvFile.isOpen()){
        QMessageBox::information(this, tr("Error"), QString("Couldn't open %1").arg(csvFileName));
        return;
    }

    QTextStream csvFileStream(&csvFile);

    // write the header
    csvFileStream << "Time (s)," \
                  << "Port " << QString::number(m_port) \
                  << " Voltage (V),"
                  << "Port " << QString::number(m_port) \
                  << " Current (A)" << endl;

    // write the data
    for(int i=0; i < m_voltageGraph->dataCount(); i++){
        //fprintf(csvFile, "%.3f,%.6f,%.6f\r\n");
        csvFileStream << QString::number(m_voltageGraph->data()->at(i)->key, 'f', 3) << ",";
        csvFileStream << QString::number(m_voltageGraph->data()->at(i)->value, 'f', 3) << ",";
        csvFileStream << QString::number(m_currentGraph->data()->at(i)->value, 'f', 3) << endl;
    }

    // close the file
    csvFileStream.flush();
    csvFile.close();

}
