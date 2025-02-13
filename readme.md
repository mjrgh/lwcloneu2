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
communicate with the LedWiz (or other) device hardware, so it can
serve as a complete replacement for the manufacturer's DLL.

## What is LedWiz.dll?  Who needs it?

LedWiz.dll is a Windows program helper file that provides games and
other applications with access to the LedWiz hardware.  Most older
programs that were written to work with the LedWiz depend upon the DLL.
They can only access the LedWiz if this DLL is present, usually in the
same folder containing the program .EXE file.

Genuine LedWiz devices come with a version of LedWiz.dll provided by
the manufacturer.  This project provides a replacement DLL, built
entirely from scratch by open-source developers, based on details
about the LedWiz's USB protocol published on the Web.  The
manufacturer never officially published any information on the
protocol, since the DLL was intended to serve as the entire documented
public API, but individuals who were curious about how it worked
reverse-engineered the protocol and posted what they found on various
sites.  You can find a fairly thorough description in the Pinscape
KL25Z firmware source code, among other places.

Most of the modern virtual pin cab software (created in the last
ten years or so) can access the LedWiz, along with numerous other
similar I/O controllers, through another helper library called DOF.
DOF uses the same native USB protocol access that this replacement
DLL is based on, so it doesn't need a copy of LedWiz.dll - and
thus, programs that are based on DOF also don't need LedWiz.dll.
So you only need LedWiz.dll for older, legacy programs that were
never updated to use DOF.



## Why would you want to replace the manufacturer's DLL?

For several reasons:

* Multi-threading.  This DLL does its USB I/O on a background
thread, so that it never stalls the calling program waiting for
the USB transaction to complete.  The original manufacturer's DLL
uses blocking I/O, which forces the calling program to wait for
every USB write to complete.  This was especially noticeable in
Future Pinball when lots of effects fired at once, such as
the start of a multiball mode, which could make the program
freeze up for seconds at a time waiting for all of the USB
commands to complete.  And even during normal play, the USB
delays could cause an annoying amount of video stutter.  This
DLL sends all of its USB commands on a background thread, so
the game keeps running smoothly even when lots of LedWiz effects
are triggered.

* Better reliability.  This version works around a serious bug in the
original LedWiz hardware that causes genuine LedWiz's to glitch when
multiple USB updates are sent too rapidly.  The "glitch" manifests as
outputs randomly firing, which can be really annoying if the LedWiz is
driving noisy devices like solenoids.  This version of the DLL also
corrects some compatibility bugs in the original manufacturer's DLL
that make the older DLL crash when certain other software is running
or certain other device types are connected.

* Expanded device support.  The original manufacturer DLL is,
understandably, tied to the genuine LedWiz hardware.  This version
expands support to several other devices popular in the virtual pin
cab community, including Pinscape KL25Z, Pinscape Pico, and
Zebsboards.com's ZB Output Control.

The original motivation for developing the pin cab fork of the DLL was
for the expanded device support, and that's still the biggest
reason you'd want to seek it out.  But this DLL isn't just a clone,
it's an upgrade - it's better at doing what the original manufacturer's
DLL was designed to do than the original is.  It fixes that glitch
bug in the original hardware that we mentioned, and the threaded I/O
makes games run much more smoothly than with the original.


## Changes vs. the original cithraidt version:

* **Better compatibility with the manufacturer's DLL.** cihraidt's
original LedWiz.dll replacement faithfully emulates the
manufacturer's DLL as far as the official documentation goes, but
it nonetheless has some slight differences in the actual ad hoc
behavior, in a few areas that the official documentation leaves
unspecified.  For example, the order of device enumeration is
different.  Strictly speaking, cihraidt's version accomplished what it
set out to do, of emulating the documented API, but as a practical
matter, some extant third-party pin cab software happens to depend in
subtle ways on the exact ad hoc behavior of the original DLL, and
didn't work properly when confronted with cihraidt's version's slight
differences.  That kind of dependency on undocumented behavior is
certainly a bug in the third-party software, but the software in
question is closed source, and the people who wrote it don't care, and
they're not going to fix it.  So if we want that buggy old software to
keep working with a replacement DLL, it's up to the new DLL to more
perfectly replicate the original DLL's behavior.  So this version has
some fixes for the third-party software dependencies we've encountered
so far, making it more widely compatible with the commonly used
virtual pin cab software.

