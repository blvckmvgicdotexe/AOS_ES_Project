# README

## Project description

This project implements a tool to sample and replay digital signals via infrared such as those sent by TV / AC remotes and the like.

This project is shared among the courses of *Advanced Operating Systems* by prof. Vittorio Zaccaria and *Embedded Systems* by prof. William Fornaciari and is supervised by prof. Federico Terraneo and prof. Daniele Cattaneo.

It runs on the Miosix RTOS (maintained by prof. Terraneo and Cattaneo) on a STM32 Nucleo F401RE board paired with a shield specifically manufactured for the *Sensor Systems* course by prof. Federica Villa.

## Miosix 3.00 setup instructions

To setup a sample project on Linux:
- Install ST-Link to detect and flash the STM32 board either via Github [here](https://github.com/stlink-org/stlink) or via your distro's package manager 
- Install the latest version of the Miosix toolchain, available [here](https://miosix.org/wiki/index.php?title=Miosix_Toolchain) , which is version 15.2.0mp4.2 at the time of writing
- Create an empty git repository for the project (`git init`) and clone the Miosix kernel as a submodule (`git submodule add https://miosix.org/git-public/miosix-kernel.git`), `cd` into the submodule's folder and checkout the `unstable` branch for the latest bug fixes
- `cd` into the root of your repo and initialise an empty template project by running a perl script with `./miosix-kernel/tools/init_project_out_of_git_tree.pl`
  > Note: make sure to have both perl and the `perl-file-copy-recursive` module installed
- Choose your board model in `config/Makefile.inc` that was just created
- Finally run `make` to compile the empty `main.cpp` 

The result of this process is a file `main.bin` which can be flashed on the board with `make program`

After flashing, unplug and plug the board and run `virtualcom.sh`, then press the reset button on board to be greeted with this:
```
$ ./virtualcom.sh
Starting Kernel... Ok

Miosix v3.01 (stm32f401re_nucleo, Jul 17 2026 02:45:01, gcc 15.2.0-mp4.2)

OS Timer freq = 84000000 Hz

Available heap 88736 out of 94472 Bytes
```

> Note: you may want to tweak `virtualcom.sh` according to your setup; in my case, the only `tty` device that is connected is `ttyACM0`
