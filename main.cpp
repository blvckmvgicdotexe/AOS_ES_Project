#include <stdio.h>
#include <sys/stat.h>
#include <thread>
#include <vector>
#include "IREmitter.hpp"
#include "IRReader.hpp"
#include "PMDB_IR.hpp"
#include "drivers/gpio/stm32_gpio.h"
#include "kernel/lock.h"
#include "kernel/thread.h"
#include "miosix.h"


using namespace std;
using namespace miosix;


void cls() {
    iprintf("\e[1;1H\e[2J");
}

void print_header() {
    iprintf("\n");
    iprintf("  ██╗██████╗     ███████╗ █████╗ ███╗   ███╗██████╗ ██╗     ███████╗██████╗ \n");
    iprintf("  ██║██╔══██╗    ██╔════╝██╔══██╗████╗ ████║██╔══██╗██║     ██╔════╝██╔══██╗\n");
    iprintf("  ██║██████╔╝    ███████╗███████║██╔████╔██║██████╔╝██║     █████╗  ██████╔╝\n");
    iprintf("  ██║██╔══██╗    ╚════██║██╔══██║██║╚██╔╝██║██╔═══╝ ██║     ██╔══╝  ██╔══██╗\n");
    iprintf("  ██║██║  ██║    ███████║██║  ██║██║ ╚═╝ ██║██║     ███████╗███████╗██║  ██║\n");
    iprintf("  ╚═╝╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝╚═╝     ╚══════╝╚══════╝╚═╝  ╚═╝\n");
    iprintf("  by Federico Bocchieri\n");

}

void print_send(uint duration, uint num_samples) {
    iprintf("\n");
    iprintf("\t[ SEND MODE ]\tPress once to send\tLong press to switch mode\n\n");
    iprintf("\t\tLast sampled signal lasts %d ms with %d samples\n", duration, num_samples);
    iprintf("\n");
}

void print_receive() {
    iprintf("\n");
    iprintf("\t[ RECV MODE ]\tPress once to receive\tLong press to switch mode\n");
    iprintf("\n");

}

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


    using blue_button = Gpio<PC, 13>;

    // Input button configuration
    {
        FastGlobalIrqLock lock;
        blue_button::mode(Mode::INPUT);
    }

    // Definitions for button keypress
    enum keystate {PRESSED = 0, NOT_PRESSED = 1} prev_keystate, curr_keystate = NOT_PRESSED;

    uint longpress_threshold_ms = 0;
    const uint MODE_LONGPRESS_MS = 1000; // Wait for 1s before switching mode

    // Definitions for toggling mode
    enum mode { IR_RECEIVE, IR_SEND } mode = IR_RECEIVE;
    static bool long_press = false;

    PMDB_IR ir;

    // Infinite loop to read blue button keypress
    while(true) {

        cls();
        print_header();
        switch (mode) {
            case IR_RECEIVE:
            print_receive();
            break;

            case IR_SEND:
            print_send(ir.info_rec_len_ms(), ir.info_rec_num_samples());
        }

        // Poll for key press
        do {
            // Remember last key state
            prev_keystate = curr_keystate;
            curr_keystate = static_cast<keystate>(blue_button::value());

            // Look for a "rising edge"
            if (!(prev_keystate == NOT_PRESSED && curr_keystate == PRESSED)) {
                this_thread::sleep_for(10ms);
            } else {
                longpress_threshold_ms = getTime() / 1000000 + MODE_LONGPRESS_MS;
                break;
            };
        } while (true);

        // Poll for key release
        do {
            // Likewise
            prev_keystate = curr_keystate;
            curr_keystate = static_cast<keystate>(blue_button::value());

            if (!(prev_keystate == PRESSED && curr_keystate == NOT_PRESSED)) {  // Button still pressed
                // If time exceeds threshold, signal keypress and stop looking for "falling edges"
                uint curr_ts_ms = getTime() / 1000000;
                if (curr_ts_ms >= longpress_threshold_ms) {
                    long_press = true;
                    break;
                } else {
                    this_thread::sleep_for(10ms);
                }
            } else {    // Button released
                break;
            };
        } while (true);

        // Toggle IR mode in case of a long press and start reading button again
        if (long_press) {
            if (mode == IR_SEND)
                mode = IR_RECEIVE;
            else
                mode = IR_SEND;
            long_press = !long_press;
            iprintf("Switching to %s mode\n", mode == IR_SEND ? "send" : "receive");
            continue;
        }

        // Act upon current mode

        switch (mode) {
            case IR_RECEIVE:
            iprintf("\t\t\t\tReceiving IR...\n");
            ir.receive();
            mode = IR_SEND;
            break;

            case IR_SEND:
            iprintf("\t\t\t\tSending IR...\n");
            ir.send();
        }
    }
}
