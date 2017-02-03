LWCloneU2
=========

LWCloneU2 is an open source LedWiz emulator for Atmel AVR
microcontrollers (Arduinos and similar devices).  The project is
available under the GNU Public License (GPL).

This ZIP contains a Windows build of the project's LEDWIZ.DLL
replacement, which is a from-scratch reimplementation of the original
LEDWIZ.DLL from the manufacturer.  This replacement has several
advantages over the original, including using threads to write to the
USB connection.  It's also compatible with Pinscape V2 boards (the
original LEDWIZ.DLL crashes if the V2 keyboard features are enabled on
the Pinscape board).

The full source code for the original LWCloneU2 project, including the
Arduino firmware, the Windows DLL, various test and utility programs
is here:

https://github.com/cithraidt/lwcloneu2


The DLL here is a slightly modified version of cithraidt's work, with
changes to improve compatibility with third-party client software and
better accommodate the timing limitations of real LedWiz hardware.
The modified source code is in a fork of the repository, here:

https://github.com/mjrgh/lwcloneu2
