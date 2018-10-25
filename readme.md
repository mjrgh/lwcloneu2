An LedWiz.dll replacement DLL based on LWCloneU2
================================================

This is a forked version of LWCloneU2 by cithraidt, with improvements
to that project's ledwiz.dll replacement specifically designed for
virtual pinball cabinet users.  This fork doesn't have any changes
to the Arduino firmware; all changes are on the Windows side.

The original LWCloneU2 was mostly Arduino firmware, but it also happened to
include a nice drop-in replacement DLL for the ledwiz.dll that ships
with the commercial LedWiz hardware.  The manufacturer's DLL provides
a simple software interface for enumerating LedWiz devices and sending
commands to the device.  The replacement DLL in this project provides
the same software interface, and uses the same USB protocol to
communicate with the LedWiz (or other) device hardware, so it's a
simple plug-and-play replacement for the manufacturer's DLL.

Why would you want to replace the manufacturer's DLL?  Several reasons:

* The manufacturer's DLL is closed-source; this version is open-source.
You can see what it's doing and fix any problems yourself if necessary.

* This version is multi-threaded.  USB I/O is done on a background
thread so that it never stalls the calling program.  The original
manufacturer's DLL makes the calling program wait for USB I/O to
complete, which can cause video stutter in games like Future Pinball.

* This version has some pin cab-related improvements and bug fixes (see below).


The new DLL is compatible with real LedWiz hardware, as well as some
alternative output controllers designed for visual pinball cabinets,
specifically the Pinscape Controller and Zebsboards.com ZB Output
Control.  It's actually *better* at controlling real LedWiz hardware
than the manufacturer's DLL, because this version has a workaround for
a serious firmware bug in the LedWiz hardware that can cause erratic
behavior with the original manufacturer's DLL.

Summary of changes vs the original cithraidt version:

* **Better compatibility with the manufacturer's DLL.**  cihraidt's
original ledwiz.dll replacement faithfully implements the *documented*
LedWiz API, but it has some slight differences in its ad hoc behavior
compared to the manufacturer's version, such as the order of device
enumeration.  Some third-party pin cab software that uses the LedWiz
interface happens to be sensitive to some of these details.  Strictly
speaking, that kind of dependency on undocumented behavior is a bug in
the third-party software, but the software in question is closed
source, and the people who wrote it aren't going to fix it.  This
version of the DLL emulates some of that ad hoc behavior more
precisely and so is more widely compatible with buggy client software.

* **LedWiz hardware bug workarounds.** The real LedWiz has a serious
bug in its firmware related to USB message timing that makes it behave
erratically when the PC sends USB commands too quickly.  (This *isn't*
an inherent USB timing limitation or a Windows driver issue, as many
people believe.  It's purely a bug in the LedWiz firmware.  Other
devices, such as Pinscape, ZB Output Control, and LWCloneU2-based
devices, don't suffer from the problem.)  This version of the DLL
works around this timing bug in the LedWiz hardware by throttling
the command rate to ensure a minimum 10ms interval between commands.
The throttling is *only* used when real LedWiz hardware is detected;
when other devices are recognized, commands are sent as quickly as
the client software generates them.

* **Pinscape Controller support.** The original manufacturer's DLL
crashes when a Pinscape devices is attached with its keyboard input
interface enabled.  This version works happily with all Pinscape
device configurations.  

* **Access to more than 32 Pinscape ports.** This version lets
ledwiz.dll clients access all ports on a Pinscape device with more
than 32 ports, by enumerating additional "virtual" LedWiz units to
represent the additional ports.  The virtual LedWiz units are numbered
consecutively after the actual unit number assigned to the Pinscape
device.  For example, if a Pinscape device is assigned to LedWiz unit
#8, and has 64 ports, it will appear to software as units #8 and #9.
Unit #9 represents the physical ports from 33 to 64.

* **ZB Output Control support.** This version recognizes ZB Output
Control devices (zebsboards.com), which use a different USB Vendor ID
(VID) than the original LedWiz.

**New LED Tester:** The project also includes **NewLedTester**, a
Windows GUI program that lets you see attached LedWiz units and
turn the output ports on and off manually.  This can be helpful
when testing a new setup.


**Building:** The Visual Studio project for building the DLL has been
updated to VS 2017.


Original Readme
===============

**LWCloneU2**

A firmware for Atmel AVR microcontroller for controlling
LEDs or Light Bulbs via USB *and* a Joystick/Mouse/Keyboard Encoder.

The device is compatible with the LED-WIZ controller on the USB
protocol level and thus can be used with many existing software.
Additionally the firmware allows to add panel support, i.e. up to 4
yoysticks, 1 mouse, 1 keyboard and more. That is with one board you
can get an input encoder and an LED output controller perfectly suited
for MAME.

The LWCloneU2 project contains a compatible driver DLL "ledwiz.dll"
replacement that fixes some bugs of the original one and does not
block your main application, i.e. the I/O is fully asynchron.


Supported Hardware
==================
- Custom Breakout Board with ATMega32U2
- Arduino Leonardo (ATMega32U4)
- Arduino Mega 2560 (tested with Rev. 3)
- Arduino Uno Rev. 2/3 (untested)


Building the firmware
=====================

In order to build all this, you need a recent toolchain for AVR
microcontroller, e.g. the 'AVR Toolchain 3.4.2-1573' from Atmel or the
one that is bundled with the Atmel AVRStudio.

Get the sources from the Git repository, then do a 'git submodule
update --init' in order to get the required LUFA (USB framework)
sources. Then a 'make' should build the firmwares for all supported
platforms.


Building the Windows DLL
========================

There are project files for Visualstudio 2008 Express and Visualstudio
2012 Express. The VS 2012 solution supports creating a 64 bit DLL.
