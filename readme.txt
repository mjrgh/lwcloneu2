Forked version - improved LEDWIZ.DLL
====================================

This is a forked version of cihraidt's LWCloneU2, to make some improvements to the LEDWIZ.DLL replacement.  cihraidt's LEDWIZ.DLL replacement faithfully implements the documented LedWiz API, but unfortunately doesn't emulate the exact behavior of the original DLL, and as a result some third-party client software won't run properly with it.  This modified version improves compatibility by more closely emulating the behavior of the original DLL.  

This version also attempts to better accommodate real LedWiz hardware by throttling the USB data rate.  cithraidt probably wasn't concerned with supporting real LedWiz hardware, since the whole point of the library is to provide access to his cloned version, but some virtual pinball people use a mix of real LedWiz hardware and emulator devices.  The real LedWiz has a serious firwmare problem that causes random glitches if USB messages are sent too quickly.  (It's not an inherent USB limitation or Windows problem or anything like that.  It's just a bug in the real LedWiz.  So it doesn't affect emulators with properly written USB interfaces, hence cithraidt wouldn't have encountered it when testing against his hardware.)  The only known workaround is to insert artificial delays of 5-10ms between messages to the LedWiz.  This modified version keeps track of the time of the last write and ensures that the next write will respect the minimum interval.  To minimize the impact on performance, this version also combines pending writes.  This means that if the client is sending updates faster than the hardware can accept them, intermediate updates that would be overwritten by subsequent updates are discarded.  This optimizes for synchronization: the device state reflects the real-time client state, rather than lagging behind as it tries to process a backlog of queued updates.  The trade-off is that fades aren't as smooth as they would be if all of the intermediate steps were processed, and too-rapid on/off switching might not have any visible effect at all.  For gaming systems this is the right trade-off, since it's more important for events to match in real time.  And there's simply no way around it: with a real LedWiz, you can either have a 200ms fade in 10 steps, or a 48-step fade in 2s, but you can't have a 48-step fade in 200ms.


LWCloneU2
=========

A firmware for Atmel AVR microcontroller for controlling LEDs or Light Bulbs via USB *and* a Joystick/Mouse/Keyboard Encoder.

The device is compatible with the LED-WIZ controller on the USB protocol level and thus can be used with many existing software. Additionally the firmware allows to add panel support, i.e. up to 4 yoysticks, 1 mouse, 1 keyboard and more. That is with one board you can get an input encoder and an LED output controller perfectly suited for MAME.

The LWCloneU2 project contains a compatible driver DLL "ledwiz.dll" replacement that fixes some bugs of the original one and does not block your main application, i.e. the I/O is fully asynchron.


Supported Hardware
==================
- Custom Breakout Board with ATMega32U2
- Arduino Leonardo (ATMega32U4)
- Arduino Mega 2560 (tested with Rev. 3)
- Arduino Uno Rev. 2/3 (untested)


Building the firmware
=====================

In order to build all this, you need a recent toolchain for AVR microcontroller, e.g. the 'AVR Toolchain 3.4.2-1573' from Atmel or the one that is bundled with the Atmel AVRStudio.

Get the sources from the Git repository, then do a 'git submodule update --init' in order to get the required LUFA (USB framework) sources. Then a 'make' should build the firmwares for all supported platforms.


Building the Windows DLL
========================

There are project files for Visualstudio 2008 Express and Visualstudio 2012 Express. The VS 2012 solution supports creating a 64 bit DLL.
