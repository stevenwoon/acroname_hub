#ifndef HUBTOOL_H
#define HUBTOOL_H


#include <QMainWindow>

#ifdef Q_OS_WIN
#define NOMINMAX
#endif

#include <QRadioButton>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>
#include <QTimer>
#include <QThread>
#include <QComboBox>
#include <QString>
#include <QStringList>
#include "qcustomplot.h"
#include "stemworker.h"
#include "plotwindow.h"
#include "clicktoeditlabel.h"

#include "appnap.h"

namespace Ui {
class HubTool;
}

class HubTool : public QMainWindow
{
    Q_OBJECT

public:
    explicit HubTool(linkSpec* spec = nullptr, QWidget *parent = nullptr);
    ~HubTool();

signals:
    void userSelectedStemForConnection(QString serialNumber);

    // per port parts
    void userChangedUSBDataState(int channel, bool enanbled);
    void Sig_userChangedUSBDataState_SS(int channel, bool enabled);
    void Sig_userChangedUSBDataState_HS(int channel, bool enabled);
    void userChangedUSBPowerState(int channel, bool enanbled);
    void userChangedUSBPortEnableState(int channel, bool enabled);
    void userChangedPortCurrentLimit(int channel, uint32_t microAmps);
    void userChangedPortMode(int channel, bool enabled);
    void userClearedPortError(int channel);

    // system parts
    void userChangedUserLed(bool enabled);
    void userSavedState(void);
    void userResetDevice(void);
    void userShowEventLogs(void);

    // upstream parts
    void userChangedUpstreamMode(int mode);
    void userChangedUpstreamBoost(uint8_t boost);

    // downstream parts
    void userChangedEnumerationDelay(uint32_t ms_delay);
    void userChangedDownstreamBoost(uint8_t boost);
    void portCurrentLimitChanged(int, uint32_t);

    // port and system labels
    void portNameChanged(QString name, int index);
    void systemNameChanged(QString name);

public slots:
    // slots for the stemWorker thread to send results tox
    void handleResults(int channel, bool checked); // test slot
    void handleLogString(QString logString);
    void handlePollingFinished();

    void handleSelectStemFromList(QStringList availableSerialNumbers);
    void handleMsgBoxRequest(QString message);
    void updatePlots();

    // per port parts
    void handlePortVoltageCurrent(int channel, int32_t microVolts, int32_t microAmps);
    void handleHubMode(uint32_t hubMode);
    void handleHubState(int channel, QString stateStr, int8_t spd);
    void handleHubErrorStatus(int channel, QString errorStr);
    void handlePortCurrentLimit(int channel, uint32_t microAmps);
    void handlePortMode(int channel, uint8_t mode);

    // system parts
    void handleTemperature(QString temperatureString);
    void handleInputVoltage(uint32_t inputVoltage);
    void handleInputVoltageCurrent(uint32_t inputVoltage, uint32_t inputCurrent);
    void handleUserLed(uint8_t ledOn);
    void handleStemInfo(uint32_t serialNumber, QString model, uint32_t firmwareVersion);
    void handleUptime(QString uptimeText);

    // upstream parts
    void handleUpstreamPort(QString upstreamPortSelection);
    void handleUpstreamMode(uint8_t upstreamPortMode);
    void handleUpstreamBoost(uint8_t boost);

    // downstream parts
    void handleEnumerationDelay(uint32_t ms_delay);
    void handleDownstreamBoost(uint8_t boost);

    void Slot_Secondary_GUI_Init();
    void Slot_HandleDataSpeed(int channel, int mode);
    void Slot_HandlePower(int channel, bool checked);
    void Slot_HandlePort(int channel, bool checked);

    // port labels
    void handle_clickToEditLabel_Changed(QString name, int index);
    void slot_handleNameChanged(QString name, int index);

protected:
    void keyPressEvent(QKeyEvent *e);

private slots:
    // slots for UI triggers
    void on_checkBoxLED_clicked();
    void on_comboBoxUpstreamUSB_activated(int index);

    void on_pushButtonSaveState_clicked();
    void on_pushButtonReset_clicked();
    void on_pushButtonShowEventLogs_clicked();
//    void on_comboBoxCurrentLimitUSB0_activated(int index);    //PLOT REMOVAL
//    void on_comboBoxCurrentLimitUSB1_activated(int index);    //PLOT REMOVAL
//    void on_comboBoxCurrentLimitUSB2_activated(int index);    //PLOT REMOVAL
//    void on_comboBoxCurrentLimitUSB3_activated(int index);    //PLOT REMOVAL

    void on_pushButtonClearPortError_clicked();
    void on_comboBoxBoostUpstream_activated(int index);
    void on_comboBoxBoostDownStream_activated(int index);
    void on_spinBoxDelay_editingFinished();
    void on_pollingDelaySpinBox_editingFinished();

    void on_checkBoxDataUSB0_clicked(bool checked);
    void on_checkBoxDataUSB1_clicked(bool checked);
    void on_checkBoxDataUSB2_clicked(bool checked);
    void on_checkBoxDataUSB3_clicked(bool checked);
    void on_checkBoxDataUSB4_clicked(bool checked);
    void on_checkBoxDataUSB5_clicked(bool checked);
    void on_checkBoxDataUSB6_clicked(bool checked);
    void on_checkBoxDataUSB7_clicked(bool checked);

