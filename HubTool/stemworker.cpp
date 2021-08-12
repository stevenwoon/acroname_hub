#include "stemworker.h"
#include <QDebug>

#include "BrainStem2/aUSBHub3p.h"
#include "BrainStem2/aUSBHub2x4.h"

#define DEMO_AS_USBHUB3P 1
#define SLOT_FOR_NAMES 10

StemWorker::StemWorker(linkSpec* spec) :
    module(0),
    numUSB(0),
    connectedModel(0),
    firmwareVersion(0),
    connectRetryCount(0),
    firstPollingEvent(true),
    hubMode(0xFFFFFFFF),
    portState{0,0,0,0,0,0,0,0},
    firmwareUpdateMessageFlag_HubState(true),
    portError{0,0,0,0,0,0,0,0},
    firmwareUpdateMessageFlag_HubError(true),
    portMode{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    temperature(0xFFFFFFFF),
    maxTemperature(0xFFFFFFFF),
    firmwareUpdateMessageFlag_Temperature(true),
    voltage(0xFFFFFFFF),
    current(0xFFFFFFFF),
    ledState(0xFF),
    serialNumber(0xFFFFFFFF),
    model(0xFF),
    moduleAddr(0xFF),
    upstreamPortState(0xFF),
    upstreamPortMode(0xFF),
    boostUpstream(0xFF),
    enumDelay(0xFFFFFFFF),
    boostDownstream(0xFF),
    uptime(0)
{

    if(spec) {
        stemToolSpec = *spec;
    }
    else { stemToolSpec.serial_num = 0; }
}

void StemWorker::start(){
    #ifdef __APPLE__
        napper.suspend();
    #endif
    if(stemToolSpec.serial_num != 0)    { initializeStem(&stemToolSpec);    }
    else                                { connectStemWithDialog();          }
}

StemWorker::~StemWorker() {
    #ifdef __APPLE__
        napper.resume();
    #endif
    qDebug() << "cleanin up from stemworker";

    // Disconnect from the module
    module.disconnect();
}

static bContinueSearch sFindAllHubs(const linkSpec* spec, bool* bSuccess, void* vpCBRef) {

    list<linkSpec>* pSpecs = (std::list<linkSpec>*)vpCBRef;
    if (spec->model == aMODULE_TYPE_USBHub2x4 || spec->model == aMODULE_TYPE_USBHub3p) {
        pSpecs->push_back(*spec);
        *bSuccess = true;
    }
    return true;
}


void StemWorker::connectStemWithDialog() {
    linkSpec spec;

    // look for devices on USB
    devicesDiscovered.clear();
    Link::sDiscover(USB, sFindAllHubs, &devicesDiscovered);

    switch (devicesDiscovered.size()){
    case 0:
        emit(requestMsgBox(QString("No devices found.\nRunning in demo mode.")));
        break;
    case 1:
        // One device found. Connect to it.
        spec = *devicesDiscovered.begin();
        initializeStem(&spec);
        break;
    default:
        // More than one device found. Need the user to pick one.

        // make a strlist of serial numbers to choose from
        QStringList availableSerialNumbers;
        for (std::list<linkSpec>::iterator it=devicesDiscovered.begin(); it != devicesDiscovered.end(); ++it){
            availableSerialNumbers << QString("0x%1").arg(it->serial_num, 8, 16, QChar('0')).toUpper();
        }

        emit selectStemFromListSignal(availableSerialNumbers);

        break;
    }
}

void StemWorker::initializeStem(linkSpec* spec) {
    aErr err = aErrNone;

    switch(spec->model) {
    case aMODULE_TYPE_USBHub2x4:
        module.setModuleAddress(aUSBHUB2X4_MODULE);
        connectedModel = aMODULE_TYPE_USBHub2x4;
        numUSB = 4;
        break;
    case aMODULE_TYPE_USBHub3p:
        module.setModuleAddress(aUSBHUB3P_MODULE);
        connectedModel = aMODULE_TYPE_USBHub3p;
        numUSB = 8;
        break;
    default:
        //Not a hub
        if(DEMO_AS_USBHUB3P){
            emit logStringReady("No link found. Running in demo mode as USBHub3+.");
            connectedModel = aMODULE_TYPE_USBHub3p;
            numUSB = 8;
        }
        else{
            emit logStringReady("No link found. Running in demo mode as USBHub2x4+.");
            connectedModel = aMODULE_TYPE_USBHub2x4;
            numUSB = 4;
        }
        return;
    }

    err = module.connectFromSpec(*spec);

    // Check that the connection was successful
    if (err != aErrNone){
        emit logStringReady(QString("Error connecting to device: %1").arg(err));
        return;
    }
    else{
        if(connectedModel == aMODULE_TYPE_USBHub2x4) {
            emit logStringReady("Successfully connected to USBHub2x4");
        }
        else { emit logStringReady("Successfully connected to USBHub3+"); }

        emit Sig_Secondary_GUI_Init();
    }

    //Initialize items entities manually.
    system.init(&module, 0);
    store[0].init(&module, storeInternalStore);
    store[1].init(&module, storeRAMStore);
    usb.init(&module, 0);
    temp.init(&module, 0);

}

void StemWorker::connectUserChosenStem(QString stemSerialNumber) {

    for (std::list<linkSpec>::iterator it=devicesDiscovered.begin(); it != devicesDiscovered.end(); ++it){
        if (stemSerialNumber == QString("0x%1").arg(it->serial_num, 8, 16, QChar('0')).toUpper()){
            firstPollingEvent = true; //Will trigger fetching basic information that is only fetched once.
            initializeStem(&(*it));
            break;
        }
    }
}

void StemWorker::pollStemForChanges(){
    linkSpec currentLinkSpec;
    aErr err = aErrNone;
    err = module.getLinkSpecifier(&currentLinkSpec);

    //check if we're connected to the stem and reconnect if we aren't
    // if we're in demo mode, the currentLink will be NULL, so no reconnect
    // if the link has dropped, linkSpec will be INVALID
    if(err == aErrNone
       && currentLinkSpec.type != INVALID
       && !module.isConnected() )
    {
        if(connectRetryCount == 0){
            // periodically print a reconnect message using the roll over of the int8
            emit logStringReady(QString("Lost link. Trying to reconnect."));
        }

        // tear down the old link to be sure we're clean
        module.disconnect();

        // try to reconnect to the stem
        module.reconnect();

        if(module.isConnected()){
            firstPollingEvent = true;
            emit logStringReady(QString("Reconnected to %1").arg(QString("0x%1").arg(currentLinkSpec.serial_num, 8, 16, QChar('0')).toUpper()));
            emit Sig_Secondary_GUI_Init();
        }

        //QThread::msleep(100);
        connectRetryCount++;
        emit finishedPolling();
        return;
    }

    // reset the reconnection and clean up
    connectRetryCount = 0;

    // system parts
    if(firstPollingEvent){
        updateStemInfo();
        // try to read the port names
        getPortNames();
        firstPollingEvent = false;
    }
    updateTemperature();
    if(connectedModel == aMODULE_TYPE_USBHub2x4)
        updateInputVoltage();
    else
        updateInputVoltageCurrent();
    updateUserLed();

    // per port parts
    updatePortVoltageAndCurrent();
    updateHubMode();
    updatedHubState();
    updateHubErrorStatus();
    updateCurrentLimit();
    updatePortMode();

    // upstream parts
    updateUpstreamPort();
    updateUpstreamMode();
    updateUpstreamBoost();

    // downstream parts
    updateEnumerationDelay();
    updateDownstreamBoost();

    //uptime
    updateUptime();

    emit finishedPolling();
}


// ///////////////////////////////////////////////////////////////////////////////////
// per port parts
// ///////////////////////////////////////////////////////////////////////////////////
void StemWorker::updatePortVoltageAndCurrent(){
    aErr err = aErrNone;

    // check for connnection
    bool stemConnected = module.isConnected();


    // read each port's voltage
    for (uint8_t channel = 0; channel < numUSB; channel++){
        int32_t newVoltage = 0;
        int32_t newAmps = 0;
        if(stemConnected){
            // To make sure things stay in sync, drain any UEI packets of this request
            usb.drainUEI(usbPortVoltage);
            err = usb.getPortVoltage(channel, &newVoltage);
            if (err != aErrNone){
                emit logStringReady(QString("Error updating port voltage %1. Err: %2").arg(channel).arg(err));
                return;
            } // aErrNone

            usb.drainUEI(usbPortCurrent);
            err = usb.getPortCurrent(channel, &newAmps);
            //qDebug("amps: %d, channel: %d", newAmps, channel);
            if (err != aErrNone){
                emit logStringReady(QString("Error updating port current %1. Err: %2").arg(channel).arg(err));
                return;
            } // aErrNone

        } // stem connected
        else {
            newVoltage = rand() % 1000000 + 4500000;
            newAmps = rand() % 2900000;
        }

        // always emit so the plots are smooth as can be
        emit portVoltageCurrentChanged(channel, newVoltage, newAmps);
    } // for channel
}

void StemWorker::updateHubMode(){
    uint32_t newMode = 0;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module

    if(module.isConnected()){
        // To make sure things stay in sync, drain any UEI packets of this request
        usb.drainUEI(usbHubMode);
        err = usb.getHubMode(&newMode);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating hub mode %1").arg(err));
            return;
        }
    }
    // if we don't have a link, fake it
    else {
        newMode = rand() % 0xFFFFFFFF;
    }

    // if the value changed, store it and emit update signal
    if(newMode != hubMode){
        hubMode = newMode;
        emit hubModeChanged(newMode);
    }
}

