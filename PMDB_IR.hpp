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


class PMDB_IR {

    private:
        // Type definitions for used pins
        typedef Gpio<PB, 10> IR_LED;
        typedef Gpio<PA, 10> IR_Receiver;

        Samples buffer;                     // Buffer for recorded samples
        const int NUM_SAMPLES = 512;

        Thread * is_recording = nullptr;    // Condition variable to wait for conversion to be complete

    public:

        // Setters / getters
        uint info_rec_len_ms() {
            return buffer.at(buffer.size() - 1).timestamp_us / 1000;
        }

        uint info_rec_num_samples() {
            return buffer.size();
        }

        PMDB_IR() {
            buffer.reserve(NUM_SAMPLES);    // Preallocate space for samples for efficient insertion

            // Configure required pins
            // Notably, TIM2 is reconfigured each time when needed
            {
                GlobalIrqLock lock;

                // Configure IR receiver pin with interrupt on edge change
                IR_Receiver::mode(Mode::INPUT); // Configure IR receiver pin as input
                SYSCFG->EXTICR[2] = SYSCFG_EXTICR3_EXTI10_PA;   // Route IR pin (PA10) to EXTI10 line
                EXTI->RTSR |= EXTI_RTSR_TR10;   // Trigger interrupt on rising edge
                EXTI->FTSR |= EXTI_FTSR_TR10;   // Trigger interrupt on falling edge
                IRQregisterIrq(lock, EXTI15_10_IRQn, &PMDB_IR::isr_sample, this);   // Register ISR to sample on each signal edge

                // Configure IR LED pin to be driven by TIM2 CH3 in PWM mode
                IR_LED::mode(Mode::ALTERNATE);
                IR_LED::alternateFunction(1);   // TIM2 CH3 (PWM) output
            }
        }

        ~PMDB_IR() {
            GlobalIrqLock lock;
            // Disable interrupts on GPIO
            EXTI->IMR &= ~EXTI_IMR_IM10;
            EXTI->RTSR &= ~EXTI_RTSR_TR10;
            EXTI->FTSR &= ~EXTI_FTSR_TR10;
            IRQunregisterIrq(lock, EXTI15_10_IRQn, &PMDB_IR::isr_sample, this);   // Unregister ISR
        }

        /*
        * Clear any interrupts on TIM2 and reset timer before repurposing it
        */
        inline void reset_TIM2() {
            GlobalIrqLock lock;

            // Put TIM2 in reset state via RCC
            RCC->APB1RSTR |= RCC_APB1RSTR_TIM2RST;
            RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM2RST;

            // Unregister ISR on TIM2
            if (IRQisIrqRegistered(TIM2_IRQn)) {
                IRQunregisterIrq(lock, TIM2_IRQn, &PMDB_IR::isr_stop_sampling, this);
            }

        }

        /*
        * Configure TIM2 for sampling (up counter one-pulse mode, 1us resolution, duration of 1s i.e. overflow after 1s with interrupt on update)
        */
        void configure_TIM2_sampler() {
            GlobalIrqLock lock;
            reset_TIM2();

            RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;     // Enable timer clock
            TIM2->CR1 |= TIM_CR1_OPM;               // One-pulse mode (stops timer at the next update event e.g. an overflow or setting the UG bit)
            TIM2->PSC = 84000000 / 1000000 - 1;     // Configure 1MHz timer (i.e. 1us resolution).
            TIM2->ARR = 1000000 - 1;                // Count 1000000us == 1s
            TIM2->EGR |= TIM_EGR_UG;                // Generate update event to update register values
            TIM2->SR &= ~TIM_SR_UIF;                // Clear update flag caused by setting UG
            IRQregisterIrq(lock, TIM2_IRQn,
                &PMDB_IR::isr_stop_sampling, this); // Register ISR to stop sampling
        }

        /*
        * Configure TIM2 CH3 to drive IR LED (38KHz 50% DC square wave) to stimulate IR receiver
        */
        void configure_TIM2_emitter() {
            FastGlobalIrqLock lock;
            reset_TIM2();

            RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;     // Enable clock to TIM2
            TIM2->CR1 |= TIM_CR1_ARPE;              // Enable auto-reload preload for timer
            TIM2->CCMR2 |= TIM_CCMR2_OC3M_2
                | TIM_CCMR2_OC3M_1;                 // Enable PWM mode 1 (110) for channel 3
            TIM2->CCMR2 |= TIM_CCMR2_OC3PE;         // Enable auto-reload preload for channel 3
            TIM2->PSC = 0;                          // No prescaling, keep time base at 84MHz
            TIM2->ARR = 84000 / 38 - 1;             // Set frequency to 38KHz
            TIM2->CCR3 = (84000 / 38 - 1) / 2;      // Set DC to 50%
            TIM2->CNT=0;                            // Reset counter to 0
            TIM2->EGR |= TIM_EGR_UG;                // Reinitialize counter and update registers
            TIM2->CR1 |= TIM_CR1_CEN;               // Start time base (notably, the timer keeps running forever from this point on)
            TIM2->CCER &= ~TIM_CCER_CC3E;           // Make sure channel output is disabled for now
        }


