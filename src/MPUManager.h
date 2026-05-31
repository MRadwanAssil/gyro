#ifndef MPUMANAGER_H
#define MPUMANAGER_H

#include "MPUData.h"

class MPUManager {
private:
    const int MPU = 0x68;
public:
    bool begin();
    void reset();
    MPUData readMPU(float gyro_sens, float smooth, float dt);
};

#endif