void StemWorker::updatedHubState(){

    uint32_t newPortState[8] = {0,0,0,0,0,0,0,0};
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        if(firmwareVersion <  0x25000000){
            if(firmwareUpdateMessageFlag_HubState){
                emit logStringReady(QString("getPortState is deprecated. Firmware update available."));
            }
            firmwareUpdateMessageFlag_HubState = false;
        }
        else {
            // To make sure things stay in sync, drain any UEI packets of this request.
            usb.drainUEI(usbPortState);
            for (uint8_t i = 0; i < numUSB; i++) {
                err = usb.getPortState(i, &newPortState[i]);
                if (err != aErrNone){
                    emit logStringReady(QString("Error updating port status port %1 %2").arg(i).arg(err));
                    return;
                }
            }
        }
    }
    // if we don't have a link, fake it
    else {
        for (uint8_t i=0; i < numUSB; i++) {
            newPortState[i] = rand() % 0xFFFFFFFF;
        }
    }

    const int usbDeviceAttached=_BIT(aUSBHUB3P_DEVICE_ATTACHED);
    const int usbConstantCurrent=_BIT(aUSBHUB2X4_CONSTANT_CURRENT);
    const int usbError=_BIT(aUSBHUB3P_USB_ERROR_FLAG);
    const int usbHiSpeed=_BIT(aUSBHUB3P_USB_SPEED_USB2);
    const int usbSSpeed=_BIT(aUSBHUB3P_USB_SPEED_USB3);

    for (int channel = 0; channel < numUSB; channel++){
        // decode the resulting state field
        QString str;
        int8_t spd;
        str = " ";
        if (portState[channel] != newPortState[channel]) {
            portState[channel] = newPortState[channel];
            str += (portState[channel] & usbDeviceAttached) ? "ATT ": "";
            str += (portState[channel] & usbConstantCurrent) ? "CC ": "";
            str += (portState[channel] & usbError) ? "ERR ": "";
            if (portState[channel] & usbDeviceAttached) {
                if (portState[channel] & usbHiSpeed) {
                    spd = usbDownstreamDataSpeed_hs;
                }
                if (portState[channel] & usbSSpeed) {
                    spd = usbDownstreamDataSpeed_ss;
                }
            } else {
                spd = -1;
            }
            emit hubStateChanged(channel, str, spd);
        }
    } // for channel
}