        /*
        * Send the last sampled IR data by toggling TIM2 channel 3 (PWM) on and off
        * This is done in polling and with disabled interrupts to ensure precision;
        * this is probably not a problem since a recording lasts tens of milliseconds.
        */
        void send() {
            configure_TIM2_emitter();       // Reconfigure TIM2 as a 38KHz 50% DC square wave to stimulate receiver
            {
                FastGlobalIrqLock irqLock;

                uint start_ts_us = IRQgetTime() / 1000; // Record initial timestamp

                for (auto& sample: buffer) {

                    int next_ts_us = start_ts_us + sample.timestamp_us;
                    while (IRQgetTime() / 1000 - next_ts_us < 0) ;  // Busy wait until system timer exceeds
                                                                    // the current sample's timestamp (+ initial timestamp)

                    // Toggle PWM channel according to signal edge
                    if (sample.level)                   // Rising edge
                        TIM2->CCER &= ~TIM_CCER_CC3E;   // Disable timer channel
                    else                                // Falling edge -> stimulate receiver (active low)
                        TIM2->CCER |= TIM_CCER_CC3E;    // Enable timer channel
                }

                // Disable timer channel for good measure
                TIM2->CCER &= ~TIM_CCER_CC3E;
            }
        }

        /*
        * Capture IR data by sampling the IR receiver's pin on signal edge
        * Capture starts on the first signal edge and stops after 1 second and is done with interrupts
        * - The first interrupt is triggered on each signal edge to sample another point
        * - The second interrupt is triggered when the 1 second timer elapses
        *  to disable both interrupts and essentially stop the recording
        * On function return, signal is memorised and ready to be used by 'send()'.
        */
        void receive() {
            buffer.clear();             // Empty buffer before receiving
            configure_TIM2_sampler();   // Reconfigure TIM2 as a one-shot 1s timer with 1us resolution
            {
                FastGlobalIrqLock lock;
                TIM2->DIER |= TIM_DIER_UIE; // Enable "stop_sampling" ISR on timer update (overflow)
                EXTI->IMR |= EXTI_IMR_IM10; // Enable "sample_signal" ISR on GPIO level change

                is_recording = Thread::IRQgetCurrentThread();
                do {
                    Thread::IRQglobalIrqUnlockAndWait(lock);
                } while (is_recording);  // Spin on condition variable
            }
        }

        /*
        * Pretty print recorded signal as a VCD (value change dump) file
        * that can be copypasted into a .vcd file and opened in PulseView / GTKWave
        */
        void pprint_vcd() {
            const uint shift_us = 10000;
            iprintf("$version PMDB16 IR dumper $end\n");
            iprintf("$timescale 1 us $end\n");
            iprintf("$scope module top $end\n");
            iprintf("$var wire 1 ! ir $end\n");
            iprintf("$upscope $end\n");
            iprintf("$enddefinitions $end\n");
            iprintf("#0 1!\n");                         // "fake" rising edge
            for (auto& sample: buffer) {
                iprintf("#%d %d!\n", sample.timestamp_us + shift_us, sample.level);
            }
        }

    private:

        // --- Interrupts ---

        /*
        * This interrupt starts the timer and samples the signal level of the GPIO
        * together with its timestamp, then wakes up the caller thread to log info
        */
        void isr_sample() {

            // Check that interrupt is pending for EXTI10
            if (!(EXTI->PR & EXTI_PR_PR10)) {
                return;
            }
            EXTI->PR |= EXTI_PR_PR10;   // Clear interrupt pending bit

            // Start timer if not started already
            if (!(TIM2->CR1 & TIM_CR1_CEN)) {   // Note: in one-pulse mode, CEN bit is cleared automatically when timer elapses
                TIM2->CR1 |= TIM_CR1_CEN;       // Start timer
            }

            // Sample timestamp and IR port level
            bool signal_level = IR_Receiver::value();
            uint timestamp_us = TIM2->CNT;
            buffer.push_back({signal_level, timestamp_us}); // Note: interrupt is triggered on signal edge, thus level == 1 => rising edge
        }

        /*
        * This interrupt disables all interrupts to stop recording
        */
        void isr_stop_sampling() {

            // Disable all interrupts to stop recording, then wake up the receiver function
            if (TIM2->SR & TIM_SR_UIF) {
                TIM2->SR &= ~TIM_SR_UIF;       // Clear update flag
                EXTI->IMR &= ~EXTI_IMR_IM10;    // Disable edge triggered interrupt on GPIO pin
                TIM2->DIER &= ~TIM_DIER_UIE;   // Disable this interrupt (timer update pin on overflow)

                // Wake up receiver function
                is_recording->IRQwakeup();   // Will occur as soon as going out of the interrupt
                is_recording = nullptr;
            }
        }

};
