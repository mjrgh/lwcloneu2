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

* Open source.  This replacement DLL is open-source, so you're free
to fix bugs and make improvements.  The manufacturer's DLL is
proprietary closed-source.

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

* Expanded device support.  The original manufacturer DLL is, naturally,
tied to the genuine LedWiz hardware.  This version expands support to
several other devices popular in the virtual pin cab community, including
Pinscape KL25Z, Pinscape Pico, and Zebsbboards.com's ZB Output Control.

The original motivation for developing the pin cab fork of the DLL was
for the expanded device support, and that's still the biggest
reason you'd want to seek it out.  But this DLL isn't just a clone,
it's an upgrade - it's better at doing what the original manufacturer's
DLL was designed to do than the original is.  It fixes that glitch
bug in the original hardware that we mentioned, and the threaded I/O
makes games run much more smoothly than with the original.


## Changes vs. the original cithraidt version:

* **Better compatibility with the manufacturer's DLL.**  cihraidt's
original ledwiz.dll replacement faithfully implements the *documented*
LedWiz API, but it has some slight differences in its ad hoc behavior
compared to the manufacturer's version, such as the order of device
enumeration.  Some third-party pin cab software that uses the LedWiz
interface happens to be sensitive to some of these details.  Strictly
speaking, that kind of dependency on unspecified behavior is a bug in
the third-party software, but the software in question is closed
source, and the people who wrote it don't care and aren't going to
fix it, so it's up to us to smooth things over.
So this version of the DLL emulates some of that known ad hoc behavior
of the original manufacturer DLL more exactly than cithraidt's base
version does, making it more widely compatible with more of the buggy
software that people want to run.

* **LedWiz hardware bug workaround.** The real LedWiz has a serious
bug in its firmware related to USB message timing that makes it behave
erratically when the PC sends USB commands too quickly.  When DOF first
became available, a lot of virtual pin cab users noticed that DOF
frequently made their LedWiz fire outputs at random.  This was long
believed to be due to some inherent USB timing limitation, or a bug
in the Windows USB driver, but after much investigation, it was
definitively traced to a defect in the LedWiz itself, most likely in
its firmware.  When the LedWiz receives a new command too quickly
after a prior one, the new command partially overwrites the old
command in the LedWiz's internal memory, causing random port state
changes due to the corrupted memory.  To work around the bug, this
version of the DLL automatically detects when it's talking to a genuine
LedWiz, and automatically throttles USB traffic to that device to one
command per 10ms.  The throttling *isn't* applied to clones, since none
of the clones suffer from the bug.  So the DLL gives you the best of both:
the real LedWiz won't glitch with this DLL, and other devices run at full speed.

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

* **ZB Output Control support.** This version recognizes ZB Output
Control devices (zebsboards.com), which use a different USB Vendor ID
(VID) than the original LedWiz.

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
