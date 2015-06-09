#include "DroneLibs.h"

typedef WahbaFilter Filter_t;
const float MINIMUM_INT_PERIOD = 5000;
Settings        settings(eeStorage::getInstance());
HardwareSerial *commSerial  = &Serial;
CommManager     comms(commSerial, eeStorage::getInstance());
Filter_t        orientation(1,1,1);

LEA6H     gps;
MPU6000   mpu;
HMC5883L  cmp;
Sensor* sens[2] = {&mpu, &cmp};
InertialManager sensors(sens, 2);
#define Output_t HK_ESCOutputDevice
Output_t esc[4] =
    { Output_t(12), Output_t(11)
     ,Output_t( 8), Output_t( 7) };
OutputDevice* outDev[4] = {&esc[0], &esc[1], &esc[2], &esc[3]};
OutputManager output(outDev);

PIDparameters pitchPID(0,0,0,-1,1), rollPID(0,0,0,-1,1);
Horizon horizon(&pitchPID, &rollPID);

void isrCallback(){
    tic(0);
    sensors.update();
    orientation.update(sensors);
    output.update(orientation);
    toc(0);
}
void updatePID(float d){
    using namespace AirSettings;
    PIDparameters newPID = PIDparameters( settings.get(ATT_P_TERM),
                                          settings.get(ATT_I_TERM),
                                          settings.get(ATT_D_TERM),
                                          -1, 1 );
    pitchPID = newPID;
    rollPID  = newPID;
}
void changeInterruptPeriod(float newPeriod){
    if(newPeriod < MINIMUM_INT_PERIOD) newPeriod = MINIMUM_INT_PERIOD;
    startInterrupt(isrCallback, newPeriod);
}
void setupSettings(){
    using namespace AirSettings;
    settings.attach(INT_PERIOD, 8000 , &changeInterruptPeriod );
    settings.attach(ACCL_MSE  , 1E2f , callback<Filter_t, &orientation, &Filter_t::setAcclMSE>);
    settings.attach(ATT_SYSMSE, 1E1f , callback<Filter_t, &orientation, &Filter_t::setSysMSE> );
    settings.attach(ATT_ERRFAC, 5E5f , callback<Filter_t, &orientation, &Filter_t::setAcclEF> );
    settings.attach(ATT_P_TERM, 0.25f, &updatePID );
    settings.attach(ATT_I_TERM, 0.05f, &updatePID );
    settings.attach(ATT_D_TERM, 0.02f, &updatePID );
    settings.attach(UNUSED_K,   4.5f , callback<Horizon, &horizon, &Horizon::setVelFac>);
}
void arm(){
    orientation.calibrate(true);
    delay(500);
    output.enable();
    orientation.calibrate(false);
}
void calibrate(){
    orientation.calibrate(true);
    delay(500);
    output.calibrate();
    orientation.calibrate(false);
}
void setupQuad() {
    Serial.begin(Protocol::BAUD_RATE);

    mpu.tuneAccl(settings.getAccelTune());
    cmp.tune(settings.getMagTune());
    setupSettings();

    sensors.start();
    gps.init();
    sensors.calibrate();

    arm();

    setupAPM2radio();
    comms.requestResync();
}
void loopQuad() {
    comms.update();
    gps.update();
}
