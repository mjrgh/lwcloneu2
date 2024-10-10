// Pinscape Pico Device interface
// Copyright 2024 Michael J Roberts / BSD-3-Clause license / NO WARRANTY
//
// This module implements an API for interacting with the Pinscape
// Pico device via its USB control interfaces.  The API is designed
// for implementing programs like the Pinscape Pico Config Tool and
// other programs that automate Pinscape device configuration and
// control functions, and for programs (such as DOF) that access
// the Feedback Controller features.
//
// The Pinscape Pico device exposes several USB interfaces.  This
// API provides access to two of them:
//
// - A WinUsb vendor interface, which provides the Configuration
//   and Control functions.  This interface is designed for the
//   Pinscape Config Tool, and for any other applications that
//   need to configure the device.
// 
//   WinUsb is a native Windows device driver, so this interface
//   doesn't require the user to install any drivers; just plug
//   in the device and Windows will recognize this interface and
//   make it available to applications.  WinUsb by design only
//   allows one application to access the interface at a time,
//   which shouldn't be a hardship for Config Tool-like programs.
//   (WinUsb access doesn't lock out access to any of the OTHER
//   USB interfaces that the device exposes, so running the
//   Config Tool won't block DOF from accessing the device.)
// 
//   The vendor interface API also provides helpers for discovery
//   of Pico devices in their native "Boot Loader" mode, in which
//   the Pico emulates a USB thumb drive for the purpose of
//   installing new firmware into the Pico's flash memory.
//
//   See PinscapeVendorInterface.h for this interface.
// 
// - A Feedback Controller interface, which provides application
//   access to the feedback devices (lights, solenoids, etc)
//   attached to the Pinscape Pico's output ports.  This interface
//   is designed for use by DOF and other programs that generate
//   feedback effects.
// 
//   The Feedback Controller is a HID interface, which makes it
//   driverless and shareable (multiple applications can access
//   it concurrently).  That's why it's a separate interface
//   from the Configuration and Control interface.  The DOF-type
//   functions of the Feedback Controller interface are for use
//   by games and other UI-oriented programs, and the user is
//   likely to run more than one at a time, either because one
//   program launches another (e.g., PinballY launching VP),
//   or because the programs are taking turns in the foreground.
//   Allowing concurrent access to the DOF-type functions avoids
//   conflicts that can be difficult for users to troubleshoot.
// 
//   See FeedbackControllerInterface.h for this interface.
// 

#pragma once

#include <stdlib.h>
#include <string.h>
#include <tchar.h>
#include <memory>
#include <functional>
#include <list>
#include <string>
#include <Windows.h>
#include <winusb.h>
#include <usb.h>
#include "../USBProtocol/VendorIfcProtocol.h"
#include "../USBProtocol/FeedbackControllerProtocol.h"

// Short-hand names for the PinscapePico types
using PinscapeRequest = PinscapePico::VendorRequest;
using PinscapeResponse = PinscapePico::VendorResponse;
using FeedbackRequest = PinscapePico::FeedbackControllerRequest;
using FeedbackReport = PinscapePico::FeedbackControllerReport;

namespace PinscapePico
{
	// String of TCHAR/WCHAR
	using TSTRING = std::basic_string<TCHAR>;
	using WSTRING = std::basic_string<WCHAR>;

	// Pico hardware ID
	struct PicoHardwareId
	{
		PicoHardwareId() { memset(b, 0, sizeof(b)); }
		PicoHardwareId(const uint8_t src[8]) { memcpy(this->b, src, 8); }

		// Clear to all zeroes.  This serves as a null ID.  (There's
		// no documentation anywhere saying that all-zeroes is actually
		// a reserved ID that will never be used in a device, but it
		// seems incredibly unlikely that it would be, and contrary to
		// all industry conventions and common sense.)
		void Clear() { memset(b, 0, sizeof(b)); }

		// The 8 bytes of the ID.  Note that these are binary byte
		// values, each 0x00..0xFF - they're NOT printable ASCII or
		// anything like that.
		uint8_t b[8];

		// Get the string representation of the ID, as a series
		// of 16 hex digits.
		std::string ToString() const;

		// match against another hardware ID object
		bool operator ==(const PicoHardwareId &other) const { return memcmp(b, other.b, sizeof(b)) == 0; }
	};

	// Pinscape Pico device ID.  This is a collection of 
	// identifiers that can be used to select a device or display
	// device listings.
	struct DeviceID
	{
		// The Pico's hardware ID.  This is a universally unique 
		// 64-bit value that's assigned to each Pico at the factory
		// and burned into ROM.  This ID is immutable and unique for
		// each Pico, so it can be used to identify a physical Pico
		// across firmware updates, USB port changes, etc.
		PicoHardwareId hwid;

		// Pico RP2040 CPU version
		uint8_t cpuVersion = 0;

		// Pico RP2040 ROM version
		uint8_t romVersion = 0;

		// ROM version name, per the nomenclature used in the SDK
		std::string romVersionName;

