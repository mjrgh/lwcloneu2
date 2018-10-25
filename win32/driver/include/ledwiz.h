/************************************************************************************************************************

LEDWIZ.DLL API

This DLL provides a compatible replacement for the original LEDWIZ.DLL distributed by
the device's manufacturer.  It was developed as part of the LWCloneU2 pro, an LedWiz
emulator for Arduino devices.

The replacement DLL is a drop-in replacement for the original manufacturer's version.
It also offers some improvements and extended functionality:

  - USB writes are handled asynchronously in a background thread, allowing the
    caller to continue running immediately without waiting for USB I/O to complete.
	The original LEDWIZ.DLL handles writes synchronously, forcing callers to wait
	for I/O to complete, which can cause visible stutter in on-screen animation
	when used from a game.  The asynchronous writes should eliminate this and allow
	much smoother game play.

  - USB messages to real LedWiz devices are automatically and transparently paced
    to work around timing limitations in the real LedWiz firmware.  Real LedWiz
	devices get confused if messages are sent too quickly, turning the wrong ports
	on and off.  This problem doesn't affect the emulators (LwCloneU2 and Pinscape),
	so the DLL detects the type of physical device connected and adjusts the message
	timing accordingly.

  - For Pinscape Controllers with more than 32 outputs, the DLL creates one or more
    "virtual" LedWiz interfaces.  This allows callers that are capable of addressing
	multiple LedWiz devices to access all Pinscape outputs, by making it look like
	the Pinscape device's outputs are spread out over several LedWiz devices.

  - This version of the DLL works correctly with Pinscape Controller devices.  The 
    original LEDWIZ.DLL crashes if the Pinscape keyboard features are enabled,
	because it can't differentiate the extra USB interfaces that the device
	creates for the keyboard from the control interface.

  - New "raw" I/O functions allow LwCloneU2-aware clients to access extended
    functionality in that device.

The exported API is backward-compatible with the original LEDWIZ.DLL interface.
Functions marked with [EXTENDED API] in the comments below are added functions
that don't exist in the original API.  The extra functions won't affect callers
written for the original API.

************************************************************************************************************************/



#ifndef LEDWIZ_H__INCLUDED
#define LEDWIZ_H__INCLUDED

#include <windows.h>


#if defined(_MSC_VER)

#define LWZCALL __cdecl		// this is what the 'original' ledwiz.dll uses
#define LWZCALLBACK __stdcall

#if (_MSC_VER >= 1600) // starting with VisualStudio 2010 stdint.h is available
#include <stdint.h>
#else
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
#endif

#else

#include <stdint.h>
#define LWZCALL
#define LWZCALLBACK

#endif


