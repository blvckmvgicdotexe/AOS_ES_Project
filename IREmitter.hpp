#pragma once

#include "PMDB_IR.hpp"
#include "interfaces/delays.h"
#include "kernel/lock.h"
#include "kernel/thread.h"
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

typedef Gpio<PB, 10> IR_LED;

class IREmitter {

    public:

        IREmitter() {
            {
                FastGlobalIrqLock lock;

                // Configure TIM2 CH3 as a PWM generator
                RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; // Enable clock to TIM2

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
        void send(Samples& samples) {
            {
                FastGlobalIrqLock irqLock;


                uint start_ts_us = IRQgetTime() / 1000; // Initial timestamp

                // Idea: busy wait on current timestamp until it is greater than the current sample's timestamp
                for (auto& sample: samples) {

                    int next_ts_us = start_ts_us + sample.timestamp_us;
                    while (IRQgetTime() / 1000 - next_ts_us < 0) ;   // Busy wait until diff between current and target timestamp are equal

                    // Toggle PWM channel according to signal edge
                    if (sample.level)   // Rising edge
                        TIM2->CCER &= ~TIM_CCER_CC3E;   // Disable timer channel
                    else                // Falling edge -> stimulate receiver (active low)
                        TIM2->CCER |= TIM_CCER_CC3E;    // Enable timer channel

                }
            }

            // Finally disable channel again
            TIM2->CCER &= ~TIM_CCER_CC3E;

        }

};