void StemWorker::updateHubErrorStatus(){

    linkSpec spec;
    module.getLinkSpecifier(&spec);

    uint32_t newPortError[8] = {0,0,0,0,0,0,0,0};
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        if(firmwareVersion <  0x25000000){
            if(firmwareUpdateMessageFlag_HubError){
                emit logStringReady(QString("getPortError is deprecated. Firmware update available."));
            }
            firmwareUpdateMessageFlag_HubError = false;
        }
        else {
            // To make sure things stay in sync, drain any UEI packets of this request.
            usb.drainUEI(usbPortError);
            for (uint8_t i = 0; i < numUSB; i++) {
                err = usb.getPortError(i, &newPortError[i]);
                if (err != aErrNone){
                    emit logStringReady(QString("Error updating port errors for port %1 %2").arg(i).arg(err));
                    return;
                }
            }
        }
    }

    // TODO: not sure why we're skipping nibbles
    const int usb_error_over_ilim = _BIT(aUSBHUB3P_ERROR_VBUS_OVERCURRENT);
    const int usb_error_back_volt = _BIT(aUSBHUB3P_ERROR_VBUS_BACKDRIVE);
    const int usb_error_hub_power = _BIT(aUSBHUB3P_ERROR_HUB_POWER);
    const int usb_error_discharge_err= _BIT(aUSBHub2X4_ERROR_DISCHARGE);

    for (int channel = 0; channel < numUSB; channel++){
        // decode the resulting state field
        QString str;
        if (portError[channel] != newPortError[channel]) {
            portError[channel] = newPortError[channel];
            str = (portError[channel] & usb_error_over_ilim)? "OVER_ILIM " : "";
            str += (portError[channel] & usb_error_back_volt) ? "BACK_VOLT " : "";
            str += (portError[channel] & usb_error_hub_power) ? "OVER_VOLT " : "";
            str += (portError[channel] & usb_error_discharge_err) ? "DISCHARGE_ERR " : "";
            emit hubErrorStatusChanged(channel, str);
        }
    } // for channel
}

void StemWorker::updateCurrentLimit(){
    uint32_t newCurrentLimit;
    bool stemConnected = false;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    stemConnected = module.isConnected();

    for (int channel=0; channel < numUSB; channel++){
        if(stemConnected){
            // To make sure things stay in sync, drain any UEI packets of this request
            usb.drainUEI(usbPortCurrentLimit);
            // Get the port current limit
            err = usb.getPortCurrentLimit(channel, &newCurrentLimit);

            if (err != aErrNone){
                emit logStringReady(QString("Error getting ch%1 port current limit %2").arg(channel).arg(err));
            }
        } // stem connected
        // otherwise, just show the max current limit
        else {
            newCurrentLimit = rand() % 2500000;
        }

        // if the value changed, store it and emit update signal
        if(newCurrentLimit != currentLimit[channel]){
            currentLimit[channel] = newCurrentLimit;
            emit portCurrentLimitChanged(channel, newCurrentLimit);
        }
        //emit logStringReady(QString("CurrentLimit port %1 %2").arg(channel).arg(newCurrentLimit));
    } // for channel
}

void StemWorker::updatePortMode(){
    uint32_t newMode;
    bool stemConnected = false;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    stemConnected = module.isConnected();

    for (int channel=0; channel < numUSB; channel++){
        if(stemConnected){
            // To make sure things stay in sync, drain any UEI packets of this request
            usb.drainUEI(usbHubMode);
            // Get the port current limit
            err = usb.getPortMode(channel, &newMode);

            if (err != aErrNone){
                emit logStringReady(QString("Error getting port Mode %1").arg(err));
            }
        } // stem connected
        // otherwise, just show random toggles
        else {
           newMode = rand() % 2;
        }

        // if the value changed, store it and emit update signal
        if(newMode != portMode[channel]){
            portMode[channel] = newMode;
            emit portModeChanged(channel, newMode);
        }
    } // for channel
}


// ///////////////////////////////////////////////////////////////////////////////////
// system parts
// ///////////////////////////////////////////////////////////////////////////////////
void StemWorker::updateTemperature(){
    int32_t newTemperature = 0;
    int32_t newMaxTemperature = 0;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        if(firmwareVersion >=  0x25000000){
//            err = temp.getTemperature(&newTemperature);
            err = temp.getValue(&newTemperature);
        }
        else {
            if(firmwareUpdateMessageFlag_Temperature){
                emit logStringReady(QString("getSystemTemperature is deprecated. Firmware update available."));
            }
            firmwareUpdateMessageFlag_Temperature = false;
        }
        if (err != aErrNone){
            emit logStringReady(QString("Error updating temperature %1").arg(err));
            return;
        }

        // if the FW version supports maxTemp, query for it
        if(firmwareVersion >=  0x25000000 && connectedModel == aMODULE_TYPE_USBHub3p){
            err = system.getMaximumTemperature(&newMaxTemperature);
            if (err != aErrNone){
                emit logStringReady(QString("Error updating max temperature %1").arg(err));
                return;
            }
        }
    }
    // if we don't have a link, fake it
    else {
        newTemperature = rand() % 150000000;
        newMaxTemperature = 150000000;
    }

    // if the value changed, store it and emit update signal
    if(newTemperature != temperature || newMaxTemperature != maxTemperature){
        temperature = newTemperature;
        maxTemperature = newMaxTemperature;

        // never print something higher than 200C
        maxTemperature = maxTemperature/1.0e6 > 200 ? 200 : maxTemperature;
        temperature = temperature/1.0e6 > 200 ? 200 : temperature;

        QString maxTempStr, tempStr;
        maxTempStr.sprintf("%.1f", maxTemperature/1.0e6);
        tempStr.sprintf("%.1f", temperature/1.0e6);
        if(firmwareVersion >=  0x25000000 && connectedModel == aMODULE_TYPE_USBHub3p){
            emit temperatureChanged(QString("%1˚C (max: %2˚C)").arg(tempStr, maxTempStr));
        }
        else {
            emit temperatureChanged(QString("%1˚C").arg(tempStr));
        }
    }
}

void StemWorker::updateInputVoltage(){
    uint32_t newVoltage = 0;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        err = system.getInputVoltage(&newVoltage);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating system input voltage %1").arg(err));
            return;
        }
    }
    // if we don't have a link, fake it
    else {
        newVoltage = rand() % 24000000;
    }

    // if the value changed, store it and emit update signal
    if(newVoltage != voltage){
        voltage = newVoltage;
        emit inputVoltageChanged(newVoltage);
    }
}

// used by the USBHub3+ to provide current and voltage data to the UI
void StemWorker::updateInputVoltageCurrent(){
    uint32_t newCurrent = 0;
    uint32_t newVoltage = 0;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        err = system.getInputCurrent(&newCurrent);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating system input current %1").arg(err));
            return;
        }
        err = system.getInputVoltage(&newVoltage);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating system input voltage %1").arg(err));
            return;
        }
    }
    // if we don't have a link, fake it
    else {
        newCurrent = rand() % 10000000;
        newVoltage = rand() % 24000000;
    }

    // if the value changed, store it and emit update signal
    if(newCurrent != current || newVoltage != voltage){
        current = newCurrent;
        voltage = newVoltage;
        emit inputVoltageCurrentChanged(newVoltage, newCurrent);
    }
}