#ifdef __cplusplus
extern "C" {
#endif


// Maximum number of devices that can be attached to the system at one time
#define LWZ_MAX_DEVICES 16

// Notification callback 'reason' codes
typedef enum {
	LWZ_REASON_ADD     = 1,   // device was added (used on initial discovery and when a new device is plugged in)
	LWZ_REASON_DELETE  = 2    // device was removed (used when an existing device is unplugged)
} LWZ_NOTIFY_REASON;


// Handle to LedWiz device
typedef int32_t LWZHANDLE;

// Caller-allocated device list.  The DLL hangs onto this structure and can
// make changes to it when processing Windows messages.  The DLL invokes the
// notification callback after making any changes.
typedef struct {
	LWZHANDLE handles[LWZ_MAX_DEVICES];
	int32_t numdevices;
} LWZDEVICELIST;

// Device types - used in LWZDEVICEINFO
#define LWZ_DEVICE_TYPE_NONE           0     // no device present
#define LWZ_DEVICE_TYPE_LEDWIZ         1     // LedWiz or unknown emulator
#define LWZ_DEVICE_TYPE_LWCLONEU2      2     // LwCloneU2
#define LWZ_DEVICE_TYPE_PINSCAPE       3     // Pinscape Controller
#define LWZ_DEVICE_TYPE_PINSCAPE_VIRT  4     // Pinscape Controller virtual LedWiz for extended ports
#define LWZ_DEVICE_TYPE_ZB             5     // ZB Output Control (zebsboards.com)

// Device description - used in LWZ_GET_DEVICE_INFO
typedef struct {
	DWORD cbSize;			// structure size
	DWORD dwDevType;        // device type (LWZ_DEVICE_TYPE_xxx constant)
	char szName[256];		// device name, from USB device descriptor
} LWZDEVICEINFO;


/************************************************************************************************************************
LWZ_SBA - All Outputs State plus Global Pulse Speed
*************************************************************************************************************************
handle is an idetifier for a specific LED-WIZ device
Values bank0, bank1, bank2, and bank3 equal 8-bit representations of on/off states for banks 1-4. 
Value globalPulseSpeed equals Global Pulse Speed setting (1 through 7).
************************************************************************************************************************/

void LWZCALL LWZ_SBA(
	LWZHANDLE hlwz, 
	unsigned int bank0, 
	unsigned int bank1,
	unsigned int bank2,
	unsigned int bank3,
	unsigned int globalPulseSpeed);


/************************************************************************************************************************
LWZ_PBA - All Outputs Profile Settings (Most Efficient): 
*************************************************************************************************************************
handle is an idetifier for a specific LED-WIZ device
Each of the 32 parameters coincide with outputs 1-32. A value of 1 to 48 sets the brightness of each LED using PWM. 
A value of 129-132 indicates an automated pulse mode as follows:
129 = Ramp Up / Ramp Down
130 = On / Off
131 = On / Ramp Down
132 = Ramp Up / On
The speed is contolled by the Global Pulse Speed parameter.
************************************************************************************************************************/

void LWZCALL LWZ_PBA(LWZHANDLE hlwz, uint8_t const *pmode32bytes);


/************************************************************************************************************************
LWZ_REGISTER - Register device for plug and play
*************************************************************************************************************************
This must be called with the hwnd that an application uses to process windows messages. 
This associates a device with a window message queue so the your application can be notified of plug/unplug events.
In order to unregister, call with hwnd == NULL.
You have to unregister if the library was manually loaded and then is going to be freed with FreeLibrary() while
the window still exists.
************************************************************************************************************************/

void LWZCALL LWZ_REGISTER(LWZHANDLE hlwz, HWND hwnd);


/************************************************************************************************************************
LWZ_SET_NOTIFY - Set notifcation mechanisms for plug/unplug events
LWZ_SET_NOTIFY_EX - same as LWZ_SET_NOTIFY, but provides a user defined pointer in the callback [EXTENDED API]
*************************************************************************************************************************
Set a notification callback for plug/unplug events. 
It searches for all connected LED-WIZ devices and then calls the notify callback for it.
The callback will come back directly from this call or later from the windows procedure thread of the caller,
that is as long as you do your stuff from the same windows procedure there will be no need for thread synchronization. 
At the same time the notification procedure is called, the LWZDEVICELIST
will be updated with any new device handles that are required.
This function is also used to set a pointer to your applications device list. 
************************************************************************************************************************/

typedef void (LWZCALLBACK * LWZNOTIFYPROC)(int32_t reason, LWZHANDLE hlwz);
typedef void (LWZCALLBACK * LWZNOTIFYPROC_EX)(void *puser, int32_t reason, LWZHANDLE hlwz);

void LWZCALL LWZ_SET_NOTIFY(LWZNOTIFYPROC notify_callback, LWZDEVICELIST *plist);
void LWZCALL LWZ_SET_NOTIFY_EX(LWZNOTIFYPROC_EX notify_ex_callback, void *puser, LWZDEVICELIST *plist);


/************************************************************************************************************************
LWZ_GET_DEVICE_INFO - retrieve information on a device [EXTENDED API]
*************************************************************************************************************************
Retrieves information on the given device, filling in the caller-allocated
structure.  Per the usual Windows conventions, the caller must fill in the
'cbSize' field of the result structure before invoking the function; this
allows for new fields to be added to the structure in the future, since the
routine can tell from the structure size which version of the structure the
caller is using.  Returns TRUE if the device was valid, FALSE if not.
************************************************************************************************************************/

BOOL LWZ_GET_DEVICE_INFO(LWZHANDLE hlwz, LWZDEVICEINFO *info);


/************************************************************************************************************************
LWZ_RAWWRITE - write raw data to the device [EXTENDED API]
*************************************************************************************************************************
return number of bytes written.
************************************************************************************************************************/

uint32_t LWZ_RAWWRITE(LWZHANDLE hlwz, uint8_t const *pdata, uint32_t ndata);

/************************************************************************************************************************
LWZ_RAWREAD - read raw data from the device [EXTENDED API]
*************************************************************************************************************************
return number of bytes read.
************************************************************************************************************************/

uint32_t LWZ_RAWREAD(LWZHANDLE hlwz, uint8_t *pdata, uint32_t ndata);


#ifdef __cplusplus
}
#endif

#endif
