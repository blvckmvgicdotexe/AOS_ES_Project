#include <stdio.h>
#include <thread>
#include <vector>
#include "IREmitter.hpp"
#include "drivers/gpio/stm32_gpio.h"
#include "kernel/lock.h"
#include "kernel/thread.h"
#include "miosix.h"


using namespace std;
using namespace miosix;

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
        {FALLING_EDGE, 19991},
        {RISING_EDGE, 20352},
        {FALLING_EDGE, 21004},
        {RISING_EDGE, 21360},
        {FALLING_EDGE, 22016},
        {RISING_EDGE, 22372},
        {FALLING_EDGE, 24058},
        {RISING_EDGE, 24414},
        {FALLING_EDGE, 25070},
        {RISING_EDGE, 25426},
        {FALLING_EDGE, 26082},
        {RISING_EDGE, 26438},
        {FALLING_EDGE, 27094},
        {RISING_EDGE, 27451},
        {FALLING_EDGE, 28106},
        {RISING_EDGE, 28463},
        {FALLING_EDGE, 30148},
        {RISING_EDGE, 30505},
        {FALLING_EDGE, 31160},
        {RISING_EDGE, 31517},
        {FALLING_EDGE, 33198},
        {RISING_EDGE, 33554},
        {FALLING_EDGE, 34210},
        {RISING_EDGE, 34566},
        {FALLING_EDGE, 35226},
        {RISING_EDGE, 35583},
        {FALLING_EDGE, 36239},
        {RISING_EDGE, 36595},
        {FALLING_EDGE, 38276},
        {RISING_EDGE, 38632},
        {FALLING_EDGE, 39288},
        {RISING_EDGE, 39644},
        {FALLING_EDGE, 87569},
        {RISING_EDGE, 87926},
        {FALLING_EDGE, 88581},
        {RISING_EDGE, 88938},
        {FALLING_EDGE, 89593},
        {RISING_EDGE, 89954},
        {FALLING_EDGE, 91635},
        {RISING_EDGE, 91992},
        {FALLING_EDGE, 92647},
        {RISING_EDGE, 93004},
        {FALLING_EDGE, 93659},
        {RISING_EDGE, 94020},
        {FALLING_EDGE, 95701},
        {RISING_EDGE, 96058},
        {FALLING_EDGE, 97739},
        {RISING_EDGE, 98095},
        {FALLING_EDGE, 98751},
        {RISING_EDGE, 99112},
        {FALLING_EDGE, 100792},
        {RISING_EDGE, 101149},
        {FALLING_EDGE, 101805},
        {RISING_EDGE, 102161},
        {FALLING_EDGE, 103842},
        {RISING_EDGE, 104199},
        {FALLING_EDGE, 105879},
        {RISING_EDGE, 106240},
        {FALLING_EDGE, 107921},
        {RISING_EDGE, 108277},
        {FALLING_EDGE, 108933},
        {RISING_EDGE, 109289},
        {FALLING_EDGE, 110970},
        {RISING_EDGE, 111331},
        {FALLING_EDGE, 155148},
        {RISING_EDGE, 155504},
        {FALLING_EDGE, 156160},
        {RISING_EDGE, 156516},
        {FALLING_EDGE, 157172},
        {RISING_EDGE, 157533},
        {FALLING_EDGE, 159213},
        {RISING_EDGE, 159570},
        {FALLING_EDGE, 160225},
        {RISING_EDGE, 160582},
        {FALLING_EDGE, 161238},
        {RISING_EDGE, 161598},
        {FALLING_EDGE, 162250},
        {RISING_EDGE, 162610},
        {FALLING_EDGE, 163266},
        {RISING_EDGE, 163623},
        {FALLING_EDGE, 165303},
        {RISING_EDGE, 165660},
        {FALLING_EDGE, 166315},
        {RISING_EDGE, 166676},
        {FALLING_EDGE, 168357},
        {RISING_EDGE, 168713},
        {FALLING_EDGE, 169369},
        {RISING_EDGE, 169726},
        {FALLING_EDGE, 170381},
        {RISING_EDGE, 170738},
        {FALLING_EDGE, 171393},
        {RISING_EDGE, 171754},
        {FALLING_EDGE, 173435},
        {RISING_EDGE, 173791},
        {FALLING_EDGE, 174447},
        {RISING_EDGE, 174803},
        {FALLING_EDGE, 222725},
        {RISING_EDGE, 223086},
        {FALLING_EDGE, 223738},
        {RISING_EDGE, 224098},
        {FALLING_EDGE, 224754},
        {RISING_EDGE, 225110},
        {FALLING_EDGE, 226791},
        {RISING_EDGE, 227152},
        {FALLING_EDGE, 227803},
        {RISING_EDGE, 228164},
        {FALLING_EDGE, 228816},
        {RISING_EDGE, 229176},
        {FALLING_EDGE, 230857},
        {RISING_EDGE, 231214},
        {FALLING_EDGE, 232895},
        {RISING_EDGE, 233255},
        {FALLING_EDGE, 233907},
        {RISING_EDGE, 234268},
        {FALLING_EDGE, 235948},
        {RISING_EDGE, 236305},
        {FALLING_EDGE, 236961},
        {RISING_EDGE, 237321},
        {FALLING_EDGE, 238998},
        {RISING_EDGE, 239359},
        {FALLING_EDGE, 241040},
        {RISING_EDGE, 241396},
        {FALLING_EDGE, 243077},
        {RISING_EDGE, 243437},
        {FALLING_EDGE, 244089},
        {RISING_EDGE, 244450},
        {FALLING_EDGE, 246130},
        {RISING_EDGE, 246487}};

    using user_btn_PC13 = Gpio<PC, 13>;

    {   // Device configuration
        FastGlobalIrqLock lock; // RAII to disable IRQs in this scoped block (critical section)

        // Input button configuration
        user_btn_PC13::mode(Mode::INPUT);
    }

    IREmitter ir;

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

        ir.send(wave);

        iprintf("Wave sent.\n");

    }
}