* **LedWiz hardware bug workaround.** The real LedWiz has a serious
bug in its firmware related to USB message timing that makes it behave
erratically when the PC sends USB commands too quickly.  When DOF
first became available, a lot of virtual pin cab users noticed that
DOF frequently made their LedWiz fire outputs at random.  This was
long believed to be due to some inherent USB timing limitation, or a
bug in the Windows USB driver, but after much investigation, it was
definitively traced to a defect in the LedWiz itself, most likely in
its firmware.  When the LedWiz receives a new command too quickly
after a prior one, the new command partially overwrites the old
command in the LedWiz's internal memory, causing random port state
changes due to the corrupted memory.  To work around the bug, this
version of the DLL detects when it's talking to a
genuine LedWiz, and automatically throttles USB traffic to that device
to one command per 10ms.  The throttling *isn't* applied to clones,
since all of the clones work perfectly fine no matter how fast you
throw commands at them.  So this DLL gives you the optimal handling
for whatever device you have: for a real LedWiz, it throttles the
USB traffic to a safe rate where the LedWiz won't glitch, and for
other devices, it runs commands at full speed, for minimum latency
carrying out effects.

* **Pinscape Controller compatibility.** The original manufacturer's
DLL has a defect that makes it crash (due to an internal memory corruption)
if it encounters a Pinscape KL25Z device with its keyboard input interface
enabled.  This is especially poor form because Groovy Game Gear uses an
unauthorized USB VID/PID identifier for their device, so they have no
business assuming exclusive use of the IDs.  If any other manufacturer
ever uses the same IDs, legitimately or not, the mere presence of that
other device will be likely to crash the original DLL in the same way.
This version of the DLL peacefully coexists with any non-LedWiz devices
sharing the same USB IDs.

* **Pinscape Pico support.** This DLL makes Pinscape Pico units appear
as virtual LedWiz units, letting you use a Pinscape Pico with pre-DOF
LedWiz-aware programs.  The original manufacturer DLL doesn't recognize
Pinscape Pico units.

* **Access to more than 32 Pinscape ports.** This version lets
ledwiz.dll clients access all ports on a Pinscape device with more
than 32 ports, by enumerating additional "virtual" LedWiz units to
represent the additional ports.  The virtual LedWiz units are numbered
consecutively after the actual unit number assigned to the Pinscape
device.  For example, if a Pinscape device is assigned to LedWiz unit #8,
and has 64 ports, it will appear to software as units #8 and #9.
Unit #9 represents the physical ports from 33 to 64.  This is
completely automatic; no configuration is needed to make this happen.
This applies to the original KL25Z Pinscape and Pinscape Pico.

* **ZB Output Control support.** This version recognizes ZB Output
Control devices (zebsboards.com), which the original DLL doesn't
recognize (because the ZB device uses a different VID).

**New LED Tester:** The project also includes **NewLedTester**, a
Windows GUI program that lets you see attached LedWiz units and
turn the output ports on and off manually.  This can be helpful
when testing a new setup.


## Building from source

The build was migrated in October 2024 to Visual Studio 2022.  Please
use VS 2022, and load the solution file **win32/LWCloneU2_vs2022.sln**.
Select Build > Build Solution from the main menu to build the DLL and
a small test program.

The win32/NewLedTester tree contains its own C# VS solution to build
that program separately.

## Firmware

For the purposes of this (mjr) fork, we're only interested in the
Windows DLL.  I haven't made any changes in the Arduino firmware, I
haven't built it, and I don't include it in the distribution.  If
your interest is in the firmware, you should refer to cihraidt's
original repository, since there might be newer versions of the
firmware since I created this fork.


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
[Correction: The build has been updated to VS 2022 -mjr, Oct 2024]
