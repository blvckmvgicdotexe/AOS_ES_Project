#include <stdio.h>
#include <thread>
#include "drivers/gpio/stm32_gpio.h"
#include "interfaces/interrupts.h"
#include "kernel/lock.h"
#include "miosix.h"

#include "CMSIS/Device/ST/STM32F4xx/Include/stm32f401xe.h"

using namespace std;
using namespace miosix;

int main()
{
    iprintf("My first program running under Miosix :-)\nClick the user (blue) button on the board to switch the green LED on and off!\n");

    using user_button = Gpio<PC, 13>;   //PC13 according to schematics
    using green_led = Gpio<PA, 5>;      //PA5 according to schematics

    {   // Disabling interrupts while configuring GPIOs
        FastGlobalIrqLock lock; // RAII to acquire lock in this scoped block
        user_button::mode(Mode::INPUT);
        green_led::mode(Mode::OUTPUT);
    }


    enum keystate {PRESSED = 0, NOT_PRESSED = 1};
    keystate prev_keystate, curr_keystate = NOT_PRESSED;

    while(true) {
        // Poll continuously for rising edge of the signal
        do {
            prev_keystate = curr_keystate;
            curr_keystate = static_cast<keystate>(user_button::value());
            if (!(prev_keystate == NOT_PRESSED && curr_keystate == PRESSED)) {
                this_thread::sleep_for(10ms);
            } else break;
        } while (true);

        static bool led_on = false;

        if (!led_on) {
            green_led::high();
            iprintf("LED has been turned ON\n");
        } else {
            green_led::low();
            iprintf("LED has been turned OFF\n");
        }

        led_on = !led_on;               // Toggle led status

    }
}