		// Pinscape Pico Unit Number.  This is a small integer that
		// identifies the device locally, to distinguish it from
		// other Pinscape Pico devices on the same system.  The unit
		// number is assigned by the user in the JSON configuration.
		// It's usually set to 1 for the first unit in the system,
		// and sequentially from 2 for any additional units.  This
		// is the ID that DOF uses.
		int unitNum = -1;

		// LedWiz emulation unit number, 1-16, or 0 to disable.
		// This is the nominal unit number that should be assigned
		// to this device by a host-side LedWiz emulation layer,
		// typically implemented on Windows as a replacement DLL
		// for the original LEDWIZ.DLL supplied by Groovy Game Gear.
		// Pinscape Pico doesn't implement the LedWiz's custom USB 
		// HID protocol, so it doesn't emulate an LedWiz for the 
		// purposes of programs that access the device directly
		// through its USB interface.  However, Groovy Game Gear
		// deliberately left the USB interface undocumented, and
		// instead published an in-process C function-call API 
		// through a DLL they supplied, called LEDWIZ.DLL, which
		// was explicitly designated as the public interface to
		// the device.  Nearly all LedWiz-aware applications
		// therefore access the device through the DLL, hence it's
		// possible to implement LedWiz emulation purely through a
		// replacement DLL, without any need to replicate the USB
		// protocol.  A replacement DLL implementing Pinscape Pico
		// access (as well as access to genuine LedWiz devices) is 
		// available at https://github.com/mjrgh/lwcloneu2/
		// 
		// This LedWiz unit number property is a helper for the
		// emulation DLL, telling the DLL the desired unit number 
		// to assign to the virtual LedWiz unit that the DLL creates 
		// to represent the Pico, per the Pico's JSON configuration.
		// 
		// A single Pico might actually need multiple LedWiz unit
		// numbers, since a single LedWiz unit can only represent
		// 32 output ports; a Pico with more than 32 logical output
		// ports thus requires two or more virtual LedWiz units to
		// represent it through the LedWiz API.  Our convention is
		// that the emulation layer will assign consecutive unit
		// numbers starting at the number given here; for example,
		// if the Pico has 96 output ports, requiring three virtual
		// LedWiz units to represent it, and the value here is 8,
		// the virtual LedWiz's are assigned unit numbers 8, 9, 
		// and 10.
		//
		// We initialize this to -1 to indicate that no value was
		// read from the device.  The device reports a value of 0
		// to mean that LedWiz emulation should be disabled for the
		// device; otherwise, the value is 1 to 16, corresponding
		// to LedWiz USB VID/PID values of FAFA/00F0 through 00FF.
		int ledWizUnitNum = -1;

		// XInput interface Player Number.  This is the player index
		// assigned by the host to the Pinscape device's virtual XBox
		// controller interface.  If the device isn't configured to
		// emulate the XInput device, or it hasn't received its player
		// number assignment from the host, this is -1.  If valid,
		// this is a value from 0 to 3.  The Windows documentation
		// says that the Player Number is stable for at least the
		// duration of the USB connection.  This can be used to
		// figure out which XInput interface on the host side
		// corresponds to the Pinscape virtual device, when other
		// XInput devices are plugged in at the same time.
		int xinputPlayerIndex = -1;

		// Unit Name.  This is a short descriptive name assigned by 
		// the user in the JSON configuration, purely for display
		// purposes.  It's mostly for use in device listings, to
		// help the user keep track of which device is which when
		// multiple Pinscape Pico units are present.
		std::string unitName;
	};

	// Vendor Interface descriptor object.  The device enumerator returns a 
	// list of these objects representing the connected devices.  The object
	// can then be used to open a live USB connection to the physical device.
	class VendorInterface;
	class VendorInterfaceDesc
	{
		// Note that the constructor is private, for use by the enumerator
		// in the base class.  We let the enumerator access it by making the
		// parent class a friend, and we let std::list's emplace() access it
		// via the private ctor key type that only friends can use.
		friend class VendorInterface;
		struct private_ctor_key_t {};

	public:
		// construct - called from the enumerator
		VendorInterfaceDesc(private_ctor_key_t, const WCHAR *path, size_t len, const WCHAR *deviceInstanceId) :
			path(path, len),
			deviceInstanceId(deviceInstanceId)
		{ }

		// the device's file system name as a string
		const WCHAR *Name() const { return path.c_str(); }

		// get the Win32 device instance ID for the underlying device
		const WCHAR *DeviceInstanceId() const { return deviceInstanceId.c_str(); }