void StemWorker::updateUserLed(){
    uint8_t newLedState = 0;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        err = system.getLED(&newLedState);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating user LED state %1").arg(err));
            return;
        }
    }
    // if we don't have a link, fake it
    else {
        newLedState = rand() % 2;
    }

    // if the value changed, store it and emit update signal
    if(newLedState != ledState){
        ledState = newLedState;
        emit userLedChanged(newLedState);
    }
}

void StemWorker::updateStemInfo(){
    firmwareVersion = 0xFFFFFFFF; // in the stemworker class so other functions can use this
    uint32_t newSerialNumber = 0;
    uint8_t newModel = 0;
    uint32_t newFirmwareVersion = 0;
    uint8_t newModuleAddr = 0;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        err = system.getSerialNumber(&newSerialNumber);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating BrainStem info: serial number %1").arg(err));
            return;
        }

        err = system.getModel(&newModel);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating BrainStem info: model %1").arg(err));
            return;
        }

        err = system.getVersion(&newFirmwareVersion);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating BrainStem info: firmware version %1").arg(err));
            return;
        }

        err = system.getModule(&newModuleAddr);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating BrainStem info: system address %1").arg(err));
            return;
        }

    } // if module connected

    // if we don't have a link, fake it
    else {
        newSerialNumber = 0xDEAD0123;
        newModel = 255;
        newFirmwareVersion = 0xABCD1234;
        newModuleAddr = 0;
    }

    // if the value changed, store it and emit update signal
    if(   newSerialNumber != serialNumber
       || newModel != model
       || newFirmwareVersion != firmwareVersion
       || newModuleAddr != moduleAddr)
    {
        serialNumber = newSerialNumber;
        model = newModel;
        firmwareVersion = newFirmwareVersion;
        moduleAddr = newModuleAddr;

        // look up the model string
        QString modelStr = "";
        switch(newModel){
        case(aMODULE_TYPE_USBHub2x4):
            modelStr="USBHub2x4"; break;
        case(aMODULE_TYPE_USBHub3p):
            modelStr="USBHub3p"; break;
        default:
            modelStr = "Unknown"; break;
        }

        emit stemInfoChanged(newSerialNumber, modelStr, newFirmwareVersion);
    } // if something changed
}


// ///////////////////////////////////////////////////////////////////////////////////
// upstream parts
// ///////////////////////////////////////////////////////////////////////////////////
void StemWorker::updateUpstreamPort(){
    uint8_t newUpstreamPortState = 0;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        err = usb.getUpstreamState(&newUpstreamPortState);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating upstream state %1").arg(err));
            return;
        }
    }
    // if we don't have a link, fake it
    else {
        newUpstreamPortState = rand() % 2;
    }

    // if the value changed, store it and emit update signal
    if(newUpstreamPortState != upstreamPortState){
        upstreamPortState = newUpstreamPortState;

        QString newUpstreamPortStateStr = "";
        switch(newUpstreamPortState){
            case usbUpstreamStateNone:
                newUpstreamPortStateStr = "None";
                break;
            case usbUpstreamStatePort0:
                newUpstreamPortStateStr = "Port 0";
                break;
            case usbUpstreamStatePort1:
                newUpstreamPortStateStr = "Port 1";
                break;
            default:
                newUpstreamPortStateStr = "Unknown";
        } // case upstreamPortState

        emit upstreamPortChanged(newUpstreamPortStateStr);
    }
}

void StemWorker::updateUpstreamMode(){
    uint8_t newUpstreamPortMode = 0;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        err = usb.getUpstreamMode(&newUpstreamPortMode);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating upstream mode %1").arg(err));
            return;
        }
    }
    // if we don't have a link, fake it
    else {
        newUpstreamPortMode = rand() % 2;
    }

    // if the value changed, store it and emit update signal
    if(newUpstreamPortMode != upstreamPortMode){
        upstreamPortMode = newUpstreamPortMode;

        QString newUpstreamPortModeStr = "";
        switch(newUpstreamPortMode){
            case usbUpstreamModeAuto:
                newUpstreamPortModeStr = "Auto";
                break;
            case usbUpstreamModePort0:
                newUpstreamPortModeStr = "Port 0";
                break;
            case usbUpstreamModePort1:
                newUpstreamPortModeStr = "Port 1";
                break;
            default:
                newUpstreamPortModeStr = "Unknown";
        } // case upstreamPortState

        //emit logStringReady(QString("Hub upstream mode changed to %1").arg(newUpstreamPortModeStr));
        emit upstreamModeChanged(newUpstreamPortMode);
    }
}

void StemWorker::updateUpstreamBoost(){
    uint8_t newBoost = 0;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        err = usb.getUpstreamBoostMode(&newBoost);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating upstream boost %1").arg(err));
            return;
        }
    }
    // if we don't have a link, fake it
    else {
        newBoost = (rand() % 3);
    }

    // if the value changed, store it and emit update signal
    if(newBoost != boostUpstream){
        boostUpstream = newBoost;

        emit upstreamBoostChanged(newBoost);
    }
}


// ///////////////////////////////////////////////////////////////////////////////////
// downstream parts
// ///////////////////////////////////////////////////////////////////////////////////
void StemWorker::updateEnumerationDelay(){
    uint32_t newEnumDelay = 0;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        err = usb.getEnumerationDelay(&newEnumDelay);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating enumeration delay %1").arg(err));
            return;
        }
    }
    // if we don't have a link, fake it
    else {
        newEnumDelay = (rand() % 10) * 100;
    }

    // if the value changed, store it and emit update signal
    if(newEnumDelay != enumDelay){
        enumDelay = newEnumDelay;

        emit enumerationDelayChanged(newEnumDelay);
    }
}

void StemWorker::updateDownstreamBoost(){
    uint8_t newBoost = 0;
    aErr err = aErrNone;

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        err = usb.getDownstreamBoostMode(&newBoost);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating downstream boost %1").arg(err));
            return;
        }
    }
    // if we don't have a link, fake it
    else {
        newBoost = (rand() % 3);
    }

    // if the value changed, store it and emit update signal
    if(newBoost != boostDownstream){
        boostDownstream = newBoost;

        emit downstreamBoostChanged(newBoost);
    }
}