    void on_checkBoxPowerUSB0_clicked(bool checked);
    void on_checkBoxPowerUSB1_clicked(bool checked);
    void on_checkBoxPowerUSB2_clicked(bool checked);
    void on_checkBoxPowerUSB3_clicked(bool checked);
    void on_checkBoxPowerUSB4_clicked(bool checked);
    void on_checkBoxPowerUSB5_clicked(bool checked);
    void on_checkBoxPowerUSB6_clicked(bool checked);
    void on_checkBoxPowerUSB7_clicked(bool checked);

    void on_checkBoxPortModeUSB0_clicked(bool checked);
    void on_checkBoxPortModeUSB1_clicked(bool checked);
    void on_checkBoxPortModeUSB2_clicked(bool checked);
    void on_checkBoxPortModeUSB3_clicked(bool checked);
    void on_checkBoxPortModeUSB4_clicked(bool checked);
    void on_checkBoxPortModeUSB5_clicked(bool checked);
    void on_checkBoxPortModeUSB6_clicked(bool checked);
    void on_checkBoxPortModeUSB7_clicked(bool checked);

    void on_spinBox_USB0_editingFinished();
    void on_spinBox_USB1_editingFinished();
    void on_spinBox_USB2_editingFinished();
    void on_spinBox_USB3_editingFinished();
    void on_spinBox_USB4_editingFinished();
    void on_spinBox_USB5_editingFinished();
    void on_spinBox_USB6_editingFinished();
    void on_spinBox_USB7_editingFinished();

    void on_checkBoxDataHS_USB0_clicked(bool checked);
    void on_checkBoxDataHS_USB1_clicked(bool checked);
    void on_checkBoxDataHS_USB2_clicked(bool checked);
    void on_checkBoxDataHS_USB3_clicked(bool checked);
    void on_checkBoxDataHS_USB4_clicked(bool checked);
    void on_checkBoxDataHS_USB5_clicked(bool checked);
    void on_checkBoxDataHS_USB6_clicked(bool checked);
    void on_checkBoxDataHS_USB7_clicked(bool checked);

    void on_checkBoxDataSS_USB0_clicked(bool checked);
    void on_checkBoxDataSS_USB1_clicked(bool checked);
    void on_checkBoxDataSS_USB2_clicked(bool checked);
    void on_checkBoxDataSS_USB3_clicked(bool checked);
    void on_checkBoxDataSS_USB4_clicked(bool checked);
    void on_checkBoxDataSS_USB5_clicked(bool checked);
    void on_checkBoxDataSS_USB6_clicked(bool checked);
    void on_checkBoxDataSS_USB7_clicked(bool checked);

    void on_checkBoxPortUSB0_clicked(bool checked);
    void on_checkBoxPortUSB1_clicked(bool checked);
    void on_checkBoxPortUSB2_clicked(bool checked);
    void on_checkBoxPortUSB3_clicked(bool checked);
    void on_checkBoxPortUSB4_clicked(bool checked);
    void on_checkBoxPortUSB5_clicked(bool checked);
    void on_checkBoxPortUSB6_clicked(bool checked);
    void on_checkBoxPortUSB7_clicked(bool checked);

    void plotClick();


private:
    Ui::HubTool *ui;
    PlotWindow* VandIdataWindow[8];

#ifdef __APPLE__
    AppNapSuspender napper;
#endif

    QTime pollingFireTime;
    QTimer plotUpdateTimer;
    StemWorker *stemWorker;
    QThread stemWorkerThread;

    uint8_t numUSB;
    QCustomPlot* voltageSparkline[8];
    QCustomPlot* currentSparkline[8];

    ClickToEditLabel* portAndSystemLabels[9];

    //USBHub2x4 Current limit options (chip allowed configurations)
    static const uint32_t USBHUB2X4_CURRENTLIMIT_500 = 500000; //500mA
    static const uint32_t USBHUB2X4_CURRENTLIMIT_900 = 900000; //900mA
    static const uint32_t USBHUB2X4_CURRENTLIMIT_1000 = 1000000; //1000mA
    static const uint32_t USBHUB2X4_CURRENTLIMIT_1200 = 1200000; //1200mA
    static const uint32_t USBHUB2X4_CURRENTLIMIT_1500 = 1500000; //1500mA
    static const uint32_t USBHUB2X4_CURRENTLIMIT_1800 = 1800000; //1800mA
    static const uint32_t USBHUB2X4_CURRENTLIMIT_2000 = 2000000; //2000mA
    static const uint32_t USBHUB2X4_CURRENTLIMIT_2500 = 2500000; //2500mA

    static const uint32_t MIN_USBHUB3P_CURRENT_LIMIT = 1000;    //1mA
    static const uint32_t MAX_USBHUB3P_CURRENT_LIMIT = 4095000; //2500mA
    static const uint32_t USBHUB3P_CURRENT_LIMIT_STEP = 1000;   //1mA

    static const uint32_t MICRO_TO_MILLI = 1000;

    void setupVoltageSparkline(QCustomPlot *customPlot);
    void setupCurrentSparkline(QCustomPlot *customPlot);

    void init();
    void init_logo();
    void init_gui();

    void roundEnumerationDelay();
    void setPortCurrentLimit(int channel, int value);

    qint64 m_startTimeMs;

    bool _isStemToolInstance;

};

#endif // HUBTOOL_H