		// Find the CDC (virtual COM) port associated with this device.
		// Pinscape Pico can be configured to expose a CDC port that can
		// be used to access log messages and command console functions
		// via a terminal window.  The CDC port appears in Windows as a
		// virtual COMn port.  The COMn port number can't be configured
		// by the user, since it's assigned automatically by Windows when
		// the device first connects.  This function lets you identify
		// that system-assigned port number given a device path.
		// 
		// If the device exposes a CDC port, this fills in 'name' with
		// the COMn port name string and returns true.  If no CDC port
		// is found (or any error occurs), the function returns false.
		//
		// The COM port name returned is of the form "COMn", where n
		// is a number.  This is the format that most user interfaces
		// use to display available ports and accept as input to
		// select a port.  You can also use this name in many Windows
		// system calls involving COM ports, either directly as the
		// string or by extracting the number suffix.  Note that the
		// number might be more than one digit, since it's possible
		// to add quite a lot of virtual COM ports.  For CreateFile(),
		// prepend the string "\\\\.\\" to the name returned.
		bool GetCDCPort(TSTRING &name) const;

		// Open the path to get a live handle to a device
		HRESULT Open(VendorInterface* &device);
		HRESULT Open(std::unique_ptr<VendorInterface> &device);
		HRESULT Open(std::shared_ptr<VendorInterface> &device);

		// Get the VID/PID for the device
		HRESULT GetVIDPID(uint16_t &vid, uint16_t &pid);

	protected:
		// File system path to the device
		WSTRING path;

		// Device Instance ID.  This is a unique identifier for
		// the device that Windows assigns.  It serves as the device
		// identifier in some system APIs.
		WSTRING deviceInstanceId;
	};

	// IR command description, for transmission, and for identifying
	// a received command.  A command description represents a particular
	// button on a particular remote control device; it contains the
	// information that the firmware needs to recognize that button's
	// transmissions on the receiver, and to mimic the button's code
	// for transmission via the IR emitter.
	struct IRCommand
	{
		IRCommand() { }
		IRCommand(const IRCommand &c) :
			protocol(c.protocol), flags(c.flags), command(c.command) { }
		IRCommand(uint8_t protocol, uint8_t flags, uint64_t command) :
			protocol(protocol), flags(flags), command(command) { }

		// The code elements are defined by the Pinscape firmware's IR
		// subsystem.  Refer to ../firmware/IRRemote/IRCommand.h for
		// details on the meanings of these fields.  Most applications
		// consider the whole IR command structure to be opaque, since
		// an application can obtain any codes that it wants to recognize
		// or transmit by reading them from the receiver.  At that level,
		// an IRCommand structure amounts to an opaque unique identifier
		// for a particular button on a particular remote control.
		// Codes can be converted to and from strings
		uint8_t protocol = 0;     // IR protocol ID
		uint8_t flags = 0;        // protocol flags
		uint64_t command = 0;     // command code

		// test for equality
		bool operator ==(const IRCommand &c) const {
			return protocol == c.protocol && flags == c.flags && command == c.command ;
		}

		// Build the string representation in our universal format.  The
		// resulting string can be used as input to IRCommand::Parse().
		std::string ToString() const;

		// Parse a string from our universal format: xx.xx.xxxxxxxx, all 
		// hex digits, for the protocol ID, flags, and command code.  The
		// command code can range from 4 to 16 hex digits (for 16 to 64 
		// bits).  Returns true on success, false if the format is invalid.
		bool Parse(const char *str, size_t len);
		bool Parse(const char *str) { return Parse(str, strlen(str)); }
		bool Parse(const std::string &str) { return Parse(str.c_str()); }
	};

	// IR Command Received.  This extends an IRCommand with additional
	// information about the specific bit sequence received for an
	// individual command.
	struct IRCommandReceived : IRCommand
	{
		// Compare against another report for equality.  Two codes
		// match if they have the same protocol and command code.
		// Note that the flags don't have to match, since those
		// might vary during auto-repeats.  Even the "protocol
		// uses dittos" flag might vary, because we can't know if
		// the specific device uses dittos - even if its protocol
		// has a defined ditto format - until we see a ditto
		// actually used in an auto-repeat.
		bool operator==(const IRCommandReceived &other) const {
			return protocol == other.protocol && command == other.command;
		}

		uint64_t elapsedTime_us = 0;    // elapsed time since previous command, in microseconds

		bool proHasDittos = false;      // protocol uses dittos

		bool hasDitto = false;          // ditto bit is valid for this command
		bool ditto = false;             // ditto bit

		bool hasToggle = false;         // toggle bit is valid for this command
		bool toggle = false;            // toggle bit

		bool isAutoRepeat = false;      // code is an auto-repeat of the previous command, due to
										// the user holding down the button

		uint8_t posCode = POS_NULL;     // position code encoded in this transmission
		static const uint8_t POS_NULL   = 0;   // no position code present
		static const uint8_t POS_FIRST  = 1;   // first transmission for a new key press
		static const uint8_t POS_MIDDLE = 2;   // second+ transmission, for a key being held down
		static const uint8_t POS_LAST   = 3;   // last transmission for a key that was just released

		// command flags from the report; this might vary from the
		// protocol flags, since some protocol features can't be
		// sensed until a command is auto-repeated
		uint8_t cmdFlags = 0;
	};

}