void StemWorker::updateUptime(){
    aErr err = aErrNone;
    uint32_t newUptime=0;

    if(firmwareVersion <  0x25000000 || connectedModel != aMODULE_TYPE_USBHub3p){
        // uptime isn't support, so bail
        emit uptimeChanged(QString("Not Supported"));
        return;
    }

    // if we have a link, then get the data from the connected module
    if(module.isConnected()){
        err = system.getUptime(&newUptime);
        if (err != aErrNone){
            emit logStringReady(QString("Error updating uptime %1").arg(err));
            return;
        }
    }
    // if we don't have a link, fake it
    else {
        newUptime = (rand() % 60);
    }

    // if the value changed, store it and emit update signal
    if(newUptime != uptime){
        uptime = newUptime;

        // format the string for display
        emit uptimeChanged(QString("%1h%2m").arg(uptime/60).arg((int)(uptime%60)));
    }
}


// ///////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////////////////////////////////////////////////
// Slots for handling changes made by the user in the UI
// these need to signal down to the HW to make the change requested
// ///////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////////////////////////////////////////////////
// system parts
void StemWorker::changeUserLed(bool enabled){
    aErr err = aErrNone;
    qDebug("changeUserLed");
    if(module.isConnected()){
        err = system.setLED(enabled);

        if (err != aErrNone){
            emit logStringReady(QString("Error changing user LED state: %1").arg(err));
            return;
        } // if err
    } // if connected
}

void StemWorker::saveState(void){
    aErr err = aErrNone;
    qDebug("saveState");
    if(module.isConnected()){
        err = system.save();

        if (err != aErrNone){
            emit logStringReady(QString("Error saving system state: %1").arg(err));
            return;
        } // if err
    } // if connected
}

void StemWorker::resetDevice(void){
    aErr err = aErrNone;
    qDebug("saveState");
    if(module.isConnected()){
        err = system.reset();

        if (err == aErrTimeout){
           emit logStringReady(QString("Successful reset (timeout received)"));
        }
        else {
            emit logStringReady(QString("Error resetting hub: %1").arg(err));
            return;
        } // if err
    } // if connected

}

void StemWorker::showEventLogs(void){
    aErr err = aErrNone;
    enum aBSLogEventId{
        aBSLogEvent_noEvent,            // 0
        aBSLogEvent_brownoutReset,      // 1
        aBSLogEvent_watchdogReset,      // 2
        aBSLogEvent_externalReset,      // 3
        aBSLogEvent_hardReset,          // 4
        aBSLogEvent_storeVersionChanged,// 5
        aBSLogEvent_firmwareUpdate,     // 6
        aBSLogEvent_systemParamSave,    // 7
        aBSLogEvent_boot,               // 8
        aBSLogEvent_unknownEvent,       // x as last real entry
    };
    // Human readable translation for each enum
    const char *logEventMessage[] = {
        "no event",
        "brownout reset",
        "watchdog reset",
        "external reset",
        "hard reset",
        "store version changed",
        "firmware update",
        "system param saved",
        "boot",
        "unknown event"
    };

    qDebug("showEventLogs");
    if(module.isConnected()){
        // event logging is only on USBHub3p
        if (connectedModel == aMODULE_TYPE_USBHub2x4) {
            emit logStringReady(QString("USBHub2x4 does not support event logs"));
        }

        err = system.logEvents();
        if (err != aErrNone){
            emit logStringReady(QString("Error logging events: %1").arg(err));
            return;
        }

        // get the current size of the data in the RAM slot
        size_t slotSizeVal=2; // arbitrary initial value
        store[storeRAMStore].getSlotSize(0, &slotSizeVal);
        emit logStringReady(QString("Showing hub event logs (log size: %1)").arg(slotSizeVal));

        // create an array to read out the logs from the RAM slot
        uint8_t* logs =  new uint8_t[slotSizeVal];
        size_t sizeOfUnloadedData=0;

        // read back and print the raw data in the RAM slot
        err = store[storeRAMStore].unloadSlot(0, slotSizeVal, logs, &sizeOfUnloadedData);
        if (err != aErrNone){
            emit logStringReady(QString("Error unloading slot: %1").arg(err));
            return;
        }

        // decode the logs:
        // basic format is 32b timestamp followed by event ID enum
        QString eventLogMessageLines;
        eventLogMessageLines += "\n";
        for(int i=0; i<(int)sizeOfUnloadedData; i+=5){
            // decode the event id enum into a string
            // protect from walking off the end of the string array
            if (logs[i+4] > aBSLogEvent_unknownEvent){
                logs[i+4] = aBSLogEvent_unknownEvent;
            }

            int timeStamp = (int32_t)((logs[i+3]<<24) + (logs[i+2]<<16) + (logs[i+1]<<8) + logs[i]);
            eventLogMessageLines += QString::number(i); //.rightJustified(3, ' ');
            eventLogMessageLines += ": time: " + QString::number(timeStamp); //.rightJustified(12, ' ');
            eventLogMessageLines += " e: " + QString(logEventMessage[logs[i+4]]);
            eventLogMessageLines += "\n";
        }
        delete [] logs;
        emit logStringReady(eventLogMessageLines);
    } // if connected
}

// per port parts
void StemWorker::changeUSBPortDataState(int channel, bool checked){
    aErr err = aErrNone;
    qDebug("changeUSBPortDataState");
    if(module.isConnected()){
        if(checked)
            err = usb.setDataEnable(channel);
        else
            err = usb.setDataDisable(channel);

        if (err != aErrNone){
            emit logStringReady(QString("Error changing port %1 data: %2").arg(channel).arg(err));
            return;
        } // if err
    } // if connected
}

void StemWorker::Slot_userChangedUSBDataState_SS(int channel, bool checked){
    aErr err = aErrNone;
    uint8_t model;
    getConnectedModel(&model);

    qDebug("changeUSBPortDataStateSS");
    if(module.isConnected() && (model == aMODULE_TYPE_USBHub3p)){
        if(checked) {
            err = usb.setSuperSpeedDataEnable(channel);
            emit Sig_HandleDataSpeed(channel, 2);
        }
        else {
            err = usb.setSuperSpeedDataDisable(channel);
            emit Sig_HandleDataSpeed(channel, 1);
        }

        if (err != aErrNone){
            emit logStringReady(QString("Error changing port %1 Super Speed data : %2").arg(channel).arg(err));
            return;
        } // if err
    } // if connected
}

