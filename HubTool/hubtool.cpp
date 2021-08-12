#include "hubtool.h"
#include "ui_hubtool.h"
#include <string>
#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <QInputDialog>
#include <QMessageBox>
#include <QTime>
#include <QDebug>
#include <QProcess>


#define aVERSION_UNPACK_MAJOR(pack) ((pack) >> 28)
#define aVERSION_UNPACK_MINOR(pack) ((pack & 0xF000000) >> 24)
#define aVERSION_UNPACK_PATCH(pack) ((pack) & 0xFFFFFF)

#define updateRateAvgLength 5
unsigned int pollingDelay = 20;
#define plotUpdateDelay 100

HubTool::HubTool(linkSpec* spec, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::HubTool)
{
    // Setup the user interface.
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/HubTool.icns"));

    m_startTimeMs = QDateTime(QDateTime::currentDateTime()).toMSecsSinceEpoch();

    // Initialize the application (mostly gui stuff).
    init();

    // Kick off a worker thread to poll the stem
    stemWorker = new StemWorker(spec);
    stemWorker->connect(&stemWorkerThread, SIGNAL(started()), SLOT(start()));
    stemWorker->moveToThread(&stemWorkerThread);

    // register standard types for slots/signals
    qRegisterMetaType<int32_t>("int32_t");
    qRegisterMetaType<uint32_t>("uint32_t");
    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<int8_t>("int8_t");

    // ////////////////////////////////////////////////////
    // connect stem worker signals to UI handling slots
    connect (stemWorker, SIGNAL(resultReady(int, bool)),
            this, SLOT(handleResults(int, bool)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(logStringReady(QString)),
             this, SLOT(handleLogString(QString)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(selectStemFromListSignal(QStringList)),
             this, SLOT(handleSelectStemFromList(QStringList)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(requestMsgBox(QString)),
             this, SLOT(handleMsgBoxRequest(QString)));
    connect (stemWorker, SIGNAL(finishedPolling()),
             this, SLOT(handlePollingFinished()), Qt::QueuedConnection);

    // per port parts
    connect (stemWorker, SIGNAL(portVoltageCurrentChanged(int, int32_t, int32_t)),
            this, SLOT(handlePortVoltageCurrent(int, int32_t, int32_t)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(hubModeChanged(uint32_t)),
             this, SLOT(handleHubMode(uint32_t)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(hubStateChanged(int, QString, int8_t)),
            this, SLOT(handleHubState(int, QString, int8_t)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(hubErrorStatusChanged(int, QString)),
             this, SLOT(handleHubErrorStatus(int, QString)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(portCurrentLimitChanged(int, uint32_t)),
             this, SLOT(handlePortCurrentLimit(int, uint32_t)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(portModeChanged(int, uint8_t)),
             this, SLOT(handlePortMode(int, uint8_t)), Qt::QueuedConnection);

    // system parts
    connect (stemWorker, SIGNAL(temperatureChanged(QString)),
             this, SLOT(handleTemperature(QString)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(inputVoltageChanged(uint32_t)),
             this, SLOT(handleInputVoltage(uint32_t)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(inputVoltageCurrentChanged(uint32_t, uint32_t)),
             this, SLOT(handleInputVoltageCurrent(uint32_t, uint32_t)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(userLedChanged(uint8_t)),
              this, SLOT(handleUserLed(uint8_t)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(stemInfoChanged(uint32_t, QString, uint32_t)),
              this, SLOT(handleStemInfo(uint32_t, QString, uint32_t)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(uptimeChanged(QString)),
             this, SLOT(handleUptime(QString)));

    // upstream parts
    connect (stemWorker, SIGNAL(upstreamPortChanged(QString)),
             this, SLOT(handleUpstreamPort(QString)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(upstreamModeChanged(uint8_t)),
             this, SLOT(handleUpstreamMode(uint8_t)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(upstreamBoostChanged(uint8_t)),
             this, SLOT(handleUpstreamBoost(uint8_t)), Qt::QueuedConnection);

    // downstream parts
    connect (stemWorker, SIGNAL(enumerationDelayChanged(uint32_t)),
             this, SLOT(handleEnumerationDelay(uint32_t)), Qt::QueuedConnection);
    connect (stemWorker, SIGNAL(downstreamBoostChanged(uint8_t)),
             this, SLOT(handleDownstreamBoost(uint8_t)), Qt::QueuedConnection);

    // ////////////////////////////////////////////////////
    // connect UI signals to stem worker slots
    connect(this, SIGNAL(userSelectedStemForConnection(QString)),
            stemWorker, SLOT(connectUserChosenStem(QString)));

    // per port parts
    connect(this, SIGNAL(userChangedUSBDataState(int,bool)),
            stemWorker, SLOT(changeUSBPortDataState(int,bool)));
    connect(this, SIGNAL(Sig_userChangedUSBDataState_SS(int,bool)),
            stemWorker, SLOT(Slot_userChangedUSBDataState_SS(int,bool)));
    connect(this, SIGNAL(Sig_userChangedUSBDataState_HS(int,bool)),
            stemWorker, SLOT(Slot_userChangedUSBDataState_HS(int,bool)));

    connect(stemWorker, SIGNAL(Sig_HandleDataSpeed(int, int)),
            this, SLOT(Slot_HandleDataSpeed(int, int)));




    connect(this, SIGNAL(userChangedUSBPowerState(int,bool)),
            stemWorker, SLOT(changeUSBPortPowerState(int,bool)));
    connect(this, SIGNAL(userClearedPortError(int)),
            stemWorker, SLOT(clearPortError(int)));
    connect(this, SIGNAL(userChangedPortCurrentLimit(int,uint32_t)),
            stemWorker, SLOT(changeUSBPortCurrentLimit(int,uint32_t)));
    connect(this, SIGNAL(userChangedPortMode(int,bool)),
            stemWorker, SLOT(changeUSBPortMode(int,bool)));
    connect(this, SIGNAL(userChangedUSBPortEnableState(int,bool)),
            stemWorker, SLOT(changeUSBPortEnableState(int,bool)));


    // system parts
    connect(this, SIGNAL(userChangedUserLed(bool)),
            stemWorker, SLOT(changeUserLed(bool)));
    connect(this, SIGNAL(userSavedState(void)),
            stemWorker, SLOT(saveState(void)));
    connect(this, SIGNAL(userResetDevice(void)),
            stemWorker, SLOT(resetDevice(void)));
    connect(this, SIGNAL(userShowEventLogs(void)),
            stemWorker, SLOT(showEventLogs(void)));

    // upstream parts
    connect(this, SIGNAL(userChangedUpstreamMode(int)),
            stemWorker, SLOT(changeUpstreamMode(int)));
    connect(this, SIGNAL(userChangedUpstreamBoost(uint8_t)),
            stemWorker, SLOT(changeUpstreamBoost(uint8_t)));

    // downstream parts
    connect(this, SIGNAL(userChangedEnumerationDelay(uint32_t)),
            stemWorker, SLOT(changeEnumerationDelay(uint32_t)));
    connect(this, SIGNAL(userChangedDownstreamBoost(uint8_t)),
            stemWorker, SLOT(changeDownstreamBoost(uint8_t)));

    // once we know what type of device, clean up the UI
    connect(stemWorker, SIGNAL(Sig_Secondary_GUI_Init()),
            this, SLOT(Slot_Secondary_GUI_Init()));

    // make the sparklines clickable
    connect(ui->voltageSparklinePort0, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->voltageSparklinePort1, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->voltageSparklinePort2, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->voltageSparklinePort3, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->voltageSparklinePort4, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->voltageSparklinePort5, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->voltageSparklinePort6, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->voltageSparklinePort7, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->currentSparklinePort0, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->currentSparklinePort1, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->currentSparklinePort2, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->currentSparklinePort3, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->currentSparklinePort4, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->currentSparklinePort5, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->currentSparklinePort6, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));
    connect(ui->currentSparklinePort7, SIGNAL(mousePress(QMouseEvent*)),
            this, SLOT(plotClick()));



    // port title double-click set up and signals
    portAndSystemLabels[0] = ui->clickToEdit_p0;
    portAndSystemLabels[1] = ui->clickToEdit_p1;
    portAndSystemLabels[2] = ui->clickToEdit_p2;
    portAndSystemLabels[3] = ui->clickToEdit_p3;
    portAndSystemLabels[4] = ui->clickToEdit_p4;
    portAndSystemLabels[5] = ui->clickToEdit_p5;
    portAndSystemLabels[6] = ui->clickToEdit_p6;
    portAndSystemLabels[7] = ui->clickToEdit_p7;
    portAndSystemLabels[8] = ui->clickToEdit_hub_system;

    // set port label indexes
    for(int i=0; i<8; i++){
        portAndSystemLabels[i]->labelIndex = i;
        portAndSystemLabels[i]->setText(QString("Port %1").arg(i));
        connect(portAndSystemLabels[i], SIGNAL(clickToEditLabel_Changed(QString, int)),
                this, SLOT(handle_clickToEditLabel_Changed(QString, int)));
    }

    portAndSystemLabels[8]->labelIndex = 8;
    portAndSystemLabels[8]->setText("My Acroname Hub");
    connect(portAndSystemLabels[8], SIGNAL(clickToEditLabel_Changed(QString,int)),
            this, SLOT(handle_clickToEditLabel_Changed(QString,int)));
    connect(stemWorker, SIGNAL(sig_nameChanged(QString,int)),
            this, SLOT(slot_handleNameChanged(QString, int)));


    // create and connect all the plot windows
    // if things are runngin too slow, this is also done
    // when a graph is clicked on. Just comment out this for loop
    for(int port=0; port<8; port++){
        VandIdataWindow[port] = new PlotWindow(port, m_startTimeMs, this);

        // setup the graphs
        VandIdataWindow[port]->setupVandIplots(port);

        // connect the update signals with the slots
        connect(&plotUpdateTimer, SIGNAL(timeout()), VandIdataWindow[port], SLOT(updatePlots()));
    }

    connect(this, SIGNAL(portNameChanged(QString, int)),
            stemWorker, SLOT(changePortName(QString, int)));
    connect(this, SIGNAL(systemNameChanged(QString)),
            stemWorker, SLOT(changeSystemName(QString)));


    // start the worker thread
    stemWorkerThread.start();


    // do a single shot in case the polling takes a long time so we don't
    // overlap the updates
    QTimer::singleShot(pollingDelay, stemWorker, SLOT(pollStemForChanges()));
    // start polling timer to track how long the polling takes
    pollingFireTime.start();

    // start a plot update timer
    connect(&plotUpdateTimer, SIGNAL(timeout()), this, SLOT(updatePlots()), Qt::DirectConnection);
    plotUpdateTimer.setInterval(plotUpdateDelay);
    plotUpdateTimer.start();

    // example debug statement to check the threading
    //qDebug() << "hubTool ID is " << this->thread()->currentThreadId();

    //If we were called by StemTool then the spec != nullptr.
    //There are some things we should not do if called by StemTool.
    //Currently we should NOT call appNap, because stemtool is doing that.

    if(spec)    { _isStemToolInstance = true;   }
    else        { _isStemToolInstance = false;  }

    if(! _isStemToolInstance) {
        #ifdef __APPLE__
            //These are the terminal commands to change local defaults of an app
            //"defaults read HubTool" can be used to check the current status.
//            QProcess appProcess;
//            appProcess.start("defaults write HubTool NSAppSleepDisabled -bool YES");
//            appProcess.waitForFinished();
//            appProcess.start("defaults write HubTool LSAppSleepDisabled -bool YES");
//            appProcess.waitForFinished();
//            appProcess.close();

            napper.suspend();
        #endif
    }


}

void HubTool::init()
{
    init_gui();
}

HubTool::~HubTool()
{

    if(! _isStemToolInstance) {
        #ifdef __APPLE__
            napper.resume();
        #endif
    }


    // stop the worker thread and clean up
    qDebug() << "cleaning up from stem worker thread";
    stemWorkerThread.quit();
    stemWorkerThread.wait();
    delete stemWorker;

    qDebug() << "cleaning up plot windows";
    for(int port=0;port<8;port++){
        if(VandIdataWindow[port])
            delete VandIdataWindow[port];
    }

    qDebug() << "cleaning up GUI";
    delete ui;
}


// Update the temperature
void HubTool::init_gui()
{

    ui->labelSystemTemperature->setAlignment(Qt::AlignRight);
    ui->labelInputVoltage->setAlignment(Qt::AlignRight);
    ui->labelModel->setAlignment(Qt::AlignRight);
    ui->labelSerialNumber->setAlignment(Qt::AlignRight);
    ui->labelUptime->setAlignment(Qt::AlignRight);
    ui->labelFirmwareVersion->setAlignment(Qt::AlignRight);
    ui->labelErrorStatusUSB0->setText("");
    ui->labelErrorStatusUSB1->setText("");
    ui->labelErrorStatusUSB2->setText("");
    ui->labelErrorStatusUSB3->setText("");
    ui->labelErrorStatusUSB4->setText("");
    ui->labelErrorStatusUSB5->setText("");
    ui->labelErrorStatusUSB6->setText("");
    ui->labelErrorStatusUSB7->setText("");

    // create/populate an array of the voltage and current plots for easy access throughout
    voltageSparkline[0]=ui->voltageSparklinePort0;
    voltageSparkline[1]=ui->voltageSparklinePort1;
    voltageSparkline[2]=ui->voltageSparklinePort2;
    voltageSparkline[3]=ui->voltageSparklinePort3;
    voltageSparkline[4]=ui->voltageSparklinePort4;
    voltageSparkline[5]=ui->voltageSparklinePort5;
    voltageSparkline[6]=ui->voltageSparklinePort6;
    voltageSparkline[7]=ui->voltageSparklinePort7;

    currentSparkline[0]=ui->currentSparklinePort0;
    currentSparkline[1]=ui->currentSparklinePort1;
    currentSparkline[2]=ui->currentSparklinePort2;
    currentSparkline[3]=ui->currentSparklinePort3;
    currentSparkline[4]=ui->currentSparklinePort4;
    currentSparkline[5]=ui->currentSparklinePort5;
    currentSparkline[6]=ui->currentSparklinePort6;
    currentSparkline[7]=ui->currentSparklinePort7;

    for(int port=0;port<8;port++){
        setupVoltageSparkline(voltageSparkline[port]);
        setupCurrentSparkline(currentSparkline[port]);
    }

    //SpinBox's - Current Limit
    ui->spinBox_USB0->setRange(0,4095);
    ui->spinBox_USB0->setValue(4095);
    ui->spinBox_USB1->setRange(0,4095);
    ui->spinBox_USB1->setValue(4095);
    ui->spinBox_USB2->setRange(0,4095);
    ui->spinBox_USB2->setValue(4095);
    ui->spinBox_USB3->setRange(0,4095);
    ui->spinBox_USB3->setValue(4095);

    ui->spinBox_USB4->setRange(0,4095);
    ui->spinBox_USB4->setValue(4095);
    ui->spinBox_USB5->setRange(0,4095);
    ui->spinBox_USB5->setValue(4095);
    ui->spinBox_USB6->setRange(0,4095);
    ui->spinBox_USB6->setValue(4095);
    ui->spinBox_USB7->setRange(0,4095);
    ui->spinBox_USB7->setValue(4095);

    // hide the save+reset flags
    ui->saveResetFlag0->setVisible(false);
    ui->saveResetFlag1->setVisible(false);
    ui->saveResetFlag2->setVisible(false);
    ui->saveResetFlag3->setVisible(false);
    ui->saveResetFlag4->setVisible(false);
    ui->saveResetFlag5->setVisible(false);
    ui->saveResetFlag6->setVisible(false);
    ui->saveResetFlag7->setVisible(false);
    ui->saveResetFlag->setVisible(false);

}

void HubTool::Slot_Secondary_GUI_Init() {
    uint8_t model;
    stemWorker->getConnectedModel(&model);

    //Special init/setup/configuration for USBHub2x4
    if (model == aMODULE_TYPE_USBHub2x4){
        ui->groupBox_USB4->setDisabled(true);
        ui->groupBox_USB5->setDisabled(true);
        ui->groupBox_USB6->setDisabled(true);
        ui->groupBox_USB7->setDisabled(true);

        ui->checkBoxDataSS_USB0->setDisabled(true);
        ui->checkBoxDataSS_USB1->setDisabled(true);
        ui->checkBoxDataSS_USB2->setDisabled(true);
        ui->checkBoxDataSS_USB3->setDisabled(true);
        ui->checkBoxDataSS_USB4->setDisabled(true);
        ui->checkBoxDataSS_USB5->setDisabled(true);
        ui->checkBoxDataSS_USB6->setDisabled(true);
        ui->checkBoxDataSS_USB7->setDisabled(true);

        ui->checkBoxDataHS_USB0->setDisabled(true);
        ui->checkBoxDataHS_USB1->setDisabled(true);
        ui->checkBoxDataHS_USB2->setDisabled(true);
        ui->checkBoxDataHS_USB3->setDisabled(true);
        ui->checkBoxDataHS_USB4->setDisabled(true);
        ui->checkBoxDataHS_USB5->setDisabled(true);
        ui->checkBoxDataHS_USB6->setDisabled(true);
        ui->checkBoxDataHS_USB7->setDisabled(true);

        //These are disabled for 2x4, but will keep people from thinking
        //They are not "High Speed"
        ui->checkBoxDataHS_USB0->setChecked(true);
        ui->checkBoxDataHS_USB1->setChecked(true);
        ui->checkBoxDataHS_USB2->setChecked(true);
        ui->checkBoxDataHS_USB3->setChecked(true);
        ui->checkBoxDataHS_USB4->setChecked(true);
        ui->checkBoxDataHS_USB5->setChecked(true);
        ui->checkBoxDataHS_USB6->setChecked(true);
        ui->checkBoxDataHS_USB7->setChecked(true);
    }

    // hide all the save+reset flags again since we may have reconnected after a power cycle
    // hide the save+reset flags
    ui->saveResetFlag0->setVisible(false);
    ui->saveResetFlag1->setVisible(false);
    ui->saveResetFlag2->setVisible(false);
    ui->saveResetFlag3->setVisible(false);
    ui->saveResetFlag4->setVisible(false);
    ui->saveResetFlag5->setVisible(false);
    ui->saveResetFlag6->setVisible(false);
    ui->saveResetFlag7->setVisible(false);
    ui->saveResetFlag->setVisible(false);
}


// handle event keys so the main window can be closed with com-W/alt-F4
void HubTool::keyPressEvent(QKeyEvent *e) {
    // close the window on com-W and alt-F4...
    if(e->matches(QKeySequence::Close)){
        for(int port=0; port<8; port++){
            if(VandIdataWindow[port]) VandIdataWindow[port]->close();
        }
        HubTool::close();
    }
    else
        QWidget::keyPressEvent(e);
}



// /////////////////////////////////////////////////////////////////////////////
// Slots for updates from the hardware
// /////////////////////////////////////////////////////////////////////////////
// slots for the stemWorker thread to send results to
void HubTool::handleResults(int channel, bool checked)
{
    qDebug() << "Handling result " << channel << " and " << checked;
}

void HubTool::handlePollingFinished(){
    // calculate the update rate
    static double historticalPollingTime[updateRateAvgLength];
    double pollingTimeSum=0.0;

    for(int i=0; i<updateRateAvgLength-1; i++){
        historticalPollingTime[i]=historticalPollingTime[i+1];
        pollingTimeSum+=historticalPollingTime[i];
    }
    historticalPollingTime[updateRateAvgLength-1]=pollingFireTime.elapsed()/1000.0;
    pollingTimeSum += historticalPollingTime[updateRateAvgLength-1];

    double updateRate = pollingTimeSum/updateRateAvgLength > 0.0 ? 1.0/(pollingTimeSum/updateRateAvgLength) : 0.0;
    ui->labelUpdateRate->setText(QString("%1Hz").arg(QString::number(updateRate, 'f', 1)));

    // just refire the timer after a short period
    // using this method ensures the polling is done, and then waits
    // and then fires anothr polling
    QTimer::singleShot(pollingDelay, stemWorker, SLOT(pollStemForChanges()));


    // reset the timer for update rate
    pollingFireTime.restart();
}


void HubTool::handleLogString(QString logString){
#define TIMESTAMP_FMT "yyyy.MM.dd HH:mm:ss:zzz"
    QString logLine;
    logLine = QDateTime::currentDateTime().toString(TIMESTAMP_FMT) + ": " + logString;
    ui->textEditLog->append(logLine);
}

void HubTool::handleSelectStemFromList(QStringList availableSerialNumbers){
    bool ok;
    QString serialNumber = QInputDialog::getItem(this, tr("Select a BrainStem"),
                                         tr("More than one device found. Please select one:"), availableSerialNumbers, 0, false, &ok);

    if (ok && !serialNumber.isEmpty()){
        emit userSelectedStemForConnection(serialNumber);
    }
}

void HubTool::handleMsgBoxRequest(QString message){
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(message);
    msgBox.exec();
}




// per port parts
void HubTool::handlePortVoltageCurrent(int channel, int32_t microVolts, int32_t microAmps){
    // add data to the plot and update the plot:
    // add data to plot and update the plot
    const double range_size = 32;
    double currentTimeKey = (QDateTime::currentMSecsSinceEpoch() - m_startTimeMs)/1000.0;

    voltageSparkline[channel]->graph()->addData(currentTimeKey, microVolts/1000000.0);
    currentSparkline[channel]->graph()->addData(currentTimeKey, microAmps/1000000.0);
    voltageSparkline[channel]->graph()->data()->removeBefore(currentTimeKey-range_size);
    currentSparkline[channel]->graph()->data()->removeBefore(currentTimeKey-range_size);

    VandIdataWindow[channel]->handlePortVoltageCurrent(channel, microVolts, microAmps);

    // update the text fields
    switch (channel){
        case 0:
            ui->labelVoltageUSB0->setText(QString("%1 V").arg(QString::number(microVolts/1000000.0, 'f', 3)));
            ui->labelCurrentUSB0->setText(QString("%1 A").arg(QString::number(microAmps/1000000.0, 'f', 3)));
            break;
        case 1:
            ui->labelVoltageUSB1->setText(QString("%1 V").arg(QString::number(microVolts/1000000.0, 'f', 3)));
            ui->labelCurrentUSB1->setText(QString("%1 A").arg(QString::number(microAmps/1000000.0, 'f', 3)));
            break;
        case 2:
            ui->labelVoltageUSB2->setText(QString("%1 V").arg(QString::number(microVolts/1000000.0, 'f', 3)));
            ui->labelCurrentUSB2->setText(QString("%1 A").arg(QString::number(microAmps/1000000.0, 'f', 3)));
            break;
        case 3:
            ui->labelVoltageUSB3->setText(QString("%1 V").arg(QString::number(microVolts/1000000.0, 'f', 3)));
            ui->labelCurrentUSB3->setText(QString("%1 A").arg(QString::number(microAmps/1000000.0, 'f', 3)));
            break;
        case 4:
            ui->labelVoltageUSB4->setText(QString("%1 V").arg(QString::number(microVolts/1000000.0, 'f', 3)));
            ui->labelCurrentUSB4->setText(QString("%1 A").arg(QString::number(microAmps/1000000.0, 'f', 3)));
            break;
        case 5:
            ui->labelVoltageUSB5->setText(QString("%1 V").arg(QString::number(microVolts/1000000.0, 'f', 3)));
            ui->labelCurrentUSB5->setText(QString("%1 A").arg(QString::number(microAmps/1000000.0, 'f', 3)));
            break;
        case 6:
            ui->labelVoltageUSB6->setText(QString("%1 V").arg(QString::number(microVolts/1000000.0, 'f', 3)));
            ui->labelCurrentUSB6->setText(QString("%1 A").arg(QString::number(microAmps/1000000.0, 'f', 3)));
            break;
        case 7:
            ui->labelVoltageUSB7->setText(QString("%1 V").arg(QString::number(microVolts/1000000.0, 'f', 3)));
            ui->labelCurrentUSB7->setText(QString("%1 A").arg(QString::number(microAmps/1000000.0, 'f', 3)));
            break;
    }
}

void HubTool::handleHubMode(uint32_t hubMode){

    uint8_t model;
    stemWorker->getConnectedModel(&model);
    int numPorts = model == aMODULE_TYPE_USBHub2x4 ? 4 : 8;

    // Loop through the 32bit return value bit masking for each part of the hubMode
    // Each bit has a meaning
    // 0  1  2  3  4  5  6  7  8  9 ...14 15 16 17 18 19
    // H0 P0 H1 P1 H2 P2 H3 P3 H4 P4...H7 P7 S0 R0 S1 R0
    // H=HiSpeed, P=Power, S=SuperSpeed, R=Reserved

    // make a quick bit mask for HS-only, SS-only and HS+SS
    const uint32_t HSonlyMask=0x00000001;
    const uint32_t SSonlyMask=0x00010000;
    const uint32_t HSandSSMask=0x00010001;

    // go through the USBHubMode bits masking first for HS+SS
    // then check for individual speeds
    for (int x = 0; x < numPorts; x++) {
        // shift and mask by 2x port number since each port gets 2 bits
        if (((hubMode >> x*2) & HSandSSMask) == HSandSSMask) Slot_HandleDataSpeed(x, 3);
        else if (((hubMode >> x*2) & SSonlyMask) == SSonlyMask) Slot_HandleDataSpeed(x, 2);
        else if (((hubMode >> x*2) & HSonlyMask) == HSonlyMask) Slot_HandleDataSpeed(x, 1);
        else Slot_HandleDataSpeed(x, 0);
    }

    // check if the port power is enabled by shifting by the port*2+1 and masking
    // with a power enabled mask of 0x10
    for (int x = 0; x < numPorts; x++) {
        if ((hubMode >> ((x*2)+1)) & 0x1) Slot_HandlePower(x, true);
        else Slot_HandlePower(x, false);
    }

    // check to see if the whole port is enabled
    uint32_t portEnabledMask = model == aMODULE_TYPE_USBHub2x4 ?
                0x00000003 /* Power+HS */ :
                0x00010003 /* Power+HS+SS */;
    for (int x = 0; x < numPorts; x++) {
        if (((hubMode >> x*2) & portEnabledMask) == portEnabledMask) Slot_HandlePort(x, true);
        else Slot_HandlePort(x, false);
    }
}

void HubTool::handleHubState(int channel, QString stateStr, int8_t spd) {

    switch(channel){
        case 0:
            ui->labelErrorStatusUSB0->setText(stateStr);
            if (spd == usbDownstreamDataSpeed_hs) {
                ui->checkBoxDataHS_USB0->setStyleSheet("QCheckBox { background-color: #ffff00; }");
                ui->checkBoxDataSS_USB0->setStyleSheet("QCheckBox { background-color: none; }");
            } else if (spd == usbDownstreamDataSpeed_ss) {
                ui->checkBoxDataHS_USB0->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB0->setStyleSheet("QCheckBox { background-color: #66ff33; }");
            } else {
                ui->checkBoxDataHS_USB0->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB0->setStyleSheet("QCheckBox { background-color: none; }");
            } break;
        case 1:
            ui->labelErrorStatusUSB1->setText(stateStr);
            if (spd == usbDownstreamDataSpeed_hs) {
                ui->checkBoxDataHS_USB1->setStyleSheet("QCheckBox { background-color: #ffff00; }");
                ui->checkBoxDataSS_USB1->setStyleSheet("QCheckBox { background-color: none; }");
            } else if (spd == usbDownstreamDataSpeed_ss) {
                ui->checkBoxDataHS_USB1->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB1->setStyleSheet("QCheckBox { background-color: #66ff33; }");
            } else {
                ui->checkBoxDataHS_USB1->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB1->setStyleSheet("QCheckBox { background-color: none; }");
            } break;
        case 2:
            ui->labelErrorStatusUSB2->setText(stateStr);
            if (spd == usbDownstreamDataSpeed_hs) {
                ui->checkBoxDataHS_USB2->setStyleSheet("QCheckBox { background-color: #ffff00; }");
                ui->checkBoxDataSS_USB2->setStyleSheet("QCheckBox { background-color: none; }");
            } else if (spd == usbDownstreamDataSpeed_ss) {
                ui->checkBoxDataHS_USB2->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB2->setStyleSheet("QCheckBox { background-color: #66ff33; }");
            } else {
                ui->checkBoxDataHS_USB2->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB2->setStyleSheet("QCheckBox { background-color: none; }");
            } break;
        case 3:
            ui->labelErrorStatusUSB3->setText(stateStr);
            if (spd == usbDownstreamDataSpeed_hs) {
                ui->checkBoxDataHS_USB3->setStyleSheet("QCheckBox { background-color: #ffff00; }");
                ui->checkBoxDataSS_USB3->setStyleSheet("QCheckBox { background-color: none; }");
            } else if (spd == usbDownstreamDataSpeed_ss) {
                ui->checkBoxDataHS_USB3->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB3->setStyleSheet("QCheckBox { background-color: #66ff33; }");
            } else {
                ui->checkBoxDataHS_USB3->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB3->setStyleSheet("QCheckBox { background-color: none; }");
            } break;
        case 4:
            ui->labelErrorStatusUSB4->setText(stateStr);
            if (spd == usbDownstreamDataSpeed_hs) {
                ui->checkBoxDataHS_USB4->setStyleSheet("QCheckBox { background-color: #ffff00; }");
                ui->checkBoxDataSS_USB4->setStyleSheet("QCheckBox { background-color: none; }");
            } else if (spd == usbDownstreamDataSpeed_ss) {
                ui->checkBoxDataHS_USB4->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB4->setStyleSheet("QCheckBox { background-color: #66ff33; }");
            } else {
                ui->checkBoxDataHS_USB4->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB4->setStyleSheet("QCheckBox { background-color: none; }");
            } break;
        case 5:
            ui->labelErrorStatusUSB5->setText(stateStr);
            if (spd == usbDownstreamDataSpeed_hs) {
                ui->checkBoxDataHS_USB5->setStyleSheet("QCheckBox { background-color: #ffff00; }");
                ui->checkBoxDataSS_USB5->setStyleSheet("QCheckBox { background-color: none; }");
            } else if (spd == usbDownstreamDataSpeed_ss) {
                ui->checkBoxDataHS_USB5->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB5->setStyleSheet("QCheckBox { background-color: #66ff33; }");
            } else {
                ui->checkBoxDataHS_USB5->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB5->setStyleSheet("QCheckBox { background-color: none; }");
            } break;
        case 6:
            ui->labelErrorStatusUSB6->setText(stateStr);
            if (spd == usbDownstreamDataSpeed_hs) {
                ui->checkBoxDataHS_USB6->setStyleSheet("QCheckBox { background-color: #ffff00; }");
                ui->checkBoxDataSS_USB6->setStyleSheet("QCheckBox { background-color: none; }");
            } else if (spd == usbDownstreamDataSpeed_ss) {
                ui->checkBoxDataHS_USB6->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB6->setStyleSheet("QCheckBox { background-color: #66ff33; }");
            } else {
                ui->checkBoxDataHS_USB6->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB6->setStyleSheet("QCheckBox { background-color: none; }");
            } break;
        case 7:
            ui->labelErrorStatusUSB7->setText(stateStr);
            if (spd == usbDownstreamDataSpeed_hs) {
                ui->checkBoxDataHS_USB7->setStyleSheet("QCheckBox { background-color: #ffff00; }");
                ui->checkBoxDataSS_USB7->setStyleSheet("QCheckBox { background-color: none; }");
            } else if (spd == usbDownstreamDataSpeed_ss) {
                ui->checkBoxDataHS_USB7->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB7->setStyleSheet("QCheckBox { background-color: #66ff33; }");
            } else {
                ui->checkBoxDataHS_USB7->setStyleSheet("QCheckBox { background-color: none; }");
                ui->checkBoxDataSS_USB7->setStyleSheet("QCheckBox { background-color: none; }");
            } break;
        default:
            handleLogString(QString("Uknown channel in handleHubState signal %1").arg(channel));
    }
}

void HubTool::handleHubErrorStatus(int channel, QString errorStr){
    if (errorStr.length() != 0) {
        handleLogString(QString("Error on port%1: %2").arg(channel).arg(errorStr));
    }
}

void HubTool::handlePortCurrentLimit(int channel, uint32_t microAmps){
    //QComboBox* currentLimitComboBox;

    // select the right combo box
    switch(channel){
        case(0): ui->spinBox_USB0->setValue(microAmps/MICRO_TO_MILLI); break;
        case(1): ui->spinBox_USB1->setValue(microAmps/MICRO_TO_MILLI); break;
        case(2): ui->spinBox_USB2->setValue(microAmps/MICRO_TO_MILLI); break;
        case(3): ui->spinBox_USB3->setValue(microAmps/MICRO_TO_MILLI); break;
        case(4): ui->spinBox_USB4->setValue(microAmps/MICRO_TO_MILLI); break;
        case(5): ui->spinBox_USB5->setValue(microAmps/MICRO_TO_MILLI); break;
        case(6): ui->spinBox_USB6->setValue(microAmps/MICRO_TO_MILLI); break;
        case(7): ui->spinBox_USB7->setValue(microAmps/MICRO_TO_MILLI); break;
        default:
            handleLogString(QString("Uknown channel in handlePortCurrentLimit signal %1").arg(channel));
    }
}

void HubTool::handlePortMode(int channel, uint8_t mode) {
    QCheckBox* portModeCheckBox = nullptr;
    // select the right combo box
    switch(channel){
        case(0):
            portModeCheckBox = ui->checkBoxPortModeUSB0; break;
        case(1):
            portModeCheckBox = ui->checkBoxPortModeUSB1; break;
        case(2):
            portModeCheckBox = ui->checkBoxPortModeUSB2; break;
        case(3):
            portModeCheckBox = ui->checkBoxPortModeUSB3; break;
        case(4):
            portModeCheckBox = ui->checkBoxPortModeUSB4; break;
        case(5):
            portModeCheckBox = ui->checkBoxPortModeUSB5; break;
        case(6):
            portModeCheckBox = ui->checkBoxPortModeUSB6; break;
        case(7):
            portModeCheckBox = ui->checkBoxPortModeUSB7; break;
        default:
            handleLogString(QString("Uknown channel in handlePortMode signal %1").arg(channel));
    }
    if (portModeCheckBox) portModeCheckBox->setChecked((mode == usbPortMode_cdp)? true: false);
}


// system parts
void HubTool::handleTemperature(QString temperatureString){
    ui->labelSystemTemperature->setText(temperatureString);
}

void HubTool::handleInputVoltage(uint32_t inputVoltage){
    ui->labelInputVoltage->setText(QString("%1 V").arg(QString::number((inputVoltage)/1000000.0, 'f', 1)));
}

void HubTool::handleInputVoltageCurrent(uint32_t inputVoltage, uint32_t inputCurrent){
    ui->labelInputVoltage->setText(QString("%1 V (%2 A)").arg(QString::number((inputVoltage)/1000000.0, 'f', 1)).arg(QString::number((inputCurrent)/1000000.0, 'f', 1)));
}

void HubTool::handleUserLed(uint8_t ledOn){
    ui->checkBoxLED->setChecked(ledOn > 0);
}

void HubTool::handleStemInfo(uint32_t serialNumber, QString model, uint32_t firmwareVersion){
    // set the FW version field
    QString formatted_firmware_version = QString("%1.%2.%3").arg(QString::number(aVERSION_UNPACK_MAJOR(firmwareVersion)),
                                                                 QString::number(aVERSION_UNPACK_MINOR(firmwareVersion)),
                                                                 QString::number(aVERSION_UNPACK_PATCH(firmwareVersion)));

    ui->labelFirmwareVersion->setText(formatted_firmware_version);

    // set the serial number
    QString stringSerial = QString("%1").arg(serialNumber, 8, 16, QChar('0')).toUpper();
    ui->labelSerialNumber->setText(QString("0x%1").arg(stringSerial));

    // set the model number
    ui->labelModel->setText(model);
}

void HubTool::handleUptime(QString uptimeText){
    ui->labelUptime->setText(uptimeText);
}


// upstream parts
void HubTool::handleUpstreamPort(QString upstreamPortSelection){
    ui->labelUpstreamUSB->setText(upstreamPortSelection);
}

void HubTool::handleUpstreamMode(uint8_t upstreamPortMode){
    switch(upstreamPortMode){
        case usbUpstreamModeAuto:
            ui->comboBoxUpstreamUSB->setCurrentIndex(0);
            break;
        case usbUpstreamModePort0:
            ui->comboBoxUpstreamUSB->setCurrentIndex(1);
            break;
        case usbUpstreamModePort1:
            ui->comboBoxUpstreamUSB->setCurrentIndex(2);
            break;
        default:
            // TODO: need to add an "unknown" in the comboBox
            //ui->comboBoxUpstreamUSB->setCurrentIndex(2);
            break;
    }
}

void HubTool::handleUpstreamBoost(uint8_t boost){
    QComboBox* combo = ui->comboBoxBoostUpstream;
    switch(boost){
    case 0:
        combo->setCurrentIndex(0);
        break;
    case 1:
        combo->setCurrentIndex(1);
        break;
    case 2:
        combo->setCurrentIndex(2);
        break;
    case 3:
        combo->setCurrentIndex(3);
        break;
    default:
        combo->setCurrentIndex(0);
        handleLogString(QString("Unknown upstream boost setting %1").arg(boost));
        break;
    }
}

void HubTool::on_pollingDelaySpinBox_editingFinished()
{
    // check the range
    int pollingDelay_request = ui->pollingDelaySpinBox->value();
    if (pollingDelay_request > 10000){
        ui->pollingDelaySpinBox->setValue(10000);
        pollingDelay_request = 10000;
    }
    if (pollingDelay_request < 0){
        ui->pollingDelaySpinBox->setValue(0);
        pollingDelay_request = 0;
    }
    pollingDelay = pollingDelay_request;
}


// downstream parts
void HubTool::handleEnumerationDelay(uint32_t ms_delay){
    ui->spinBoxDelay->setValue(int32_t(ms_delay));
}

void HubTool::handleDownstreamBoost(uint8_t boost){
    QComboBox* combo = ui->comboBoxBoostDownStream;
    qDebug() << "Downstream boost changed to " << boost;
    switch(boost){
    case 0:
        combo->setCurrentIndex(0);
        break;
    case 1:
        combo->setCurrentIndex(1);
        break;
    case 2:
        combo->setCurrentIndex(2);
        break;
    case 3:
        combo->setCurrentIndex(3);
        break;
    default:
        combo->setCurrentIndex(0);
        handleLogString(QString("Unknown downstream boost setting %1").arg(boost));
        break;
    }
}



void HubTool::Slot_HandleDataSpeed(int channel, int mode){
    uint8_t model=0;
    stemWorker->getConnectedModel(&model);

    // a quick way to handle checking/unchecking the data box
    // for devices that only support HiSpeed (not SuperSpeed)
    // also use this from falsely checking the SS box
    bool OnlyHSdataSupported = model == aMODULE_TYPE_USBHub2x4 ? true : false;

    switch (channel){
        case 0:
            switch (mode){
                case 0:
                    ui->checkBoxDataUSB0->setChecked(false);
                    ui->checkBoxDataHS_USB0->setChecked(false);
                    ui->checkBoxDataSS_USB0->setChecked(false);
                    break;
                case 1:
                    ui->checkBoxDataUSB0->setChecked(true && OnlyHSdataSupported);
                    ui->checkBoxDataHS_USB0->setChecked(true);
                    ui->checkBoxDataSS_USB0->setChecked(false);
                    break;
                case 2:
                    ui->checkBoxDataUSB0->setChecked(false);
                    ui->checkBoxDataHS_USB0->setChecked(false);
                    ui->checkBoxDataSS_USB0->setChecked(true && !OnlyHSdataSupported);
                    break;
                case 3:
                    ui->checkBoxDataUSB0->setChecked(true);
                    ui->checkBoxDataHS_USB0->setChecked(true);
                    ui->checkBoxDataSS_USB0->setChecked(true && !OnlyHSdataSupported);
                    break;
            } // mode switch
            break;
        case 1:
            switch (mode){
                case 0:
                    ui->checkBoxDataUSB1->setChecked(false);
                    ui->checkBoxDataHS_USB1->setChecked(false);
                    ui->checkBoxDataSS_USB1->setChecked(false);
                    break;
                case 1:
                    ui->checkBoxDataUSB1->setChecked(true && OnlyHSdataSupported);
                    ui->checkBoxDataHS_USB1->setChecked(true);
                    ui->checkBoxDataSS_USB1->setChecked(false);
                    break;
                case 2:
                    ui->checkBoxDataUSB1->setChecked(false);
                    ui->checkBoxDataHS_USB1->setChecked(false);
                    ui->checkBoxDataSS_USB1->setChecked(true && !OnlyHSdataSupported);
                    break;
                case 3:
                    ui->checkBoxDataUSB1->setChecked(true);
                    ui->checkBoxDataHS_USB1->setChecked(true);
                    ui->checkBoxDataSS_USB1->setChecked(true && !OnlyHSdataSupported);
                    break;
            } // mode switch
            break;
        case 2:
            switch (mode){
                case 0:
                    ui->checkBoxDataUSB2->setChecked(false);
                    ui->checkBoxDataHS_USB2->setChecked(false);
                    ui->checkBoxDataSS_USB2->setChecked(false);
                    break;
                case 1:
                    ui->checkBoxDataUSB2->setChecked(true && OnlyHSdataSupported);
                    ui->checkBoxDataHS_USB2->setChecked(true);
                    ui->checkBoxDataSS_USB2->setChecked(false);
                    break;
                case 2:
                    ui->checkBoxDataUSB2->setChecked(false);
                    ui->checkBoxDataHS_USB2->setChecked(false);
                    ui->checkBoxDataSS_USB2->setChecked(true && !OnlyHSdataSupported);
                    break;
                case 3:
                    ui->checkBoxDataUSB2->setChecked(true);
                    ui->checkBoxDataHS_USB2->setChecked(true);
                    ui->checkBoxDataSS_USB2->setChecked(true && !OnlyHSdataSupported);
                    break;
            } // mode switch
            break;
        case 3:
            switch (mode){
                case 0:
                    ui->checkBoxDataUSB3->setChecked(false);
                    ui->checkBoxDataHS_USB3->setChecked(false);
                    ui->checkBoxDataSS_USB3->setChecked(false);
                    break;
                case 1:
                    ui->checkBoxDataUSB3->setChecked(true && OnlyHSdataSupported);
                    ui->checkBoxDataHS_USB3->setChecked(true);
                    ui->checkBoxDataSS_USB3->setChecked(false);
                    break;
                case 2:
                    ui->checkBoxDataUSB3->setChecked(false);
                    ui->checkBoxDataHS_USB3->setChecked(false);
                    ui->checkBoxDataSS_USB3->setChecked(true && !OnlyHSdataSupported);
                    break;
                case 3:
                    ui->checkBoxDataUSB3->setChecked(true);
                    ui->checkBoxDataHS_USB3->setChecked(true);
                    ui->checkBoxDataSS_USB3->setChecked(true && !OnlyHSdataSupported);
                    break;
            } // mode switch
            break;
        case 4:
            switch (mode){
                case 0:
                    ui->checkBoxDataUSB4->setChecked(false);
                    ui->checkBoxDataHS_USB4->setChecked(false);
                    ui->checkBoxDataSS_USB4->setChecked(false);
                    break;
                case 1:
                    ui->checkBoxDataUSB4->setChecked(true && OnlyHSdataSupported);
                    ui->checkBoxDataHS_USB4->setChecked(true);
                    ui->checkBoxDataSS_USB4->setChecked(false);
                    break;
                case 2:
                    ui->checkBoxDataUSB4->setChecked(false);
                    ui->checkBoxDataHS_USB4->setChecked(false);
                    ui->checkBoxDataSS_USB4->setChecked(true && !OnlyHSdataSupported);
                    break;
                case 3:
                    ui->checkBoxDataUSB4->setChecked(true);
                    ui->checkBoxDataHS_USB4->setChecked(true);
                    ui->checkBoxDataSS_USB4->setChecked(true && !OnlyHSdataSupported);
                    break;
            } // mode switch
            break;
        case 5:
            switch (mode){
                case 0:
                    ui->checkBoxDataUSB5->setChecked(false);
                    ui->checkBoxDataHS_USB5->setChecked(false);
                    ui->checkBoxDataSS_USB5->setChecked(false);
                    break;
                case 1:
                    ui->checkBoxDataUSB5->setChecked(true && OnlyHSdataSupported);
                    ui->checkBoxDataHS_USB5->setChecked(true);
                    ui->checkBoxDataSS_USB5->setChecked(false);
                    break;
                case 2:
                    ui->checkBoxDataUSB5->setChecked(false);
                    ui->checkBoxDataHS_USB5->setChecked(false);
                    ui->checkBoxDataSS_USB5->setChecked(true && !OnlyHSdataSupported);
                    break;
                case 3:
                    ui->checkBoxDataUSB5->setChecked(true);
                    ui->checkBoxDataHS_USB5->setChecked(true);
                    ui->checkBoxDataSS_USB5->setChecked(true && !OnlyHSdataSupported);
                    break;
            } // mode switch
            break;
        case 6:
            switch (mode){
                case 0:
                    ui->checkBoxDataUSB6->setChecked(false);
                    ui->checkBoxDataHS_USB6->setChecked(false);
                    ui->checkBoxDataSS_USB6->setChecked(false);
                    break;
                case 1:
                    ui->checkBoxDataUSB6->setChecked(true && OnlyHSdataSupported);
                    ui->checkBoxDataHS_USB6->setChecked(true);
                    ui->checkBoxDataSS_USB6->setChecked(false);
                    break;
                case 2:
                    ui->checkBoxDataUSB6->setChecked(false);
                    ui->checkBoxDataHS_USB6->setChecked(false);
                    ui->checkBoxDataSS_USB6->setChecked(true && !OnlyHSdataSupported);
                    break;
                case 3:
                    ui->checkBoxDataUSB6->setChecked(true);
                    ui->checkBoxDataHS_USB6->setChecked(true);
                    ui->checkBoxDataSS_USB6->setChecked(true && !OnlyHSdataSupported);
                    break;
            } // mode switch
            break;
        case 7:
            switch (mode){
                case 0:
                    ui->checkBoxDataUSB7->setChecked(false);
                    ui->checkBoxDataHS_USB7->setChecked(false);
                    ui->checkBoxDataSS_USB7->setChecked(false);
                    break;
                case 1:
                    ui->checkBoxDataUSB7->setChecked(true && OnlyHSdataSupported);
                    ui->checkBoxDataHS_USB7->setChecked(true);
                    ui->checkBoxDataSS_USB7->setChecked(false);
                    break;
                case 2:
                    ui->checkBoxDataUSB7->setChecked(false);
                    ui->checkBoxDataHS_USB7->setChecked(false);
                    ui->checkBoxDataSS_USB7->setChecked(true && !OnlyHSdataSupported);
                    break;
                case 3:
                    ui->checkBoxDataUSB7->setChecked(true);
                    ui->checkBoxDataHS_USB7->setChecked(true);
                    ui->checkBoxDataSS_USB7->setChecked(true && !OnlyHSdataSupported);
                    break;
            } // mode switch
            break;
    } // channel switch
}

void HubTool::Slot_HandlePower(int channel, bool checked) {
    switch (channel) {
        case 0: ui->checkBoxPowerUSB0->setChecked(checked); break;
        case 1: ui->checkBoxPowerUSB1->setChecked(checked); break;
        case 2: ui->checkBoxPowerUSB2->setChecked(checked); break;
        case 3: ui->checkBoxPowerUSB3->setChecked(checked); break;
        case 4: ui->checkBoxPowerUSB4->setChecked(checked); break;
        case 5: ui->checkBoxPowerUSB5->setChecked(checked); break;
        case 6: ui->checkBoxPowerUSB6->setChecked(checked); break;
        case 7: ui->checkBoxPowerUSB7->setChecked(checked); break;
        default: break;
    }
}

void HubTool::Slot_HandlePort(int channel, bool checked) {
    switch (channel) {
        case 0: ui->checkBoxPortUSB0->setChecked(checked); break;
        case 1: ui->checkBoxPortUSB1->setChecked(checked); break;
        case 2: ui->checkBoxPortUSB2->setChecked(checked); break;
        case 3: ui->checkBoxPortUSB3->setChecked(checked); break;
        case 4: ui->checkBoxPortUSB4->setChecked(checked); break;
        case 5: ui->checkBoxPortUSB5->setChecked(checked); break;
        case 6: ui->checkBoxPortUSB6->setChecked(checked); break;
        case 7: ui->checkBoxPortUSB7->setChecked(checked); break;
        default:break;
    }
}




// /////////////////////////////////////////////////////////////////////////////
// Slots for changes made to the UI by the user
// /////////////////////////////////////////////////////////////////////////////
void HubTool::on_checkBoxLED_clicked()
{
    int checkState= ui->checkBoxLED->checkState();
    bool bOn = checkState > 0;
    emit userChangedUserLed(bOn);
}

void HubTool::on_comboBoxUpstreamUSB_activated(int index)
{
    switch(index)
    {
        case 0:
            emit userChangedUpstreamMode(usbUpstreamModeAuto);
            break;
        case 1:
            emit userChangedUpstreamMode(usbUpstreamModePort0);
            break;
        case 2:
            emit userChangedUpstreamMode(usbUpstreamModePort1);
            break;
        case 3:
            emit userChangedUpstreamMode(usbUpstreamModeNone);
            break;
    }
}


//Data Enable Check Boxes.
void HubTool::on_checkBoxDataUSB0_clicked(bool checked)
{

    emit userChangedUSBDataState(0, checked);
    emit Sig_userChangedUSBDataState_SS(0, checked);
    emit Sig_userChangedUSBDataState_HS(0, checked);
}

void HubTool::on_checkBoxDataUSB1_clicked(bool checked)
{
    emit userChangedUSBDataState(1, checked);
    emit Sig_userChangedUSBDataState_SS(1, checked);
    emit Sig_userChangedUSBDataState_HS(1, checked);
}

void HubTool::on_checkBoxDataUSB2_clicked(bool checked)
{
    emit userChangedUSBDataState(2, checked);
    emit Sig_userChangedUSBDataState_SS(2, checked);
    emit Sig_userChangedUSBDataState_HS(2, checked);
}

void HubTool::on_checkBoxDataUSB3_clicked(bool checked)
{
    emit userChangedUSBDataState(3, checked);
    emit Sig_userChangedUSBDataState_SS(3, checked);
    emit Sig_userChangedUSBDataState_HS(3, checked);
}

void HubTool::on_checkBoxDataUSB4_clicked(bool checked)
{
    emit userChangedUSBDataState(4, checked);
    emit Sig_userChangedUSBDataState_SS(4, checked);
    emit Sig_userChangedUSBDataState_HS(4, checked);
}

void HubTool::on_checkBoxDataUSB5_clicked(bool checked)
{
    emit userChangedUSBDataState(5, checked);
    emit Sig_userChangedUSBDataState_SS(5, checked);
    emit Sig_userChangedUSBDataState_HS(5, checked);
}

void HubTool::on_checkBoxDataUSB6_clicked(bool checked)
{
    emit userChangedUSBDataState(6, checked);
    emit Sig_userChangedUSBDataState_SS(6, checked);
    emit Sig_userChangedUSBDataState_HS(6, checked);
}

void HubTool::on_checkBoxDataUSB7_clicked(bool checked)
{
    emit userChangedUSBDataState(7, checked);
    emit Sig_userChangedUSBDataState_SS(7, checked);
    emit Sig_userChangedUSBDataState_HS(7, checked);
}


//High Speed Check Boxes. (Will have no affect on USBHub2x4)
void HubTool::on_checkBoxDataHS_USB0_clicked(bool checked)
{
    //ui->checkBoxDataHS_USB0->setChecked(!checked);
    emit Sig_userChangedUSBDataState_HS(0, checked);
}

void HubTool::on_checkBoxDataHS_USB1_clicked(bool checked)
{
    //ui->checkBoxDataHS_USB1->setChecked(!checked);
    emit Sig_userChangedUSBDataState_HS(1, checked);
}

void HubTool::on_checkBoxDataHS_USB2_clicked(bool checked)
{
    //ui->checkBoxDataHS_USB2->setChecked(!checked);
    emit Sig_userChangedUSBDataState_HS(2, checked);
}

void HubTool::on_checkBoxDataHS_USB3_clicked(bool checked)
{
    //ui->checkBoxDataHS_USB3->setChecked(!checked);
    emit Sig_userChangedUSBDataState_HS(3, checked);
}

void HubTool::on_checkBoxDataHS_USB4_clicked(bool checked)
{
    //ui->checkBoxDataHS_USB4->setChecked(!checked);
    emit Sig_userChangedUSBDataState_HS(4, checked);
}

void HubTool::on_checkBoxDataHS_USB5_clicked(bool checked)
{
    //ui->checkBoxDataHS_USB5->setChecked(!checked);
    emit Sig_userChangedUSBDataState_HS(5, checked);
}

void HubTool::on_checkBoxDataHS_USB6_clicked(bool checked)
{
    //ui->checkBoxDataHS_USB6->setChecked(!checked);
    emit Sig_userChangedUSBDataState_HS(6, checked);
}

void HubTool::on_checkBoxDataHS_USB7_clicked(bool checked)
{
    //ui->checkBoxDataHS_USB7->setChecked(!checked);
    emit Sig_userChangedUSBDataState_HS(7, checked);
}


//Super Speed Check Box. (Will have no affect on USBHub2x4)
void HubTool::on_checkBoxDataSS_USB0_clicked(bool checked)
{
    emit Sig_userChangedUSBDataState_SS(0, checked);
}

void HubTool::on_checkBoxDataSS_USB1_clicked(bool checked)
{
    emit Sig_userChangedUSBDataState_SS(1, checked);
}

void HubTool::on_checkBoxDataSS_USB2_clicked(bool checked)
{
    emit Sig_userChangedUSBDataState_SS(2, checked);
}

void HubTool::on_checkBoxDataSS_USB3_clicked(bool checked)
{
    emit Sig_userChangedUSBDataState_SS(3, checked);
}

void HubTool::on_checkBoxDataSS_USB4_clicked(bool checked)
{
    emit Sig_userChangedUSBDataState_SS(4, checked);
}

void HubTool::on_checkBoxDataSS_USB5_clicked(bool checked)
{
    emit Sig_userChangedUSBDataState_SS(5, checked);
}

void HubTool::on_checkBoxDataSS_USB6_clicked(bool checked)
{
    emit Sig_userChangedUSBDataState_SS(6, checked);
}

void HubTool::on_checkBoxDataSS_USB7_clicked(bool checked)
{
    emit Sig_userChangedUSBDataState_SS(7, checked);
}


//Power Check Boxes
void HubTool::on_checkBoxPowerUSB0_clicked(bool checked)
{
    emit userChangedUSBPowerState(0, checked);
}

void HubTool::on_checkBoxPowerUSB1_clicked(bool checked)
{
    emit userChangedUSBPowerState(1, checked);
}

void HubTool::on_checkBoxPowerUSB2_clicked(bool checked)
{
    emit userChangedUSBPowerState(2, checked);
}

void HubTool::on_checkBoxPowerUSB3_clicked(bool checked)
{
    emit userChangedUSBPowerState(3, checked);
}

void HubTool::on_checkBoxPowerUSB4_clicked(bool checked)
{
    emit userChangedUSBPowerState(4, checked);
}

void HubTool::on_checkBoxPowerUSB5_clicked(bool checked)
{
    emit userChangedUSBPowerState(5, checked);
}

void HubTool::on_checkBoxPowerUSB6_clicked(bool checked)
{
    emit userChangedUSBPowerState(6, checked);
}

void HubTool::on_checkBoxPowerUSB7_clicked(bool checked)
{
    emit userChangedUSBPowerState(7, checked);
}


//Port Check Boxes
void HubTool::on_checkBoxPortUSB0_clicked(bool checked)
{
    emit userChangedUSBPortEnableState(0, checked);
}

void HubTool::on_checkBoxPortUSB1_clicked(bool checked)
{
    emit userChangedUSBPortEnableState(1, checked);
}

void HubTool::on_checkBoxPortUSB2_clicked(bool checked)
{
    emit userChangedUSBPortEnableState(2, checked);
}

void HubTool::on_checkBoxPortUSB3_clicked(bool checked)
{
    emit userChangedUSBPortEnableState(3, checked);
}

void HubTool::on_checkBoxPortUSB4_clicked(bool checked)
{
    emit userChangedUSBPortEnableState(4, checked);
}

void HubTool::on_checkBoxPortUSB5_clicked(bool checked)
{
    emit userChangedUSBPortEnableState(5, checked);
}

void HubTool::on_checkBoxPortUSB6_clicked(bool checked)
{
    emit userChangedUSBPortEnableState(6, checked);
}

void HubTool::on_checkBoxPortUSB7_clicked(bool checked)
{
    emit userChangedUSBPortEnableState(7, checked);
}


//Port Mode SpinBoxes (CDP - Charging Dowstream Port - High current mode)
void HubTool::on_checkBoxPortModeUSB0_clicked(bool checked) {
    uint8_t model;
    stemWorker->getConnectedModel(&model);

    // if this is a 3+, then save and reset is needed
    // enable the UI flag for this
    if(model == aMODULE_TYPE_USBHub3p){
        ui->saveResetFlag0->setVisible(true);
    }

    emit userChangedPortMode(0, checked);
}

void HubTool::on_checkBoxPortModeUSB1_clicked(bool checked) {
    uint8_t model;
    stemWorker->getConnectedModel(&model);

    // if this is a 3+, then save and reset is needed
    // enable the UI flag for this
    if(model == aMODULE_TYPE_USBHub3p){
        ui->saveResetFlag1->setVisible(true);
    }
    emit userChangedPortMode(1, checked);
}

void HubTool::on_checkBoxPortModeUSB2_clicked(bool checked) {
    uint8_t model;
    stemWorker->getConnectedModel(&model);

    // if this is a 3+, then save and reset is needed
    // enable the UI flag for this
    if(model == aMODULE_TYPE_USBHub3p){
        ui->saveResetFlag2->setVisible(true);
    }
    emit userChangedPortMode(2, checked);
}

void HubTool::on_checkBoxPortModeUSB3_clicked(bool checked) {
    uint8_t model;
    stemWorker->getConnectedModel(&model);

    // if this is a 3+, then save and reset is needed
    // enable the UI flag for this
    if(model == aMODULE_TYPE_USBHub3p){
        ui->saveResetFlag3->setVisible(true);
    }
    emit userChangedPortMode(3, checked);
}

void HubTool::on_checkBoxPortModeUSB4_clicked(bool checked) {
    uint8_t model;
    stemWorker->getConnectedModel(&model);

    // if this is a 3+, then save and reset is needed
    // enable the UI flag for this
    if(model == aMODULE_TYPE_USBHub3p){
        ui->saveResetFlag4->setVisible(true);
    }
    emit userChangedPortMode(4, checked);
}

void HubTool::on_checkBoxPortModeUSB5_clicked(bool checked) {
    uint8_t model;
    stemWorker->getConnectedModel(&model);

    // if this is a 3+, then save and reset is needed
    // enable the UI flag for this
    if(model == aMODULE_TYPE_USBHub3p){
        ui->saveResetFlag5->setVisible(true);
    }
    emit userChangedPortMode(5, checked);
}

void HubTool::on_checkBoxPortModeUSB6_clicked(bool checked) {
    uint8_t model;
    stemWorker->getConnectedModel(&model);

    // if this is a 3+, then save and reset is needed
    // enable the UI flag for this
    if(model == aMODULE_TYPE_USBHub3p){
        ui->saveResetFlag6->setVisible(true);
    }
    emit userChangedPortMode(6, checked);
}

void HubTool::on_checkBoxPortModeUSB7_clicked(bool checked) {
    uint8_t model;
    stemWorker->getConnectedModel(&model);

    // if this is a 3+, then save and reset is needed
    // enable the UI flag for this
    if(model == aMODULE_TYPE_USBHub3p){
        ui->saveResetFlag7->setVisible(true);
    }
    emit userChangedPortMode(7, checked);
}


//Current Limit spin Boxes
void HubTool::on_spinBox_USB0_editingFinished()
{
    setPortCurrentLimit(0, ui->spinBox_USB0->value());
}

void HubTool::on_spinBox_USB1_editingFinished()
{
    setPortCurrentLimit(1, ui->spinBox_USB1->value());
}

void HubTool::on_spinBox_USB2_editingFinished()
{
    setPortCurrentLimit(2, ui->spinBox_USB2->value());
}

void HubTool::on_spinBox_USB3_editingFinished()
{
    setPortCurrentLimit(3, ui->spinBox_USB3->value());
}

void HubTool::on_spinBox_USB4_editingFinished()
{
    setPortCurrentLimit(4, ui->spinBox_USB4->value());
}

void HubTool::on_spinBox_USB5_editingFinished()
{
    setPortCurrentLimit(5, ui->spinBox_USB5->value());
}

void HubTool::on_spinBox_USB6_editingFinished()
{
    setPortCurrentLimit(6, ui->spinBox_USB6->value());
}

void HubTool::on_spinBox_USB7_editingFinished()
{
    setPortCurrentLimit(7, ui->spinBox_USB7->value());
}



void HubTool::roundEnumerationDelay()
{
    // If something typed in isn't a multiple of 100
    // make it so it is.
    int delay = ui->spinBoxDelay->value();
    ui->spinBoxDelay->setValue(100*(delay/100));
}

void HubTool::on_spinBoxDelay_editingFinished()
{
    uint8_t model;
    stemWorker->getConnectedModel(&model);

    // Call this to make the delay a multiple of 100
    //roundEnumerationDelay();

    if(model == aMODULE_TYPE_USBHub3p){
        // save+reset is needed
        // enable the UI flag for this
        ui->saveResetFlag->setVisible(true);
        qDebug()<<"saveResetFlagDownstreamDelay";
    }

    uint32_t ms_delay = uint32_t(ui->spinBoxDelay->value());
    emit userChangedEnumerationDelay(ms_delay);
}

void HubTool::on_pushButtonSaveState_clicked()
{
    emit userSavedState();
}



void HubTool::on_pushButtonReset_clicked()
{
    emit userResetDevice();
}

void HubTool::on_pushButtonShowEventLogs_clicked()
{
    emit userShowEventLogs();
}



void HubTool::on_pushButtonClearPortError_clicked()
{
    emit userClearedPortError(0);
    emit userClearedPortError(1);
    emit userClearedPortError(2);
    emit userClearedPortError(3);

    uint8_t model = 0;
    stemWorker->getConnectedModel(&model);
    if (model == aMODULE_TYPE_USBHub3p) {
        emit userClearedPortError(4);
        emit userClearedPortError(5);
        emit userClearedPortError(6);
        emit userClearedPortError(7);
    }
}


void HubTool::setPortCurrentLimit(int channel, int value)
{
    uint32_t remainder = 0;
    uint32_t microAmps = 0;
    uint8_t model;
    uint32_t uint_value = uint32_t(value); //For error supression, less casting.

    stemWorker->getConnectedModel(&model);

    //Current limit logic for the USBHub2x4:  It will trickly down the avaliable current options
    //until the lowest acceptable one is found.
    if (model == aMODULE_TYPE_USBHub2x4){
        if ((uint_value * MICRO_TO_MILLI) <= USBHUB2X4_CURRENTLIMIT_2500) microAmps = USBHUB2X4_CURRENTLIMIT_2500;
        if ((uint_value * MICRO_TO_MILLI) <= USBHUB2X4_CURRENTLIMIT_2000) microAmps = USBHUB2X4_CURRENTLIMIT_2000;
        if ((uint_value * MICRO_TO_MILLI) <= USBHUB2X4_CURRENTLIMIT_1800) microAmps = USBHUB2X4_CURRENTLIMIT_1800;
        if ((uint_value * MICRO_TO_MILLI) <= USBHUB2X4_CURRENTLIMIT_1500) microAmps = USBHUB2X4_CURRENTLIMIT_1500;
        if ((uint_value * MICRO_TO_MILLI) <= USBHUB2X4_CURRENTLIMIT_1200) microAmps = USBHUB2X4_CURRENTLIMIT_1200;
        if ((uint_value * MICRO_TO_MILLI) <= USBHUB2X4_CURRENTLIMIT_1000) microAmps = USBHUB2X4_CURRENTLIMIT_1000;
        if ((uint_value * MICRO_TO_MILLI) <= USBHUB2X4_CURRENTLIMIT_900) microAmps = USBHUB2X4_CURRENTLIMIT_900;
        if ((uint_value * MICRO_TO_MILLI) <= USBHUB2X4_CURRENTLIMIT_500) microAmps = USBHUB2X4_CURRENTLIMIT_500;

    } else if (model ==  aMODULE_TYPE_USBHub3p) {
        //Test for min or max. Max really isn't needed because the spinBox was setup that way
        //in the gui_init().
        if ((uint_value * MICRO_TO_MILLI) <= MIN_USBHUB3P_CURRENT_LIMIT) {
            emit userChangedPortCurrentLimit(channel, MIN_USBHUB3P_CURRENT_LIMIT);
            return;
        } else if ((uint_value * MICRO_TO_MILLI) >= MAX_USBHUB3P_CURRENT_LIMIT) {
            emit userChangedPortCurrentLimit(channel, MAX_USBHUB3P_CURRENT_LIMIT);
            return;
        }

        remainder = ((uint_value * MICRO_TO_MILLI) % USBHUB3P_CURRENT_LIMIT_STEP);
        //Yes, I know I could reduce USBHUB3P_CURRENT_LIMIT_STEP/USBHUB3P_CURRENT_LIMIT_STEP, but I am expecting
        //the value to be truncated.
        microAmps = (((uint_value * MICRO_TO_MILLI) / USBHUB3P_CURRENT_LIMIT_STEP) * USBHUB3P_CURRENT_LIMIT_STEP);

        if (remainder > 0) {
            microAmps += USBHUB3P_CURRENT_LIMIT_STEP;
            //Test that we don't go over the max value.
            if (microAmps > MAX_USBHUB3P_CURRENT_LIMIT) microAmps = MAX_USBHUB3P_CURRENT_LIMIT;
        }
    }

    emit userChangedPortCurrentLimit(channel, microAmps);
}



void HubTool::on_comboBoxBoostUpstream_activated(int index)
{
    // this will be called even if the index didn't change
    // using currentIndexChanged would prevent extra calls
    // but that slot is fired when the list is changed
    // programatically

    // save+reset is needed
    // enable the UI flag for this
    ui->saveResetFlag->setVisible(true);

    uint8_t boost=0;
    switch(index){
        case 0:
            boost = 0;
            break;
        case 1:
            boost = 1;
            break;
        case 2:
            boost = 2;
            break;
        case 3:
            boost = 3;
            break;
        default:
            boost = 0;
            break;
    }
    emit userChangedUpstreamBoost(boost);
}

void HubTool::on_comboBoxBoostDownStream_activated(int index)
{
    // this will be called even if the index didn't change
    // using currentIndexChanged would prevent extra calls
    // but that slot is fired when the list is changed
    // programatically

    // save+reset is needed
    // enable the UI flag for this
    ui->saveResetFlag->setVisible(true);

    uint8_t boost=0;
    switch(index){
        case 0:
            boost = 0;
            break;
        case 1:
            boost = 1;
            break;
        case 2:
            boost = 2;
            break;
        case 3:
            boost = 3;
            break;
        default:
            boost = 0;
            break;
    }
    emit userChangedDownstreamBoost(boost);
}




void HubTool::setupVoltageSparkline(QCustomPlot *customPlot)
{
    customPlot->legend->setVisible(false);
    customPlot->axisRect()->setAutoMargins(QCP::msNone);
    customPlot->axisRect()->setMargins(QMargins(0,0,0,0));
    customPlot->setBackground(this->palette().window().color());
    customPlot->xAxis->setVisible(false);
    customPlot->yAxis->setVisible(false);
    customPlot->yAxis->setRange(4.5, 5.5);

    customPlot->addGraph(); // blue line
    customPlot->graph()->setPen(QPen(Qt::blue));
}



void HubTool::setupCurrentSparkline(QCustomPlot *customPlot)
{
    customPlot->legend->setVisible(false);
    customPlot->axisRect()->setAutoMargins(QCP::msNone);
    customPlot->axisRect()->setMargins(QMargins(0,0,0,0));
    customPlot->setBackground(this->palette().window().color());
    customPlot->xAxis->setVisible(false);
    customPlot->yAxis->setVisible(false);

    customPlot->addGraph();
    customPlot->graph()->setPen(QPen(Qt::red));
    customPlot->yAxis->setRange(-0.05, 6);
}



void HubTool::updatePlots(){
    double currentTimeKey = (QDateTime::currentMSecsSinceEpoch() - m_startTimeMs)/1000.0;
    const double range_size = 16;

    uint8_t model=0;
    stemWorker->getConnectedModel(&model);
    int numPorts = model == aMODULE_TYPE_USBHub2x4 ? 4 : 8;

    for(int port=0; port<numPorts; port++){
        voltageSparkline[port]->xAxis->setRange(currentTimeKey+0.25, range_size, Qt::AlignRight);
        voltageSparkline[port]->graph()->data()->removeBefore(currentTimeKey-range_size);
        voltageSparkline[port]->replot(QCustomPlot::rpQueuedReplot);

        currentSparkline[port]->xAxis->setRange(currentTimeKey+0.25, range_size, Qt::AlignRight);
        currentSparkline[port]->graph()->data()->removeBefore(currentTimeKey-range_size);
        currentSparkline[port]->replot(QCustomPlot::rpQueuedReplot);
    }

}


void HubTool::plotClick()
{
    int port = -1;
    uint8_t model=0;
    stemWorker->getConnectedModel(&model);

    if(QObject::sender() == ui->voltageSparklinePort0 ||
       QObject::sender() == ui->currentSparklinePort0){
        port = 0;
    }
    if(QObject::sender() == ui->voltageSparklinePort1 ||
       QObject::sender() == ui->currentSparklinePort1){
        port = 1;
    }
    if(QObject::sender() == ui->voltageSparklinePort2 ||
       QObject::sender() == ui->currentSparklinePort2){
        port = 2;
    }
    if(QObject::sender() == ui->voltageSparklinePort3 ||
       QObject::sender() == ui->currentSparklinePort3){
        port = 3;
    }
    if((QObject::sender() == ui->voltageSparklinePort4 ||
       QObject::sender() == ui->currentSparklinePort4) &&
       model == aMODULE_TYPE_USBHub3p){
        port = 4;
    }
    if((QObject::sender() == ui->voltageSparklinePort5 ||
       QObject::sender() == ui->currentSparklinePort5) &&
       model == aMODULE_TYPE_USBHub3p){
        port = 5;
    }
    if((QObject::sender() == ui->voltageSparklinePort6 ||
       QObject::sender() == ui->currentSparklinePort6) &&
       model == aMODULE_TYPE_USBHub3p){
        port = 6;
    }
    if((QObject::sender() == ui->voltageSparklinePort7 ||
       QObject::sender() == ui->currentSparklinePort7) &&
       model == aMODULE_TYPE_USBHub3p){
        port = 7;
    }

    // swallow the click if it came from a invalid port
    // since these sparklines are disabled, this really should never happen
    if(port <= -1)
        return;

    // create the plot window if it hasn't been created yet
    if(!VandIdataWindow[port]){
        qDebug() << "re-creating plotWindow";

        // make a new plot window
        VandIdataWindow[port] = new PlotWindow(port, m_startTimeMs, this);

        // setup the graphs
        VandIdataWindow[port]->setupVandIplots(port);

        // connect the update signals with the slots
        connect(&plotUpdateTimer, SIGNAL(timeout()), VandIdataWindow[port], SLOT(updatePlots()));
    }

    // show the plot window
    // this brings it to the front if the window was just created
    VandIdataWindow[port]->showNormal();

    // if the window was buried, these usually bring it to the top
    VandIdataWindow[port]->activateWindow();
    VandIdataWindow[port]->raise();

}


void HubTool::handle_clickToEditLabel_Changed(QString name, int index){
    // after editting, save the updated port name to the stem
    qDebug() << "name label changed" << index;

    // clean up the name if needed
    name.replace("=", ":");
    name.replace("\n", " ");
    name.replace("\r", " ");

    // don't allow empty names
    if(name == ""){
        name = QString("Port %1").arg(index);
        if(index == 8){
            name = QString("My Acroname Hub");
        }
        portAndSystemLabels[index]->setText(name);
    }

    if(index >= 0 && index < 8){
        emit(portNameChanged(name, index));
    }
    else if(index == 8 ){
        emit(systemNameChanged(name));
    }
    else {
        qDebug() << "invalid index in handle_clickToEditLabel_Changed " << index;
    }
}

void HubTool::slot_handleNameChanged(QString name, int index){
    qDebug() << "GUI handling name data from stem " << index << "  " << name;
    if(index <= 8){
        portAndSystemLabels[index]->setText(name);
    }
}


