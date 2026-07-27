#pragma once

#include "CMSIS/Device/ST/STM32F4xx/Include/stm32f401xe.h"
#include "interfaces/interrupts.h"
#include "kernel/lock.h"
#include <miosix.h>

using namespace std;
using namespace miosix;

struct Sample {
    bool level;
    uint timestamp_us;
};

typedef vector<Sample> Samples;
