#include <stdio.h>
#include <thread>
#include <vector>
#include "drivers/gpio/stm32_gpio.h"
#include "kernel/lock.h"
#include "kernel/thread.h"
#include "miosix.h"

#include "CMSIS/Device/ST/STM32F4xx/Include/stm32f401xe.h"

using namespace std;
using namespace miosix;

// Defining data structures for signal

// Signal state
//         ^-------------+      <-------- HIGH
//         |             |
// --------+             v----- <-------- LOW
//         ^ RISING      ^ FALLING
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


/*
 * Plan: generate a PWM signal at 38KHz (50% DC) using TIM2 CH3 and route it towards pin PB10 connected to the IR LED
 * To do:
 * - Configure timer to generate PWM
 * - Configure GPIO with alternate function 1 from timer
 * - Toggle GPIO on and off at the proper timing, reading data from the waveform
 * - Send the full waveform at the press of the user button
 *
 * Expected outcome: pressing the button turns on the POUL soundboard
 */
int main()
{
    iprintf("IR Remote emitter\n");

    Wave wave = {{RISING_EDGE, 0},
        {FALLING_EDGE, 9993500},
        {RISING_EDGE, 12685000},
        {FALLING_EDGE, 13522000},
        {RISING_EDGE, 14008500},
        {FALLING_EDGE, 14404500},
        {RISING_EDGE, 14890500},
        {FALLING_EDGE, 15286500},
        {RISING_EDGE, 15772500},
        {FALLING_EDGE, 16605500},
        {RISING_EDGE, 17091500},
        {FALLING_EDGE, 17933000},
        {RISING_EDGE, 19297000},
        {FALLING_EDGE, 20134000},
        {RISING_EDGE, 20616000},
        {FALLING_EDGE, 21012000},
        {RISING_EDGE, 21498000},
        {FALLING_EDGE, 21894000},
        {RISING_EDGE, 22380000},
        {FALLING_EDGE, 22776000},
        {RISING_EDGE, 23262500},
        {FALLING_EDGE, 23658500},
        {RISING_EDGE, 24144500},
        {FALLING_EDGE, 24540500},
        {RISING_EDGE, 25027000},
        {FALLING_EDGE, 25423000},
        {RISING_EDGE, 25909000},
        {FALLING_EDGE, 26305000},
        {RISING_EDGE, 26787000},
        {FALLING_EDGE, 27187000},
        {RISING_EDGE, 27669000},
        {FALLING_EDGE, 28069500},
        {RISING_EDGE, 28551000},
        {FALLING_EDGE, 28947000},
        {RISING_EDGE, 29874500},
        {FALLING_EDGE, 30275000},
        {RISING_EDGE, 30761000},
        {FALLING_EDGE, 31157000},
        {RISING_EDGE, 31643000},
        {FALLING_EDGE, 32039000},
        {RISING_EDGE, 32525000},
        {FALLING_EDGE, 32921000},
        {RISING_EDGE, 33407000},
        {FALLING_EDGE, 34240000},
        {RISING_EDGE, 34726000},
        {FALLING_EDGE, 35122500},
        {RISING_EDGE, 35604000},
        {FALLING_EDGE, 36004500},
        {RISING_EDGE, 36486000},
        {FALLING_EDGE, 36886500},
        {RISING_EDGE, 37809500},
        {FALLING_EDGE, 38646500},
        {RISING_EDGE, 39133000},
        {FALLING_EDGE, 39529000},
        {RISING_EDGE, 40015000},
        {FALLING_EDGE, 40411000},
        {RISING_EDGE, 40897000},
        {FALLING_EDGE, 41293500},
        {RISING_EDGE, 41779500},
        {FALLING_EDGE, 42175500},
        {RISING_EDGE, 42661500},
        {FALLING_EDGE, 43057500},
        {RISING_EDGE, 43980500},
        {FALLING_EDGE, 44385000},
        {RISING_EDGE, 44867000},
        {FALLING_EDGE, 45704000},
        {RISING_EDGE, 46627000},
        {FALLING_EDGE, 116243000},
        {RISING_EDGE, 118934500},
        {FALLING_EDGE, 119771500},
        {RISING_EDGE, 120258000},
        {FALLING_EDGE, 120654000},
        {RISING_EDGE, 121135500},
        {FALLING_EDGE, 121536000},
        {RISING_EDGE, 122018000},
        {FALLING_EDGE, 122855000},
        {RISING_EDGE, 123337000},
        {FALLING_EDGE, 124182500},
        {RISING_EDGE, 125546500},
        {FALLING_EDGE, 126379500},
        {RISING_EDGE, 126865500},
        {FALLING_EDGE, 127261500},
        {RISING_EDGE, 127747500},
        {FALLING_EDGE, 128143500},
        {RISING_EDGE, 128629500},
        {FALLING_EDGE, 129025500},
        {RISING_EDGE, 129507500},
        {FALLING_EDGE, 129908000},
        {RISING_EDGE, 130389500},
        {FALLING_EDGE, 130790000},
        {RISING_EDGE, 131271500},
        {FALLING_EDGE, 131672000},
        {RISING_EDGE, 132154000},
        {FALLING_EDGE, 132554000},
        {RISING_EDGE, 133036000},
        {FALLING_EDGE, 133436500},
        {RISING_EDGE, 133918000},
        {FALLING_EDGE, 134318500},
        {RISING_EDGE, 134800000},
        {FALLING_EDGE, 135200500},
        {RISING_EDGE, 136123500},
        {FALLING_EDGE, 136523500},
        {RISING_EDGE, 137010000},
        {FALLING_EDGE, 137406000},
        {RISING_EDGE, 137892000},
        {FALLING_EDGE, 138288000},
        {RISING_EDGE, 138769500},
        {FALLING_EDGE, 139170000},
        {RISING_EDGE, 139651500},
        {FALLING_EDGE, 140488500},
        {RISING_EDGE, 140970500},
        {FALLING_EDGE, 141371000},
        {RISING_EDGE, 141852500},
        {FALLING_EDGE, 142253000},
        {RISING_EDGE, 142734500},
        {FALLING_EDGE, 143135000},
        {RISING_EDGE, 144058000},
        {FALLING_EDGE, 144899000},
        {RISING_EDGE, 145381000},
        {FALLING_EDGE, 145781500},
        {RISING_EDGE, 146263000},
        {FALLING_EDGE, 146663500},
        {RISING_EDGE, 147145000},
        {FALLING_EDGE, 147545500},
        {RISING_EDGE, 148027000},
        {FALLING_EDGE, 148423000},
        {RISING_EDGE, 148909000},
        {FALLING_EDGE, 149305500},
        {RISING_EDGE, 150228000},
        {FALLING_EDGE, 150632500},
        {RISING_EDGE, 151114500},
        {FALLING_EDGE, 151951500},
        {RISING_EDGE, 152874500}};

    using user_btn_PC13 = Gpio<PC, 13>;
    using IR_LED_PB10 = Gpio<PB, 10>;

    {   // Device configuration
        FastGlobalIrqLock lock; // RAII to disable IRQs in this scoped block (critical section)

        // Input button configuration
        user_btn_PC13::mode(Mode::INPUT);

        // Timer configuration as a PWM generator
        // Refer to 13.4 in the manual for register configuration
        RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

        // Control register 1
        TIM2->CR1 |= TIM_CR1_ARPE;  // Enable auto-reload preload

        // Capture/compare mode register (channels 3/4)
        TIM2->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1; // Enable PWM mode 1 (110) for channel 3
        TIM2->CCMR2 |= TIM_CCMR2_OC3PE;                     // Enable auto-reload preload for channel 3

        TIM2->PSC = 0;                      // No prescaling, keep time base at 84MHz
        TIM2->ARR = 84000 / 38 - 1;         // Set frequency to 38KHz
        TIM2->CCR3 = (84000 / 38 - 1) / 2;  // Set DC to 50%
        TIM2->CNT=0;                        // Reset counter to 0

        TIM2->EGR |= TIM_EGR_UG;   // Reinitialize counter and update registers

        TIM2->CR1 |= TIM_CR1_CEN;   // Start timer (notably, the timer keeps running forever from this point on)
        TIM2->CCER &= ~TIM_CCER_CC3E;   // Make sure channel output is disabled for now

        // Routing PWM output to IR LED pin
        IR_LED_PB10::mode(Mode::ALTERNATE);
        IR_LED_PB10::alternateFunction(1);
    }

    enum keystate {PRESSED = 0, NOT_PRESSED = 1};
    keystate prev_keystate, curr_keystate = NOT_PRESSED;

    while(true) {

        // Poll continuously for keypress
        do {
            prev_keystate = curr_keystate;
            curr_keystate = static_cast<keystate>(user_btn_PC13::value());
            if (!(prev_keystate == NOT_PRESSED && curr_keystate == PRESSED)) {
                this_thread::sleep_for(10ms);
            } else break;
        } while (true);


        // Send the wave via IR by toggling channel on and off
        // To ensure precision, interrupts are disabled while sending the waveform

        {
            FastGlobalIrqLock irqLock;

            uint prev_ts_ns = 0;
            //uint prev_ts_sys_ns = IRQgetTime(); // WARNING: use this with interrupts disabled!
            //uint curr_ts_sys_ns;

            for(auto sc: wave) {

                // Busy wait for a given delta time across consecutive timestamps
                uint delta_us = (sc.timestamp_ns - prev_ts_ns) / 1000;

                // Compute elapsed time according to system as a sanity check
                // curr_ts_sys_ns = IRQgetTime();
                // uint delta_sys_us = (curr_ts_sys_ns - prev_ts_sys_ns) / 1000;
                // int error_us = delta_sys_us - delta_us;

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
                // prev_ts_sys_ns = curr_ts_sys_ns;

                //iprintf("%s edge after %d us with error %d\n", sc.state == RISING_EDGE ? "Rising" : "Falling", delta_us, error_us);
            }
        }

        // Finally disable channel again
        TIM2->CCER &= ~TIM_CCER_CC3E;
    }
}
