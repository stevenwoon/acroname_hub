#ifndef STEMWORKER_H
#define STEMWORKER_H

#include <QObject>
#include <QThread>
#include <QMap>
#include <QTimer>
#include <QString>
#include <QStringList>

#include "BrainStem2/BrainStem-all.h"
#include "appnap.h"

using namespace Acroname::BrainStem;

class StemWorker : public QObject
//class StemWorker : public QThread
{
    Q_OBJECT
    //QThread stemWorkerThread;

public:
    explicit StemWorker(linkSpec* spec = nullptr);
    ~StemWorker();
    void getConnectedModel(uint8_t *model);

signals:
    // test signal
    void resultReady(int channel, bool checked);
    void finishedPolling();

    // logging
    void logStringReady(QString logLine);

    // make a dialog to select a stem
    void selectStemFromListSignal(QStringList availableSerialNumbers);
    void requestMsgBox(QString message);

    // per port parts
    void portVoltageCurrentChanged(int channel, int32_t newVoltage, int32_t newCurrent);
    void portCurrentLimitChanged(int channel, uint32_t microAmps);
    void portModeChanged(int channel, uint8_t mode);
    void hubModeChanged(uint32_t hubMode);
    void hubStateChanged(int channel, QString errorString, int8_t spd);
    void hubErrorStatusChanged(int channel, QString errorString);


    // system parts
    void temperatureChanged(QString temperatureString);
    void inputVoltageChanged(uint32_t inputVoltage);
    void inputVoltageCurrentChanged(uint32_t newVoltage, uint32_t newCurrent);
    void userLedChanged(uint8_t ledOn);
    void stemInfoChanged(uint32_t serialNumber, QString model, uint32_t firmwareVersion);
    void uptimeChanged(QString uptimeString);

    // upstream parts
    void upstreamPortChanged(QString upstreamPortSelection);
    void upstreamModeChanged(uint8_t upstreamPortMode);
    void upstreamBoostChanged(uint8_t boost);

    // downstream parts
    void enumerationDelayChanged(uint32_t ms_delay);
    void downstreamBoostChanged(uint8_t boost);

    void Sig_Secondary_GUI_Init();
    void Sig_HandleDataSpeed(int channel, int mode);

    // port and system name
    void sig_nameChanged(QString name, int index);

public slots:
    void start();
    void pollStemForChanges();
    void connectUserChosenStem(QString stemSerialNumber);

    // per port parts
    void changeUSBPortDataState(int channel, bool enabled);
    void Slot_userChangedUSBDataState_SS(int channel, bool checked);
    void Slot_userChangedUSBDataState_HS(int channel, bool checked);

    void changeUSBPortPowerState(int channel, bool enabled);
    void clearPortError(int channel);
    void changeUSBPortCurrentLimit(int channel, uint32_t limit);
    void changeUSBPortMode(int channel, bool enabled);
    void changeUSBPortEnableState(int channel, bool enabled);

    // upstream parts
    void changeUpstreamMode(int mode);
    void changeUpstreamBoost(uint8_t boost);

    // downstream parts
    void changeEnumerationDelay(uint32_t ms_delay);
    void changeDownstreamBoost(uint8_t boost);

    // system parts
    void changeUserLed(bool enabled);
    void saveState(void);
    void resetDevice(void);
    void showEventLogs(void);

    // port and system names
    void changePortName(QString name, int index);
    void changeSystemName(QString name);

private:
    Module module;
    SystemClass system;
    StoreClass store[2];
    USBClass usb;
    TemperatureClass temp;
    linkSpec stemToolSpec;

    uint8_t numUSB;
    uint8_t connectedModel;
    uint32_t firmwareVersion;

    uint32_t currentLimit[8] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
                                0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};



    uint8_t connectRetryCount;
    bool firstPollingEvent;
    uint32_t hubMode;
    uint32_t portState[8];
    bool firmwareUpdateMessageFlag_HubState;
    uint32_t portError[8];
    bool firmwareUpdateMessageFlag_HubError;
    uint8_t portMode[8];
    int32_t temperature;
    int32_t maxTemperature;
    bool firmwareUpdateMessageFlag_Temperature;
    uint32_t voltage;
    uint32_t current;
    uint8_t ledState;
    uint32_t serialNumber;
    uint8_t model;
    uint8_t moduleAddr;
    uint8_t upstreamPortState;
    uint8_t upstreamPortMode;
    uint8_t boostUpstream;
    uint32_t enumDelay;
    uint8_t boostDownstream;
    uint32_t uptime;

    QString portAndSystemNames[9];

    void getLinkSpec(linkSpec* spec);

    //QTimer pollingTimer;
    void connectStemWithDialog();

    list<linkSpec> devicesDiscovered;

    // per port parts
    void updatePortVoltageAndCurrent();
    void updateHubMode();
    void updatedHubState();
    void updateHubErrorStatus();
    void updateCurrentLimit();
    void updatePortMode();

    // system parts
    void updateTemperature();
    void updateInputVoltage();
    void updateInputVoltageCurrent();
    void updateUserLed();
    void updateStemInfo();

    // upstream parts
    void updateUpstreamPort();
    void updateUpstreamMode();
    void updateUpstreamBoost();

    // downstream parts
    void updateEnumerationDelay();
    void updateDownstreamBoost();

    void updateUptime();

    void initializeStem(linkSpec* spec);

    void getPortNames();
    void setDefaultNames();
    void parseStoredNameData(char* slotData);
    void parseStoredNameData(QString slotData);
    void setPortAndSystemNames();

#ifdef __APPLE__
    AppNapSuspender napper;
#endif
};

#endif // STEMWORKER_H
