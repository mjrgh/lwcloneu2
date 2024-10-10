# An LedWiz.dll replacement DLL based on LWCloneU2

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

* This version is open-source.  The manufacturer's is proprietary
closed-source.

* Multi-threading.  This DLL does its USB I/O on a background
thread, so that it never stalls the calling program waiting for
the USB transaction to complete.  The original manufacturer's DLL
uses blocking I/O, which forces the calling program to wait for
every USB write to complete.  This can cause video stutter and
UI lag in games like Future Pinball, and it can even stall the
game for seconds at a time during intense bursts of feedback
device effect activity, such as when a multiball mode or other
event triggers a big light show in the game.  This version of the
DLL never blocks the main thread with USB I/O, allowing the game 
to run smoothly even when heavy USB activity is going on.

* Better reliability.  This version works around a bug in the original 
LedWiz hardware that causes genuine LedWiz's to glitch when multiple USB 
updates are sent too rapidly.  It also fixes some bugs in the original
manufacturer's DLL (or, more accurately, never had the bugs in the
first place) that make the original DLL crash with some software or
when some other types of devices are present in the system.


The new DLL is compatible with real LedWiz hardware, as well as some
alternative output controllers designed for visual pinball cabinets,
specifically the Pinscape Controller and Zebsboards.com ZB Output
Control.  It's actually *better* at controlling real LedWiz hardware
than the manufacturer's DLL, because it has a workaround for
a serious firmware bug in the LedWiz hardware that can cause erratic
behavior with the original manufacturer's DLL, plus it has threaded
USB I/O that doesn't stall the calling program while USB commands
are being sent.

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

* **LedWiz hardware bug workaround.** The real LedWiz has a serious
bug in its firmware related to USB message timing that makes it behave
erratically when the PC sends USB commands too quickly.  When DOF first
became available, a lot of virtual pin cab users noticed that DOF
frequently made their LedWiz fire outputs at random.  This was long
believed to be due to some inherent USB timing limitation, or a bug 
in the Windows USB driver, but after much investigation, it was
definitively traced to a bug in the LedWiz itself, most likely in
its firmware.  When the LedWiz receives a new command too quickly
after a prior one, the new command partially overwrites the old 
command in the LedWiz's internal memory, causing random port state
changes due to the corrupted memory.  To work around the bug, this 
version of the DLL automatically detects when it's talking to a genuine 
LedWiz, and automatically throttles USB traffic to that device to one 
command per 10ms.  The throttling *isn't* applied to clones, since none
of the clones suffer from the bug.  So the DLL gives you the best of both:
the real LedWiz won't glitch with this DLL, and other devices run at full speed.

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
Unit #9 represents the physical ports from 33 to 64.  This is
completely automatic; no configuration is needed to make this happen.

* **ZB Output Control support.** This version recognizes ZB Output
Control devices (zebsboards.com), which use a different USB Vendor ID
(VID) than the original LedWiz.

**New LED Tester:** The project also includes **NewLedTester**, a
Windows GUI program that lets you see attached LedWiz units and
turn the output ports on and off manually.  This can be helpful
when testing a new setup.

## Building

The build was migrated in October 2024 to Visual Studio 2022.  Please
use VS 2022, and load the solution file **win32/LWCloneU2_vs2022.sln**.
Select Build > Build Solution from the main menu to build the DLL and
a small test program.

The win32/NewLedTester tree contains its own C# VS solution to build
that program separately.


# Original Readme

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
block your main application, i.e. the I/O is fully asynchronous.


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
[Correction: The build has been updated to VS 2017 -mjr, Oct 2018]
