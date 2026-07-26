#pragma once

#include "CMSIS/Device/ST/STM32F4xx/Include/stm32f401xe.h"
#include "interfaces/interrupts.h"
#include "kernel/lock.h"
#include <miosix.h>


/*
 * PMDB16 IR Reader class
 *
 * Rationale: the digital IR signal comes from a IR RX receiver (TSOP58238) module integrated on the PMDB16 board
 * which demodulates the 38KHz stimuli already into a more digestible 9600 baud rate
 * (as a pessimistic estimate, in reality most commercial IR remotes never go past 4800)
 * -> in principle, each time I have a rising or falling edge of the IR signal on the GPIO port,
 * I need to sample both whether it is a rising/falling edge and also the timestamp (in us)
 * -> I need to configure GPIO interrupts on rising/falling edge and configure a timer with a resolution
 * sufficient enough, maybe a 1us
 *
 * Estimating 500 signal edges in a single recording with a 9600 baud rate, my recording can last
 * at least 500 * (1 / 9600) = 52ms, while with lower baud rates like 500 baud I need to wait at most
 * 1s -> if I configure a timer with a resolution of 1us, I need to be able to count up until 1s / 1us = 1000000,
 * which greatly exceeds the 16bits of a "standard" timer -> I need to use either TIM2 or TIM5 which are 32bit
 *
 */

using namespace std;
using namespace miosix;

typedef Gpio<PA, 10> IR_Receiver;
#define TIMER TIM2

class IRReader {
    private:

        Thread * waiting = nullptr; // Condition variable to wait for conversion to be complete

        struct Sample {
            bool level;
            uint timestamp_us;
        };

        typedef vector<Sample> Samples;
        Samples samples;    // Buffer for recorded samples
        const int NUM_SAMPLES = 512;

        // This interrupt is triggered each time there is a signal change on the IR RX pin
        void ISRsampleSignal() {
            static bool recording = false;

            // Do stuff only if the interrupt comes from my pin
            if (!(EXTI->PR & EXTI_PR_PR10)) {
                return;
            }

            EXTI->PR |= EXTI_PR_PR10;   // Clear interrupt pending bit

            // Start timer if not recording already
            if (!recording) {
                TIMER->CR1 |= TIM_CR1_CEN;  // Start timer
                recording = true;
            }

            // Sample timestamp and IR port level
            // Interrupt is triggered on level change, thus level == 1 => rising edge
            uint timestamp_us = TIMER->CNT;
            bool signal_level = IR_Receiver::value();
            samples.push_back({signal_level, timestamp_us});

            // TODO move this in another interrupt, otherwise another spurious signal change is needed to disable the interrupt
            if ((TIMER->CR1 & TIM_CR1_CEN) == 0) {  // Check whether timer has elapsed
                EXTI->IMR &= ~EXTI_IMR_IM10; // Disable interrupt
                recording = false;

                // Wake up receiver function
                waiting->IRQwakeup();
                waiting = nullptr;
            }
        }


    public:

        IRReader() {
            // Preallocate space for samples -> should enable O(1) insertion
            samples.reserve(NUM_SAMPLES);

            {
                GlobalIrqLock lock; // RAII irq lock to avoid contention

                // 1. Configure IR RX pin
                IR_Receiver::mode(Mode::INPUT);

                // 2. Configure IR RX pin interrupt
                SYSCFG->EXTICR[2] = SYSCFG_EXTICR3_EXTI10_PA;   // Route IR pin (PA10) to EXTI10
                EXTI->RTSR |= EXTI_RTSR_TR10;   // Trigger interrupt on rising edge
                EXTI->FTSR |= EXTI_FTSR_TR10;   // Trigger interrupt on falling edge
                IRQregisterIrq(lock, EXTI15_10_IRQn, &IRReader::ISRsampleSignal, this);   // Register interrupt routine

                // 3. Configure timer (Up counter, one-pulse mode, 1us resolution, duration of 1s)
                RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; // Enable TIM2 clock
                TIMER->CR1 |= TIM_CR1_OPM;  // One-pulse mode (stops timer at the next update event like an overflow or setting the UG bit)
                TIMER->PSC = 84000000 / 1000000 - 1;    // Configure 1MHz timer (i.e. 1us resolution).
                TIMER->ARR = 1000000 - 1;   // Count 1000000us == 1s
                TIMER->EGR |= TIM_EGR_UG;   // Generate update event to update register values
            }

        }


        // Starts capturing IR data, then returns once it is done
        void receive() {
            samples.clear();    // Empty buffer before receiving
            {
                FastGlobalIrqLock lock;
                EXTI->IMR |= EXTI_IMR_IM10; // Enable interrupt to start processing

                waiting = Thread::IRQgetCurrentThread();
                do {
                    iprintf("Waiting...\n");
                    Thread::IRQglobalIrqUnlockAndWait(lock);    // Sleep until ISR wakes me up
                } while (waiting);

            }
            iprintf("Received IR data, outputting as VCD waveform\n\n");
            iprintf("$version PMDB16 IR dumper $end\n");
            iprintf("$timescale 1 us $end\n");
            iprintf("$scope module top $end\n");
            iprintf("$var wire 1 ! ir $end\n");
            iprintf("$upscope $end\n");
            iprintf("$enddefinitions $end\n");
            iprintf("#0 1!\n"); // "fake" rising edge
            const uint shift_us = 10000;
            for (auto& sample: samples) {
                iprintf("#%d %d!\n", sample.timestamp_us + shift_us, sample.level);
            }

        }
};