void StemWorker::Slot_userChangedUSBDataState_HS(int channel, bool checked){
    aErr err = aErrNone;
    uint8_t model=0;
    getConnectedModel(&model);

    qDebug("changeUSBPortDataStateHS");
    if(module.isConnected() && (model == aMODULE_TYPE_USBHub3p)){
        if(checked) {
            err = usb.setHiSpeedDataEnable(channel);
            emit Sig_HandleDataSpeed(channel, 2);
        }
        else {
            err = usb.setHiSpeedDataDisable(channel);
            emit Sig_HandleDataSpeed(channel, 1);
        }

        if (err != aErrNone){
            emit logStringReady(QString("Error changing port %1 High Speed data : %2").arg(channel).arg(err));
            return;
        } // if err
    } // if connected
}

void StemWorker::changeUSBPortPowerState(int channel, bool checked){
    aErr err = aErrNone;
    qDebug("changeUSBPortPowerState");
    if(module.isConnected()){
        if(checked)
            err = usb.setPowerEnable(channel);
        else
            err = usb.setPowerDisable(channel);

        if (err != aErrNone){
            emit logStringReady(QString("Error changing port %1 power: %2").arg(channel).arg(err));
            return;
        } // if err
    } // if connected
}

void StemWorker::clearPortError(int channel){
    aErr err = aErrNone;
    qDebug("clearPortError");
    if(module.isConnected()){
        err = usb.clearPortErrorStatus(channel);

        if (err != aErrNone){
            emit logStringReady(QString("Error changing port %1 power: %2").arg(channel).arg(err));
            return;
        } // if err
    } // if connected
}

void StemWorker::changeUSBPortCurrentLimit(int channel, uint32_t limit){
    aErr err = aErrNone;
    qDebug("changeUSBPortCurrentLimit");
    currentLimit[channel] = limit;
    if(module.isConnected()){
        err = usb.setPortCurrentLimit(channel, limit);

        if (err != aErrNone){
            emit logStringReady(QString("Error changing port %1 current limit: %2").arg(channel).arg(err));
            return;
        } // if err
    } // if connected
}

void StemWorker::changeUSBPortMode(int channel, bool enabled) {
    aErr err = aErrNone;
    qDebug("changeUSBPortMode");
    if(module.isConnected()) {
        err = usb.setPortMode(channel, (enabled)? usbPortMode_cdp: usbPortMode_sdp);
        if (err != aErrNone) {
            emit logStringReady(QString("Error changing port %1 port mode: %2").arg(channel).arg(err));
            return;
        }
    }
}

void StemWorker::changeUSBPortEnableState(int channel, bool enabled) {
    aErr err = aErrNone;
    qDebug("changeUSBPortEnableState");
    if(module.isConnected()) {
        if(enabled){
            err = usb.setPortEnable(channel);
        }
        else {
            err = usb.setPortDisable(channel);
        }
        if (err != aErrNone) {
            emit logStringReady(QString("Error changing port %1 port state: %2").arg(channel).arg(err));
            return;
        }
    }
}



// ///////////////////////////////////////////////////////////////////////////////////

// upstream parts
void StemWorker::changeUpstreamMode(int mode){
    aErr err = aErrNone;
    qDebug("changeUpstreamMode");
    if(module.isConnected()){
        err = usb.setUpstreamMode(mode);

        if (err != aErrNone){
            emit logStringReady(QString("Error changing upstream mode: %1").arg(err));
            return;
        } // if err
    } // if connected
}

void StemWorker::changeUpstreamBoost(uint8_t boost){
    aErr err = aErrNone;
    qDebug() << "changeUpstreamBoost" << boost;
    if(module.isConnected()){
        err = usb.setUpstreamBoostMode(boost);

        if (err != aErrNone){
            emit logStringReady(QString("Error changing upstream boost: %1").arg(err));
            return;
        } // if err
    } // if connected
}


// ///////////////////////////////////////////////////////////////////////////////////

// downstream parts
void StemWorker::changeEnumerationDelay(uint32_t ms_delay){
    aErr err = aErrNone;
    qDebug("changeEnumerationDelay");
    if(module.isConnected()){
        err = usb.setEnumerationDelay(ms_delay);

        if (err != aErrNone){
            emit logStringReady(QString("Error changing enumeration delay: %1").arg(err));
            return;
        } // if err

    } // if connected
}

void StemWorker::changeDownstreamBoost(uint8_t boost){
    aErr err = aErrNone;
    qDebug("changeDownstreamBoost");
    if(module.isConnected()){
        err = usb.setDownstreamBoostMode(boost);

        if (err != aErrNone){
            emit logStringReady(QString("Error changing downstream boost: %1").arg(err));
            return;
        } // if err
    } // if connected
}


// accessor function to tell others what model is connected (or simulated)
void StemWorker::getConnectedModel(uint8_t *model) {
    if(model)
        *model = connectedModel;
}


// handle changing port and system names
void StemWorker::changePortName(QString name, int index){
    // store local mirror of port name
    qDebug() << "Saving port names triggered by " << index;
    if(index < 9){
        portAndSystemNames[index] = name;
    }

    // save to the connected device
    setPortAndSystemNames();

}

void StemWorker::changeSystemName(QString name){
    changePortName(name, 8);
}

void StemWorker::setDefaultNames(){
    // set some default values
    for(int i=0; i<8; i++){
        portAndSystemNames[i] = "";
    }
    if(connectedModel == aMODULE_TYPE_USBHub2x4){
        portAndSystemNames[8] = "";
    } else {
        portAndSystemNames[8] = "";
    }
}

