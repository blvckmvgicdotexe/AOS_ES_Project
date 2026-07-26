#pragma once

#include "interfaces/delays.h"
#include "kernel/lock.h"
#include <vector>

/*
 * IR Emitter class built for a STM32 Nucleo-F401RE paired with the PMDB16 board
 * built for the Microcontrollori course / Sensor Systems course @ Politecnico di Milano
 * which features, among other things, an IR LED powered by a class B amplifier connected to the PB10 pin
 *
 * What this class does is configure a timer in PWM mode to generate a 38KHz 50% DC square wave
 * which is then toggled on and off according to the waveform to send to stimulate the IR receiver on the other end
 */


using namespace std;
using namespace miosix;

// Signal definitions

// Signal state
//         ^-------------+      <-------- HIGH
//         |             |
// --------+             v----- <-------- LOW
//         ^ RISING      ^ FALLING
#include <sys/types.h>
enum SigState { LOW, HIGH, RISING_EDGE, FALLING_EDGE };

// Signal values at given timestamps (ns)
struct SigChange {
    SigState state;
    uint timestamp_ns;

    SigChange(SigState s, uint t_ns) {
        state = s;
        timestamp_ns = t_ns;
    }
};

typedef vector<SigChange> Wave;
typedef Gpio<PB, 10> IR_LED;

// TODO make class a singleton
// TODO enable concurrent access with locks
class IREmitter {
    private:

    public:

        // Configure TIM2 CH3 as PWM and PB10
        IREmitter() {
            {
                FastGlobalIrqLock lock;

                // Timer TIM2 CH3 configuration as a PWM generator
                // Refer to 13.4 in the manual for register configuration
                RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

                TIM2->CR1 |= TIM_CR1_ARPE;  // Enable auto-reload preload for timer

                TIM2->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1; // Enable PWM mode 1 (110) for channel 3
                TIM2->CCMR2 |= TIM_CCMR2_OC3PE;                     // Enable auto-reload preload for channel 3

                TIM2->PSC = 0;                      // No prescaling, keep time base at 84MHz
                TIM2->ARR = 84000 / 38 - 1;         // Set frequency to 38KHz
                TIM2->CCR3 = (84000 / 38 - 1) / 2;  // Set DC to 50%
                TIM2->CNT=0;                        // Reset counter to 0

                TIM2->EGR |= TIM_EGR_UG;   // Reinitialize counter and update registers

                TIM2->CR1 |= TIM_CR1_CEN;   // Start timer (notably, the timer keeps running forever from this point on)
                TIM2->CCER &= ~TIM_CCER_CC3E;   // Make sure channel output is disabled for now

                // Route PWM output to IR LED pin
                IR_LED::mode(Mode::ALTERNATE);
                IR_LED::alternateFunction(1);
            }

        }

        // Stop timer and put pin to high impedance
        ~IREmitter() {

        }

        // Send the wave via IR by toggling channel on and off
        // To ensure precision, interrupts are disabled while sending the waveform
        void send(Wave& wave) {
            {
                FastGlobalIrqLock irqLock;

                uint prev_ts_ns = 0;

                for(auto& sc: wave) {

                    // Busy wait for a given delta time across consecutive timestamps
                    uint delta_us = (sc.timestamp_ns - prev_ts_ns) / 1000;

                    if (delta_us > 1000) {
                        delayMs(delta_us / 1000);
                    } else {
                        delayUs(delta_us);
                    }

                    // Toggle PWM channel according to signal edge
                    switch (sc.state) {
                        case RISING_EDGE:
                        TIM2->CCER |= TIM_CCER_CC3E;    // Enable timer channel
                        break;

                        default:    // FALLING_EDGE
                        TIM2->CCER &= ~TIM_CCER_CC3E;   // Disable timer channel

                    }

                    prev_ts_ns = sc.timestamp_ns;
                }
            }

            // Finally disable channel again
            TIM2->CCER &= ~TIM_CCER_CC3E;

        }

};