void StemWorker::getPortNames(){
    // always just set some reasonable defaults
    setDefaultNames();

    if(module.isConnected()){
        aErr err = aErrNone;
        size_t currentSlotSize = 0;
        size_t unloadedSize = 0;
        char *slotData = NULL;


        //read the flash slot 11 length
        err = store[storeInternalStore].getSlotSize(SLOT_FOR_NAMES, &currentSlotSize);
        if (err != aErrNone){
            emit logStringReady(QString("Error reading store size: %1").arg(err));
            return;
        }

        // if it's 0, fall back to defaults
        if(currentSlotSize < 6){
            //emit(logStringReady(QString("No store names found on hub (size %1)").arg(currentSlotSize)));
            return;
        }

        // allocate space for the data in the slot
        qDebug() << "Allocating" << currentSlotSize << "for store data";
        slotData = (char*) calloc(currentSlotSize, sizeof(char));

        // read back the slot
        err = store[storeInternalStore].unloadSlot(SLOT_FOR_NAMES, currentSlotSize, (uint8_t*)slotData, &unloadedSize);
        if(err != aErrNone){
            emit logStringReady(QString("Error reading name data from store: %1").arg(err));
            if(slotData) free(slotData);
            slotData = NULL;
            return;
        }
        if(currentSlotSize != unloadedSize){
            emit logStringReady(QString("Got wrong size back when reading store: got %1; expected %2").arg(unloadedSize).arg(currentSlotSize));
            if(slotData) free(slotData);
            slotData = NULL;
            return;
        }

        //emit(logStringReady(QString("Read slot length %1/%2").arg(unloadedSize).arg(currentSlotSize)));
        //emit(logStringReady(QString::fromLocal8Bit(slotData)));

        //parseStoredNameData(slotData);
        parseStoredNameData(QString::fromLocal8Bit(slotData));

        if(slotData) free(slotData);
        slotData = NULL;

    } // if module connected
}

void StemWorker::parseStoredNameData(char* slotData){
    char* pch = nullptr;
    // parse the slot contents for format:
    // NAMES\n
    // entity.index.option=my port name\n
    // set up a state machine for the various portions
    enum parseStates {
        findHeader,
        findEntity,
        findName,
        handleFoundInfo,
        bail,
        done,
    };

    uint8_t currentParseState = findHeader;
    int numFound = -1, indexValue = -1, commandValue = -1, subIndexValue = -1;

    //emit(logStringReady(QString("Parsing slot data length: %1").arg(strlen(slotData))));

    pch = strtok(slotData, "=\n");
    while(currentParseState != done){
        QString logStr = QString("Parsing: \"%1\"").arg(QString::fromLocal8Bit(pch));
        qDebug() << logStr;
        //emit(logStringReady(logStr));
        switch(currentParseState){
            case findHeader:
                logStr = "  Looking for header";
                qDebug() << logStr;
                //emit(logStringReady(logStr));
                if(0 == strcmp("NAMES", pch)){
                    //emit(logStringReady("  Found NAMES"));
                    qDebug() << "  Found NAMES";
                    // go to the next line and next state
                    currentParseState = findEntity;
                    pch = strtok(NULL, "=\n");
                }
                else {
                    //emit(logStringReady("  No saved names found"));
                    qDebug() << "  Didn't find NAMES";
                    currentParseState = bail;
                }
                break;

            case findEntity:
                logStr = "  Looking for entity";
                qDebug() << logStr;
                //emit(logStringReady(logStr));

                //reset UEI info
                numFound = -1;
                indexValue = -1;
                commandValue = -1;
                subIndexValue = -1;

                // scan the line for UEI
                if(pch != NULL){
                    numFound = sscanf(pch, "%d.%d.%d", &commandValue, &indexValue, &subIndexValue);
                }
                switch(numFound){
                    case 3:
                        logStr = QString("   Found subIndex %1").arg(subIndexValue);
                        qDebug() << logStr;
                        //emit(logStringReady(logStr));
                        // fall through
                    case 2:
                        // handle getting the name, skip the key section, grab name
                        logStr = QString("   Found entity %1 and command %2").arg(commandValue).arg(indexValue);
                        //emit(logStringReady(logStr));
                        //emit(logStringReady(QString("Found entity %1 and command %2").arg(commandValue).arg(indexValue)));
                        qDebug() << "  Found command " << commandValue;
                        qDebug() << "  Found index " << indexValue;

                        // move to the next section of this line
                        pch = strtok(NULL, "=\n");
                        currentParseState = findName;
                        break;
                    case 0:
                        // bail
                        //emit(logStringReady("   Couldn't parse UEI from key-value line"));
                        qDebug() << "  Couldn't parse command value in name data " << pch;

                        // move to next token
                        pch = strtok(NULL, "=\n");
                        currentParseState = findEntity;
                        break;
                    case EOF:
                    default:
                        //emit(logStringReady("   Out of data to parse"));
                        qDebug() << "  Out of data to parse" << pch;
                        currentParseState = bail;
                        break;
                } // end numFound switch
                break;

            case findName:
                logStr = "  Looking for name";
                qDebug() << logStr;
                //emit(logStringReady(logStr));
                if(pch==NULL){
                    //bail
                    //emit(logStringReady("   Couldn't parse name from key-value line"));
                    qDebug() << "  Couldn't parse name from key-value line" << pch;
                    currentParseState = bail;
                    break;
                }
                else {
                    //emit(logStringReady(QString("   Found name length %1; %2").arg(strlen(pch)).arg(QString::fromLocal8Bit(pch))));
                    qDebug() << "  Found name length " << strlen(pch);
                    qDebug() << "  Found name: " << pch;

                    // handle the key-vale of UEI and name
                    currentParseState = handleFoundInfo;
                }
                break;

            case handleFoundInfo:
                logStr = "  Handling command+name data";
                qDebug() << logStr;
                //emit(logStringReady(logStr));
                switch(commandValue){
                    case cmdSYSTEM:
                        //emit(logStringReady(QString("   Found hub name: %1").arg(QString::fromLocal8Bit(pch))));
                        if(portAndSystemNames[8] != QString::fromLocal8Bit(pch)){
                            portAndSystemNames[8] = QString::fromLocal8Bit(pch);
                            emit(sig_nameChanged(portAndSystemNames[8], 8));
                        }

                        break;
                    case cmdUSB:
                        //emit(logStringReady(QString("   Found port %1 name: %2").arg(subIndexValue).arg(QString::fromLocal8Bit(pch))));
                        if(portAndSystemNames[subIndexValue] != QString::fromLocal8Bit(pch)){
                            portAndSystemNames[subIndexValue] = QString::fromLocal8Bit(pch);
                            emit(sig_nameChanged(portAndSystemNames[subIndexValue], subIndexValue));
                        }
                        break;
                    default:
                        //emit(logStringReady(QString("   Unsupported name command: %1.%2").arg(commandValue).arg(indexValue)));
                        qDebug() << "Unsupported name commmand" << commandValue << "." << indexValue << " " << pch;
                        break;
                } // end commandValue switch


                // grab the next line
                pch = strtok(NULL, "=\n");
                currentParseState = findEntity;
                break;

            case bail:
            default:
                qDebug() << "  finished parsing";

                pch = NULL;
                currentParseState = done;
                break;

        } // end parser state machine
    } // end while parser !done
}



void StemWorker::parseStoredNameData(QString slotData){
    // parse the slot contents for format:
    // NAMES\n
    // entity.index.option=my port name\n
    // set up a state machine for the various portions
    enum parseStates {
        findHeader,
        findEntity,
        findName,
        handleFoundInfo,
    };

    uint8_t currentParseState = findHeader;
    int numFound = -1, indexValue = -1, commandValue = -1, subIndexValue = -1;

    //emit(logStringReady(QString("Parsing slot data length: %1").arg(slotData.length())));

    QStringList storeLines = slotData.split("\n");
    for(QString line: storeLines){
        QString logStr = QString("Parsing: \"%1\"").arg(line);
        qDebug() << logStr;
        //emit(logStringReady(logStr));

        if(line.length() == 0){
            logStr = "  Skipping blank line";
            qDebug() << logStr;
            //emit(logStringReady(logStr));
            break;
        }

        switch(currentParseState){
            case findHeader: {
                logStr = "  Looking for header";
                qDebug() << logStr;
                //emit(logStringReady(logStr));

                if(QString("NAMES") == line){
                    //emit(logStringReady("  Found NAMES"));
                    qDebug() << "  Found NAMES";
                    // go to the next line and next state
                    currentParseState = findEntity;
                }
                break;
            }
            case findEntity:{
                logStr = "  Looking for entity";
                qDebug() << logStr;
                //emit(logStringReady(logStr));

                //reset UEI info
                numFound = -1;
                indexValue = -1;
                commandValue = -1;
                subIndexValue = -1;

                QStringList nameLineList = line.split("=");

                // if we just get one element back, there was no =
                // we know there should only be 2 elements (1 =)
                if(nameLineList.length() != 2){
                    logStr = QString("   Couldn't parse UEI=name from key-value line in\"%1\"").arg(line);
                    qDebug() << logStr;
                    //emit(logStringReady(logStr));
                    currentParseState = findEntity;
                    break;
                }

                // scan the line for UEI
                QStringList ueiList = nameLineList[0].split(".");
                // if we just get one element back, splitting failed
                if(nameLineList.length() <= 1){
                    logStr = QString("   Couldn't parse UEI from key-value line in\"%1\"").arg(nameLineList[0]);
                    qDebug() << logStr;
                    //emit(logStringReady(logStr));
                    currentParseState = findEntity;
                    break;
                }
                commandValue = ueiList[0].toInt();
                indexValue = ueiList[1].toInt();
                logStr = QString("   Found entity %1 and command %2").arg(commandValue).arg(indexValue);
                //emit(logStringReady(logStr));
                qDebug() << logStr;

                if(ueiList.length() == 3){
                    subIndexValue = ueiList[2].toInt();
                    logStr = QString("   Found subIndex %1").arg(subIndexValue);
                    qDebug() << logStr;
                    //emit(logStringReady(logStr));
                }

                // grab the name
                currentParseState = findName;
                logStr = "  Looking for name";
                qDebug() << logStr;
                //emit(logStringReady(logStr));

                logStr = QString("   Found name length %1: \"%2\"").arg(nameLineList[1].length()).arg(nameLineList[1]);
                //emit(logStringReady(logStr));
                qDebug() << logStr;

                // handle the key-vale of UEI and name
                currentParseState = handleFoundInfo;
                logStr = "  Handling command+name data";
                qDebug() << logStr;
                //emit(logStringReady(logStr));
                switch(commandValue){
                    case cmdSYSTEM:
                        //emit(logStringReady(QString("   Found hub name: \"%1\"").arg(nameLineList[1])));
                        if(portAndSystemNames[8] != nameLineList[1]){
                            portAndSystemNames[8] = nameLineList[1];
                            emit(sig_nameChanged(portAndSystemNames[8], 8));
                        }

                        break;
                    case cmdUSB:
                        //emit(logStringReady(QString("   Found port %1 name: \"%2\"").arg(subIndexValue).arg(nameLineList[1])));
                        if(portAndSystemNames[subIndexValue] != nameLineList[1]){
                            portAndSystemNames[subIndexValue] = nameLineList[1];
                            emit(sig_nameChanged(portAndSystemNames[subIndexValue], subIndexValue));
                        }
                        break;
                    default:
                        emit(logStringReady(QString("   Unsupported name command: %1.%2").arg(commandValue).arg(indexValue)));
                        qDebug() << "Unsupported name commmand" << commandValue << "." << indexValue << " " << line;
                        break;
                } // end commandValue switch

                currentParseState = findEntity;

                break;
            }
            default: {
                qDebug() << "Shouldn't get here.";
                break;
            }
        } // end switch state machine

    } // end for each line

}


void StemWorker::setPortAndSystemNames(){
    QString nameData = "";

    // build up the name data
    nameData += "NAMES\n";

    for(int i=0; i<8; i++){
        if(portAndSystemNames[i].length() > 0){
            nameData += QString("18.0.%1=%2\n").arg(i).arg(portAndSystemNames[i].toLocal8Bit().constData());
        }
    }

    if(portAndSystemNames[8].length() > 0){
        nameData += QString("3.0=%1\n").arg(portAndSystemNames[8].toLocal8Bit().constData());
    }

    //make sure we're not going to run out of space
    if(nameData.length() > 4095){
        emit(logStringReady(QString("Names too long to be saved to the device (current is %1; max=4095 characters)").arg(nameData.length())));
        return;
    }

    if(module.isConnected()){
        qDebug() << "Writing names to store len: " << nameData.length();
        qDebug() << nameData;
        aErr err = aErrNone;
        err = store[storeInternalStore].loadSlot(SLOT_FOR_NAMES, (uint8_t*) nameData.toLocal8Bit().constData(), nameData.length());
        if(err != aErrNone){
            emit logStringReady(QString("Error writing names to store: %1").arg(err));
        }
    }

}
