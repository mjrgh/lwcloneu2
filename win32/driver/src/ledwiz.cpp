/*
 * LWCloneU2
 * Copyright (C) 2013 Andreas Dittrich <lwcloneu2@cithraidt.de>
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory>
#include <regex>

#include <windows.h>
#include <crtdbg.h>
#include <Setupapi.h>
#include <Shlwapi.h>

#include "PinscapePico/WinAPI/FeedbackControllerInterface.h"

#pragma comment(lib, "PinscapePicoAPI")

extern "C" {
#include <Hidsdi.h>
}

#include <Dbt.h>

#define LWZ_DLL_EXPORT
#include "../include/ledwiz.h"
#include "usbdev.h"

#define USE_SEPARATE_IO_THREAD

#ifdef USE_SEPARATE_IO_THREAD
#define IF_SEPARATE_IO_THREAD(x) x
#else
#define IF_SEPARATE_IO_THREAD(x)
#endif


const GUID HIDguid = { 0x4d1e55b2, 0xf16f, 0x11Cf, { 0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };

USHORT const VendorID_LEDWiz       = 0xFAFA;
USHORT const VendorID_Zebs         = 0x20A0;
USHORT const ProductID_LEDWiz_min  = 0x00F0;
USHORT const ProductID_LEDWiz_max  = ProductID_LEDWiz_min + LWZ_MAX_DEVICES - 1;

// Device control struct
struct lwz_device_t
{
	// handle to USB device
	HUDEV hudev = NULL;

	// detected device type
	UINT device_type = LWZ_DEVICE_TYPE_NONE;

	// input report (device to host) length
	UINT input_rpt_len = 0;

	// Number of outputs on the physical unit.  This is always 32 for real
	// LedWiz units and most clones.  Pinscape (KL25Z) units can have up to 128
	// outputs, and Pinscape Pico units can have up to 255.
	int num_outputs = 0;

	// Does this device support the Pinscape SBX/PBX extensions?
	bool supports_sbx_pbx = false;

	// If this is a Pinscape Virtual LedWiz interface, this contains 
	// information on the underlying physical Pinscape unit and which
	// subset of the physical ports we address.  This isn't used for
	// entires corresponding to physical units (including Pinscape units).
	struct pinscape_info_t
	{
		// index of the base Pinscape unit in the devices[] array
		int base_unit = 0;

		// First port number on underlying device
		int first_port_num = 0;
	};
	pinscape_info_t ps_info;

	// Device name, from the USB HID descriptor
	char device_name[256]{ 0 };

	// USB HID descriptor data; we save this because it contains the file
	// system path for the device, which we might need to re-open the
	// file handle after a device change event
	std::vector<BYTE> diDetail;

	// Pinscape Pico device, if applicable
	std::shared_ptr<PinscapePico::FeedbackControllerInterface> psPico;
};

typedef void * HQUEUE;

// logging levels
#define LOGLEVEL_NONE    0
#define LOGLEVEL_NORMAL  1
#define LOGLEVEL_DEBUG   2

struct lwz_context_t
{
	~lwz_context_t() { }

	// DLL instance handle
	HINSTANCE hInstance = NULL;

	// log file handle
	FILE *fpLog = nullptr;

	// logging level (LOGLEVEL_xxx value)
	int logLevel = LOGLEVEL_NONE;

	// internal device list
	lwz_device_t devices[LWZ_MAX_DEVICES];

	// client device list
	LWZDEVICELIST *plist = nullptr;

	// client window handle, for notifications
	HWND hwnd = NULL;

	// original client window procedure, for restoration on exit
	// (to remove our subclass handler)
	WNDPROC WndProc = NULL;

	// device-change notification handle
	HANDLE hDevNotify = NULL;

	// asynchronous I/O queue, when using threaded I/O
	IF_SEPARATE_IO_THREAD(HQUEUE hqueue = nullptr);

	// client notify callbacks
	struct Callbacks
	{
		void *puser = nullptr;			// user callback context
		LWZNOTIFYPROC notify;			// notify callback
		LWZNOTIFYPROC_EX notify_ex;		// extended notify callback
	} cb;
};

// 'g_cs' protects our state if there is more than on thread in the process using the API.
// Do not synchronize with other threads from within the callback routine because then it can deadlock!
// Calling the API within the callback from the same thread is fine because the critical section does not block for that.
CRITICAL_SECTION g_cs;

// global context
lwz_context_t * g_plwz = NULL;


static LRESULT CALLBACK lwz_wndproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static lwz_context_t * lwz_open(HINSTANCE hinstDLL);
static void lwz_close(lwz_context_t *h);

static void lwz_register(lwz_context_t *h, int indx_user, HWND hwnd);
static bool lwz_get_hdev(lwz_context_t *h, int indx_user, HUDEV &huDev, std::shared_ptr<PinscapePico::FeedbackControllerInterface> &psPico);
static void lwz_notify_callback(lwz_context_t *h, int reason, LWZHANDLE hlwz);

static void lwz_refreshlist_attached(lwz_context_t *h);
static void lwz_refreshlist_detached(lwz_context_t *h);
static void lwz_freelist(lwz_context_t *h);
static void lwz_add(lwz_context_t *h, int indx);
static void lwz_remove(lwz_context_t *h, int indx);

enum packet_type_t
{
	PACKET_TYPE_NONE,       // No data
	PACKET_TYPE_SBA,		// Original LedWiz SBA
	PACKET_TYPE_PBA,		// Original LedWiz PBA
	PACKET_TYPE_RAW,		// raw format (for LwCloneU2 control messages)	
	PACKET_TYPE_SBX,		// Pinscape SBX (extended SBA, for ports beyond 32)
	PACKET_TYPE_PBX, 		// Pinscape PBX (extended PBA, for ports beyond 32)
};

static void queue_close(HQUEUE hqueue, bool unload);
static HQUEUE queue_open(void);
static size_t queue_push(HQUEUE hqueue, 
	HUDEV hudev, std::shared_ptr<PinscapePico::FeedbackControllerInterface> *fci, int starting_port_num,
	packet_type_t typ, uint8_t const *pdata, size_t ndata);
static size_t queue_shift(HQUEUE hqueue, packet_type_t &packet_type,
	HUDEV *phudev, std::shared_ptr<PinscapePico::FeedbackControllerInterface> &fci,
	int &starting_port_num, uint8_t *pbuffer, size_t nsize);
static void queue_wait_empty(HQUEUE hqueue);


struct CAutoLockCS  // helper class to lock a critical section, and unlock it automatically
{
    CRITICAL_SECTION *m_pcs;
	CAutoLockCS(CRITICAL_SECTION *pcs) { m_pcs = pcs; EnterCriticalSection(m_pcs); };
	~CAutoLockCS() { LeaveCriticalSection(m_pcs); };
};

#define AUTOLOCK(cs) CAutoLockCS lock_##__LINE__##__(&cs)   // helper macro for using the helper class


//**********************************************************************************************************************
// Logging
//
static void LOG(int level, char *f, ...)
{
	if (g_plwz != nullptr && g_plwz->fpLog != nullptr && g_plwz->logLevel >= level)
	{
		va_list args;
		va_start(args, f);
		vfprintf(g_plwz->fpLog, f, args);
		va_end(args);

		fflush(g_plwz->fpLog);
	}
}


//**********************************************************************************************************************
// Top Level API functions
//**********************************************************************************************************************

void LWZ_SBA(LWZHANDLE hlwz, 
	unsigned int bank0, unsigned int bank1, unsigned int bank2, unsigned int bank3,
	unsigned int globalPulseSpeed)
{
	LOG(LOGLEVEL_DEBUG, "SBA(unit=%d, {%02x,%02x,%02x,%02x}, speed=%d)\n",
		hlwz, bank0, bank1, bank2, bank3, globalPulseSpeed);

	AUTOLOCK(g_cs);

	// validate the device index
	int indx = hlwz - 1;
	if (indx < 0 || indx >= LWZ_MAX_DEVICES)
		return;

	// Start with the standard SBA message: command code 64.  The
	// "port group" byte is used only in SBX messages; this is unused
	// in regular SBA messages and must be zero.
	BYTE cmd = 64;
	int port_group = 0;
	packet_type_t packet_type = PACKET_TYPE_SBA;
	int startingPortNum = 0;

	// check to see if this is addressed to a Pinscape virtual LedWiz
	lwz_device_t *pDev = &g_plwz->devices[indx];
	if (pDev->device_type == LWZ_DEVICE_TYPE_PINSCAPE_VIRT)
	{
		// get the real Pinscape reference information
		auto *ps = &g_plwz->devices[indx].ps_info;

		// Figure the port group.
		// The port group tells the Pinscape unit which group of 32
		// ports we're addressing.  The base Pinscape interface for
		// the unit addresses the first 32 ports (0-31).  The first
		// *virtual* interface addresses the next 32 ports (32-64).
		// The second virtual interface addresses the next 32, and
		// so on.  The virtual interfaces are always numbered
		// consecutively after the base Pinscape interface, so we
		// can determine the group simply by looking at the indices.
		// If the Pinscape unit is at index N, and we're at index
		// N+1, we're the first virtual interface, so we address the
		// first block of extended ports; if we're at N+2, we address
		// the second block.
		port_group = indx - ps->base_unit;

		// redirect the message to the physical Pinscape device
		indx = ps->base_unit;

		// switch to Pinscape SBX message
		cmd = 67;
		packet_type = PACKET_TYPE_SBX;
	}
	else if (pDev->device_type == LWZ_DEVICE_TYPE_PINSCAPE_PICO)
	{
		// Pinscape Pico.  Pass the starting port number in the
		// queue.
		startingPortNum = pDev->ps_info.first_port_num;
	}

	// make sure we have a valid file handle
	HUDEV huDev = NULL;
	std::shared_ptr<PinscapePico::FeedbackControllerInterface> psPico;
	if (!lwz_get_hdev(g_plwz, indx, huDev, psPico))
		return;

	// set up the SBA or SBX message
	BYTE data[8];
	data[0] = cmd;
	data[1] = bank0;
	data[2] = bank1;
	data[3] = bank2;
	data[4] = bank3;
	data[5] = globalPulseSpeed;
	data[6] = port_group;
	data[7] = 0;

	#if defined(USE_SEPARATE_IO_THREAD)

	queue_push(g_plwz->hqueue, huDev, &psPico, startingPortNum, packet_type, &data[0], 8);

	#else

	usbdev_write(hudev, &data[0], 8);

	#endif

}

void LWZ_PBA(LWZHANDLE hlwz, BYTE const *pbrightness_32bytes)
{
	LOG(LOGLEVEL_DEBUG, "PBA(unit=%d, {", hlwz);
	for (int i = 0 ; i < 32 ; ++i)
		LOG(LOGLEVEL_DEBUG, "%s%d:%d", i == 0 ? "" : ", ", i, pbrightness_32bytes[i]);
	LOG(LOGLEVEL_DEBUG, "})\n");

	AUTOLOCK(g_cs);

	// get and validate the device index
	int indx = hlwz - 1;
	if (indx < 0 || indx >= LWZ_MAX_DEVICES)
		return;

	// make sure we have a non-null brightness buffer
	if (pbrightness_32bytes == NULL)
		return;

	// for regular PBA messages, we'll send the caller's brightness
	// byte array directly
	const BYTE *pdata = pbrightness_32bytes;
	size_t dataSize = 32;

	// presume we'll use a send this as a standard PBA message
	packet_type_t packet_type = PACKET_TYPE_PBA;

	// buffer for rewritten message, for Pinscape and Pinscape Pico variants
	BYTE bBuf[64];

	// Check to see if this is addressed to a Pinscape unit or Pinscape
	// virtual LedWiz interface.  If so, switch the message to the
	// extended PBX format instead.
	BOOL pbx = false;
	lwz_device_t *pDev = &g_plwz->devices[indx];
	int port_group = 0;
	int starting_port_num = 0;
	if (pDev->device_type == LWZ_DEVICE_TYPE_PINSCAPE && pDev->supports_sbx_pbx)
	{
		// It's a physical Pinscape unit, and it supports the extended
		// SBX/PBX messages.  The updates are addressed to ports 1-32, so
		// we could just keep the message with the PBA format.  But switch
		// to PBX anyway, as it's a more reliable message format.  The
		// regular PBA is stateful, as the ports being addressed are
		// implied by the protocol state.  PBX encodes the port address
		// directly in the message, which eliminates the possibility of
		// the host and device getting out of sync.
		pbx = true;
	}
	else if (pDev->device_type == LWZ_DEVICE_TYPE_PINSCAPE_VIRT)
	{
		// It's a Pinscape virtual LedWiz unit.  Get the underlying
		// physical Pinscape unit reference.
		auto *ps = &pDev->ps_info;

		// Figure the port group.
		// The port group tells the Pinscape unit which group of 8
		// ports we're addressing.  The base Pinscape interface for
		// the unit addresses the first 32 ports (0-31).  The first
		// *virtual* interface addresses the next 32 ports (32-64).
		// The second virtual interface addresses the next 32, and
		// so on.  The virtual interfaces are always numbered
		// consecutively after the base Pinscape interface, so we
		// can figure which block of 32 ports this unit addresses
		// from the unit index.  If the Pinscape unit is at index N,
		// and we're at index N+1, we're the first virtual interface,
		// so we address ports 32-64.  If we're at N+2, we address
		// ports 65-96, and so on.
		//
		// For PBX purposes, we want the group of 8 ports we're
		// addressing.  Each unit is 32 ports, so the unit number
		// offset (from the base unit) gives us the group of 32
		// ports.  So the group of 8 is offset*32/8 = offset*4.
		port_group = 4*(indx - ps->base_unit);

		// redirect the message to the Pinscape device as a PBX
		pbx = true;
		indx = ps->base_unit;
	}
	else if (pDev->device_type == LWZ_DEVICE_TYPE_PINSCAPE_PICO)
	{
		// Pinscape Pico virtual LedWiz.  We can transmit the 32
		// profile bytes as-is, but we need to add the starting
		// port number, which we'll pass in the queue.
		starting_port_num = pDev->ps_info.first_port_num;
	}

	// If we're using the Pinscape extended PBX message, rewrite the
	// message data using the PBX format.
	if (pbx)
	{
		// Encode each set of 8 bytes as a PBX message
		const BYTE *psrc = pdata;
		BYTE *pdst = bBuf;
		for (int block = 0 ; block < 4 ;
			 ++block, ++port_group, psrc += 8, pdst += 8)
		{
			// encode this PBX message:
			//
			// 68 pp ee ee ee ee ee ee
			//
			// 68 = command code
			// pp = port group: 0 for ports 1-8, 1 for 9-16, etc
			// ee = packed brightness values, 6 bits per port

			// LedWiz flash codes have to be translated for PBX to fit into 6 bits.
			// 129->60, 130->61, 131->62, 132->63.
			BYTE tmp[8];
			for (int i = 0 ; i < 8 ; ++i)
				tmp[i] = (psrc[i] >= 129 ? psrc[i] - 129 + 60 : psrc[i]) & 0x3F;

			// pack the first four brightness values into tmp1, the next four into tmp2
			unsigned int tmp1 = tmp[0] | (tmp[1]<<6) | (tmp[2]<<12) | (tmp[3]<<18);
			unsigned int tmp2 = tmp[4] | (tmp[5]<<6) | (tmp[6]<<12) | (tmp[7]<<18);

			// now construct the 8-byte PBX message
			pdst[0] = 68;
			pdst[1] = port_group;
			pdst[2] = tmp1 & 0xFF;
			pdst[3] = (tmp1 >> 8) & 0xFF;
			pdst[4] = (tmp1 >> 16) & 0xFF;
			pdst[5] = tmp2 & 0xFF;
			pdst[6] = (tmp2 >> 8) & 0xFF;
			pdst[7] = (tmp2 >> 16) & 0xFF;
		}

		// use the encoded private copy instead of the original
		pdata = bBuf;
		packet_type = PACKET_TYPE_PBX;
	}

	// get the USB handle
	HUDEV huDev = NULL;
	std::shared_ptr<PinscapePico::FeedbackControllerInterface> psPico;
	if (!lwz_get_hdev(g_plwz, indx, huDev, psPico))
		return;

	#if defined(USE_SEPARATE_IO_THREAD)

	queue_push(g_plwz->hqueue, huDev, &pDev->psPico, starting_port_num, packet_type, pdata, dataSize);

	#else

	usbdev_write(hudev, pdata, 32);

	#endif
}

DWORD LWZ_RAWWRITE(LWZHANDLE hlwz, BYTE const *pdata, DWORD ndata)
{
	AUTOLOCK(g_cs);

	int indx = hlwz - 1;

	if (pdata == NULL || ndata == 0)
		return 0;

	if (ndata > 63)
	    ndata = 63;

	HUDEV huDev = NULL;
	std::shared_ptr<PinscapePico::FeedbackControllerInterface> psPico;
	if (!lwz_get_hdev(g_plwz, indx, huDev, psPico))
		return 0;

	int res = 0;
	DWORD nBytesWritten = 0;

	#if defined(USE_SEPARATE_IO_THREAD)

	nBytesWritten = static_cast<DWORD>(queue_push(g_plwz->hqueue, huDev, nullptr, 0, PACKET_TYPE_RAW, pdata, ndata));

	#else

	if (hdDev != NULL)
		nBytesWritten = usbdev_write(huDev, pdata, ndata);
	else if (psPico != nullptr)
		nBytesWritten = psPico->Write(pdata, ndata, 1000) ? ndata : 0;

	#endif

	return nBytesWritten;
}

DWORD LWZ_RAWREAD(LWZHANDLE hlwz, BYTE *pdata, DWORD ndata)
{
	AUTOLOCK(g_cs);

	int indx = hlwz - 1;

	if (pdata == NULL)
		return 0;

	if (ndata > 64)
	    ndata = 64;

	IF_SEPARATE_IO_THREAD(queue_wait_empty(g_plwz->hqueue));

	HUDEV huDev = NULL;
	std::shared_ptr<PinscapePico::FeedbackControllerInterface> psPico;
	if (!lwz_get_hdev(g_plwz, indx, huDev, psPico))
		return 0;

	if (huDev != NULL)
		return static_cast<DWORD>(usbdev_read(huDev, pdata, ndata));
	else if (psPico != nullptr)
	{
		PinscapePico::FeedbackControllerReport rpt;
		if (psPico->Read(rpt, 2500))
		{
			if (ndata > 0) pdata[0] = rpt.type;
			if (ndata > 1) memcpy(&pdata[1], rpt.args, min(ndata - 1, sizeof(rpt.args)));
			return ndata;
		}
	}

	// failed
	return 0;
}

void LWZ_REGISTER(LWZHANDLE hlwz, HWND hwnd)
{
	LOG(LOGLEVEL_NORMAL, hwnd == 0 ? "LWZ_REGISTER(%d, null)\n" : "LWZ_REGISTER(%d, %lx)\n",
		hlwz, hwnd);

	AUTOLOCK(g_cs);

	int indx = hlwz - 1;

	lwz_register(g_plwz, indx, hwnd);
}

void LWZ_SET_NOTIFY_EX(LWZNOTIFYPROC_EX notify_ex_cb, void * puser, LWZDEVICELIST *plist)
{
	AUTOLOCK(g_cs);

	lwz_context_t * const h = g_plwz;

	h->plist = plist;
	h->cb.notify_ex = notify_ex_cb;
	h->cb.puser = puser;

	if (h->plist != nullptr)
		memset(h->plist, 0x00, sizeof(*plist));

	lwz_refreshlist_attached(h);
}

void LWZ_SET_NOTIFY(LWZNOTIFYPROC notify_cb, LWZDEVICELIST *plist)
{
	LOG(LOGLEVEL_NORMAL, "LWZ_SET_NOTIFY(cb=%08lx, listp=%08lx)\n", notify_cb, plist);

	AUTOLOCK(g_cs);

	lwz_context_t * const h = g_plwz;

	// Remove any previous list.  This will force a call to the
	// callback for each device found on the new scan we'll do
	// before returning.  If we didn't do this, the callback
	// wouldn't be invoked for any device we already scanned.
	lwz_freelist(h);

	// set new list pointer and callbacks
	h->plist = plist;
	h->cb.notify = notify_cb;

	if (h->plist != nullptr)
		memset(h->plist, 0x00, sizeof(*plist));

	// create a new internal list of available devices
	lwz_refreshlist_attached(h);
}

static void safe_strcpy(char *dst, size_t dst_size, const char *src)
{
	// only proceed if we have a destination buffer of non-zero size
	if (dst_size != 0 && dst != 0)
	{
		// substitute an empty string if the source is null
		if (src == 0)
			src = "";

		// figure the copy length as the source length, or the destination
		// buffer size minus one for the trailing null, whichever is shorter
		size_t copy_len = strlen(src);
		if (copy_len > dst_size - 1)
			copy_len = dst_size - 1;

		// copy the copy length, then add the null terminator
		memcpy(dst, src, copy_len);
		dst[copy_len] = '\0';
	}
}

static void safe_strcat(char *dst, size_t dst_size, const char *src)
{
	// make sure we have a destination buffer of non-zero size
	if (dst != 0 && dst_size != 0)
	{
		// get the current length, and make sure there's room for more
		size_t old_len = strlen(dst);
		if (old_len < dst_size)
		{
			// figure the maximum copy length
			size_t copy_len = strlen(src);
			if (copy_len > dst_size - old_len - 1)
				copy_len = dst_size - old_len - 1;

			// copy the copy length, and add a null terminator
			memcpy(dst + old_len, src, copy_len);
			dst[old_len + copy_len] = '\0';
		}
	}
}

BOOL LWZ_GET_DEVICE_INFO(LWZHANDLE hlwz, LWZDEVICEINFO *info)
{
	// enter our critical section
	AUTOLOCK(g_cs);

	// get our global context
	lwz_context_t * const h = g_plwz;

	// set defaults in the return struct
	info->dwDevType = LWZ_DEVICE_TYPE_NONE;
	info->szName[0] = '\0';

	// validate the index
	int indx = (int)hlwz - 1;
	if (indx < 0 || indx >= LWZ_MAX_DEVICES)
		return FALSE;

	// get the device and make sure it's an existing device
	lwz_device_t *dev = &h->devices[indx];
	if (dev->device_type == LWZ_DEVICE_TYPE_NONE)
		return FALSE;

	// fill in the fields
	info->dwDevType = dev->device_type;
	safe_strcpy(info->szName, sizeof(info->szName), dev->device_name);

	// success
	return TRUE;
}


//**********************************************************************************************************************
// Internal implementation 
//**********************************************************************************************************************

BOOL WINAPI DllMain(
	HINSTANCE hinstDLL,
	DWORD fdwReason,
	LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		InitializeCriticalSection(&g_cs);

		g_plwz = lwz_open(hinstDLL);

		if (g_plwz == NULL)
		{
			DeleteCriticalSection(&g_cs);
			return FALSE;
		}

		LOG(LOGLEVEL_NORMAL, "*****\n"
			"LEDWIZ.DLL loading\n\n");
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
		{
			AUTOLOCK(g_cs);
			lwz_close(g_plwz);
		}

		DeleteCriticalSection(&g_cs);
	}

	return TRUE;
}

static LRESULT CALLBACK lwz_wndproc(
	HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
	AUTOLOCK(g_cs);

	lwz_context_t * const h = g_plwz;

	// get the original WndProc
	WNDPROC OriginalWndProc = h->WndProc;

	// check the message type
	switch (uMsg)
	{
	case WM_DEVICECHANGE:
		// device change - check which type (attach or detach)
		switch (wParam)
		{
		case DBT_DEVICEARRIVAL:
			lwz_refreshlist_attached(h);
			break;
			
		case DBT_DEVICEREMOVECOMPLETE:
			lwz_refreshlist_detached(h);
			break;
		}
		break;

	case WM_DESTROY:
		// destroying the window - cancel the registration
		lwz_register(h, 0, NULL);
		break;
	}
	
	// forward the message to original windows procedure
	if (OriginalWndProc != NULL)
	{
		return CallWindowProc(
			OriginalWndProc,
			hwnd,
			uMsg,
			wParam,
			lParam);
	}

	return 0;
}

static lwz_context_t * lwz_open(HINSTANCE hinstDLL)
{
	// allocate the context
	lwz_context_t * const h = new lwz_context_t();
	if (h == NULL)
		return NULL;

	// clear the context structure to all zeroes
	memset(h, 0x00, sizeof(*h));

	// get the directory path containing the DLL
	char path[MAX_PATH];
	GetModuleFileNameA(hinstDLL, path, _countof(path));
	PathRemoveFileSpecA(path);

	// load the config file
	char configFile[MAX_PATH];
	PathCombineA(configFile, path, "ledwiz.dll.config");
	if (FILE *fp; fopen_s(&fp, configFile, "r") == 0 && fp != nullptr)
	{
		// read the file
		for (;;)
		{
			// read a line
			char buf[256];
			if (fgets(buf, _countof(buf), fp) == nullptr)
				break;

			// parse it
			std::match_results<const char*> m;
			static const std::regex pat("\\s*(\\w+)\\s*=\\s*(.*?)\\s*\\n?");
			if (std::regex_match(buf, m, pat))
			{
				// matched - check the keyword
				if (m[1].str() == "logging")
				{
					// logging = <log level>
					h->logLevel = atoi(m[2].str().c_str());;
				}
			}
		}

		// done with the file
		fclose(fp);
	}

	// if logging is enabled, open a log file, named based on the current time
	// to keep it separate from files created by other concurrent processes
	// using the DLL
	if (h->logLevel > 0)
	{
		char logName[MAX_PATH];
		SYSTEMTIME st;
		GetLocalTime(&st);
		_snprintf_s(logName, _TRUNCATE, "%s\\LedWiz-%04d%02d%02d-%02d%02d%02d.log",
			path, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		if (fopen_s(&h->fpLog, logName, "w") != 0)
			h->fpLog = nullptr;
	}

	// initialize all unit types to None
	for (int i = 0 ; i < LWZ_MAX_DEVICES ; ++i)
		h->devices[i].device_type = LWZ_DEVICE_TYPE_NONE;

	// set up the I/O queue and worker thread
	#if defined(USE_SEPARATE_IO_THREAD)
	if ((h->hqueue = queue_open()) == NULL)
	{
		free(h);
		return NULL;
	}
	#endif

	return h;
}

static void lwz_close(lwz_context_t *h)
{
	if (h == NULL)
		return;

	// close all open device handles and
	// unhook our window proc (and unregister the device change notifications)
	lwz_freelist(h);
	lwz_register(h, 0, NULL);

	#if defined(USE_SEPARATE_IO_THREAD)
	if (h->hqueue != NULL)
	{
		queue_close(h->hqueue, true);
		h->hqueue = NULL;
	}
	#endif

	// close the log file
	if (h->fpLog != nullptr)
		fclose(h->fpLog);

	// free resources
	delete h;
}
	
static void lwz_register(lwz_context_t *h, int indx, HWND hwnd)
{
	// if there's a non-null window handle, register, otherwise unregister
	if (hwnd != NULL)
	{
		// Window handle provided - register.
		
		// If there's a previous window handle registered, fail.  Only
		// one window may be registered at a time.  The caller can register
		// a new window, but only if they explicitly un-register the prior
		// one first.
		if (h->hwnd != NULL && h->hwnd != hwnd)
			return;

		// verify that this index is valid
		if (indx < 0 ||	indx >= LWZ_MAX_DEVICES)
			return;

		// verify that there's a device at this index
		auto &d = h->devices[indx];
		if (d.hudev == NULL && d.psPico == nullptr)
			return;

		// "subclass" the window to intercept messages
		WNDPROC PrevWndProc = (WNDPROC)SetWindowLongPtrA(
			hwnd,
			GWLP_WNDPROC,
			(LONG_PTR)lwz_wndproc);

		// make sure that succeeded, and that we didn't already do this
		if (PrevWndProc == NULL || PrevWndProc == lwz_wndproc)
			return;

		// remember the original window proc so that we can forward messages to it
		h->WndProc = PrevWndProc;
		h->hwnd = hwnd;

		// register for device notifications
		if (h->hDevNotify == NULL)
		{
			DEV_BROADCAST_DEVICEINTERFACE_A dbch = {};
			dbch.dbcc_size = sizeof(dbch); 
			dbch.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE; 
			dbch.dbcc_classguid = HIDguid; 
			dbch.dbcc_name[0] = '\0'; 

			h->hDevNotify = RegisterDeviceNotificationA(hwnd, &dbch, DEVICE_NOTIFY_WINDOW_HANDLE);
		}
	}
	else
	{
		// Null window handle - unregister
		
		// unregister the device notification
		if (h->hDevNotify)
		{
			UnregisterDeviceNotification(h->hDevNotify);
			h->hDevNotify = NULL;
		}

		// un-subclass the window
		if (h->hwnd != NULL && h->WndProc != NULL)
		{
			SetWindowLongPtrA(
				h->hwnd,
				GWLP_WNDPROC,
				(LONG_PTR)h->WndProc);

			h->hwnd = NULL;
			h->WndProc = NULL;
		}
	}
}

static bool lwz_get_hdev(lwz_context_t *h, int indx, 
	HUDEV &huDev, std::shared_ptr<PinscapePico::FeedbackControllerInterface> &psPico)
{
	// validate the index
	if (indx < 0 || indx >= LWZ_MAX_DEVICES)
		return false;

	// populate the handles
	auto &pDev = h->devices[indx];
	huDev = pDev.hudev;
	psPico = pDev.psPico;
	return true;
}

static std::shared_ptr<PinscapePico::FeedbackControllerInterface> lwz_get_pspico(lwz_context_t *h, int indx)
{

}

static void lwz_notify_callback(lwz_context_t *h, int reason, LWZHANDLE hlwz)
{
	if (h->cb.notify != 0)
	{
		LOG(LOGLEVEL_NORMAL, "NOTIFY(reason=%d (%s), unit=%d)\n",
			reason,
			reason == LWZ_REASON_ADD ? "Add" : reason == LWZ_REASON_DELETE ? "Delete" : "Unknown",
			hlwz);
		h->cb.notify(reason, hlwz);
	}

	if (h->cb.notify_ex != 0)
	{
		LOG(LOGLEVEL_NORMAL, "NOTIFY_EX(reason=%d (%s), unit=%d)\n",
			reason,
			reason == LWZ_REASON_ADD ? "Add" : reason == LWZ_REASON_DELETE ? "Delete" : "Unknown",
			hlwz);
		h->cb.notify_ex(h->cb.puser, reason, hlwz);
	}
}

// Add one or more new devices to the client's device list, and invoke
// the client callback.
//
// For compatibility with some existing clients, it's necessary to add
// ALL devices to the client's list before the FIRST notify callback.
// The original LEDWIZ.DLL does this, and some clients depend on it.
// E.g., LedBlinky apparently only pays attention to the first notify
// callback, and ignores all subsequent invocations, so it only detects
// devices that are in the list on the first call.  Ergo the list must
// be populated with all devices before the first call.  This isn't
// specified one way or the other in the API, but it would seem more
// reasonable to me to populate the list incrementally, adding each
// device just before calling the callback for that device.  This is
// in fact what the original LWCloneU2 did, but that broke LedBlinky.
// For full compatibility, we have to do things the same peculiar way
// as the original DLL.
static void lwz_add(lwz_context_t *h, int ndevices, const int *device_indices)
{

	// First, update the user list if one was provided.  We have to
	// add all devices to the list before invoking the callback for
	// any device.
	if (h->plist)
	{
		for (int i = 0 ; i < ndevices ; ++i)
		{
			// get the current unit number (== device index + 1)
			LWZHANDLE hlwz = device_indices[i] + 1;

			// check to see if it's already in the list
			bool found = false;
			for (int j = 0 ; j < h->plist->numdevices ; ++j)
			{
				if (h->plist->handles[j] == hlwz)
				{
					found = true;
					break;
				}
			}

			// if there's room, and it's not already in the list, add it
			if (!found && h->plist->numdevices < LWZ_MAX_DEVICES)
			{
				h->plist->handles[h->plist->numdevices] = hlwz;
				h->plist->numdevices += 1;
				LOG(LOGLEVEL_NORMAL, "lwz_add(unit=%d, #devices=%d)\n", hlwz, h->plist->numdevices);
			}
		}
	}

	// Now invoke the user callback once for each added device
	for (int i = 0 ; i < ndevices ; ++i)
	{
		LWZHANDLE hlwz = device_indices[i] + 1;
		lwz_notify_callback(h, LWZ_REASON_ADD, hlwz);
	}
}

static void lwz_remove(lwz_context_t *h, int indx)
{
	LWZHANDLE hlwz = indx + 1;

	// update user list (if one waw provided)

	if (h->plist)
	{
		for (int i = 0; i < h->plist->numdevices; i++)
		{
			LWZHANDLE hlwz = indx + 1;

			if (h->plist->handles[i] != hlwz)
				continue;

			h->plist->handles[i] = h->plist->handles[h->plist->numdevices - 1];
			h->plist->handles[h->plist->numdevices - 1] = 0;

			h->plist->numdevices -= 1;
		}
	}

	// notify callback

	lwz_notify_callback(h, LWZ_REASON_DELETE, hlwz);
}

static void lwz_refreshlist_detached(lwz_context_t *h)
{
	// check for removed devices
	// i.e. try to re-open all registered devices in our internal list

	for (int i = 0; i < LWZ_MAX_DEVICES; i++)
	{
		auto *dev = &h->devices[i];
		if (dev->hudev != NULL)
		{
			// try opening the device handle again
			SP_DEVICE_INTERFACE_DETAIL_DATA_A * pdiddat = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(dev->diDetail.data());
			HANDLE hdev = CreateFileA(pdiddat->DevicePath, GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

			// if we couldn't open the handle, the device must have been unplugged
			if (hdev == INVALID_HANDLE_VALUE)
			{
				// If this is a Pinscape device, remove any virtual LedWiz units
				// that refer back to it.
				if (dev->device_type == LWZ_DEVICE_TYPE_PINSCAPE)
				{
					// Pinscape units set up one virtual LedWiz interface per
					// block of 32 output ports after the first 32.  The new
					// devices are at consecutive unit numbers after the actual
					// Pinscape unit.  The virtual devices are only created when
					// there's no actual device at the same unit number, so we
					// have to check each slot to see if it is indeed a virtual
					// interface.
					lwz_device_t *vdev = &h->devices[i+1];
					for (int vidx = i + 1, portno = 32 ;
						 vidx < LWZ_MAX_DEVICES && portno < dev->num_outputs ;
						 ++vidx, portno += 32, ++vdev)
					{
						// check to see if it's a virtual LedWiz interface that's
						// tied to the Pinscape interface we're deleting
						if (vdev->device_type == LWZ_DEVICE_TYPE_PINSCAPE_VIRT
							&& vdev->ps_info.base_unit == i)
						{
							// it's one of ours - remove this interface too
							vdev->device_type = LWZ_DEVICE_TYPE_NONE;
							lwz_remove(h, vidx);
						}
					}
				}

				// close our existing USB file handle
				usbdev_release(dev->hudev);
				dev->hudev = NULL;
				dev->device_type = LWZ_DEVICE_TYPE_NONE;

				// remove the device from the user list and notify the user callback
				lwz_remove(h, i);
			}
			else
			{
				// Success - we have a valid new handle to the file.  We only
				// needed the new handle to see if we could create it, though,
				// so we have no more use for it; close it.
				CloseHandle(hdev);
			}
		}
		else if (dev->psPico != nullptr)
		{
			// reconnect the Pico
			if (!dev->psPico->TestFileSystemPath())
			{
				// device no longer active - release the interface object
				dev->psPico.reset();
				dev->device_type = LWZ_DEVICE_TYPE_NONE;

				// remove the device from the user list and notify the user callback
				lwz_remove(h, i);
			}
		}
	}
}

static void lwz_refreshlist_attached(lwz_context_t *h)
{
	LOG(LOGLEVEL_NORMAL, "Refreshing attached device list\n");

	// no new devices found yet
	int num_new_devices = 0;
	int new_devices[LWZ_MAX_DEVICES];

	// set up a search on all HID devices
	HDEVINFO hDevInfo = SetupDiGetClassDevsA(&HIDguid, NULL, NULL,
		DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);

	// we can't proceed unless we got the HID list
	if (hDevInfo == INVALID_HANDLE_VALUE)
		return;

	// go through all available devices and look for the proper VID/PID
 	for (DWORD dwindex = 0 ; ; dwindex++)
	{
		// get the next interface in the HID list; stop on failure, which
		// indicates the end of the list
		SP_DEVICE_INTERFACE_DATA didat = {};
		didat.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		if (!SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &HIDguid, dwindex, &didat))
			break;

		// get the device detail size
		DWORD requiredSize = 0;
		SetupDiGetDeviceInterfaceDetailA(hDevInfo, &didat, NULL, 0, &requiredSize, NULL);
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			continue;

		// allocate a buffer for the detail data
		lwz_device_t device_tmp;
		device_tmp.diDetail.resize(requiredSize);

		// retrieve the device detail
		SP_DEVICE_INTERFACE_DETAIL_DATA_A *pDiDetail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(device_tmp.diDetail.data());
		pDiDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
		if (!SetupDiGetDeviceInterfaceDetailA(hDevInfo, &didat, pDiDetail, requiredSize, NULL, NULL))
			continue;

		// open the file handle to the USB device
		device_tmp.hudev = usbdev_create(pDiDetail->DevicePath);
		if (device_tmp.hudev != NULL)
		{
			// retrieve the HID attributes
			HIDD_ATTRIBUTES attrib = {};
			attrib.Size = sizeof(HIDD_ATTRIBUTES);
			BOOLEAN bSuccess = HidD_GetAttributes(
				usbdev_handle(device_tmp.hudev),
				&attrib);

			LOG(LOGLEVEL_NORMAL, ". Found USB HID device, VID %04X, PID %04X\n", attrib.VendorID, attrib.ProductID);

			// Check to see if this looks like an LedWiz VID/PID combo.  LedWiz devices
			// identify as Vendor ID FAFA, Product ID 00F0..00FF.  The low 4 bits of the
			// product ID is by convention the LedWiz "unit number".  The API uses this
			// to distinguish multiple units in one system and direct commands to the
			// desired unit.  The nominal unit number is in the range 1..16, so it's
			// equivalent to (ProductID & 0x000F) + 1.
			int indx = (int)attrib.ProductID - (int)ProductID_LEDWiz_min;
			if (bSuccess &&
				(attrib.VendorID == VendorID_LEDWiz || attrib.VendorID == VendorID_Zebs) &&
				indx >= 0 && indx < LWZ_MAX_DEVICES)
			{
				// It's an LedWiz, according to the VID/PID
				LOG(LOGLEVEL_NORMAL, ".. vendor/product code matches LedWiz, checking HID descriptors\n", indx+1);

				// Before we conclude for sure that it's an LedWiz, though, do some more
				// checks.  Retrieve the preparsed data for the device.
				PHIDP_PREPARSED_DATA p_prepdata = NULL;
				if (HidD_GetPreparsedData(usbdev_handle(device_tmp.hudev), &p_prepdata) == TRUE)
				{
					LOG(LOGLEVEL_NORMAL, ".. retrieved preparsed data OK\n");

					// get the HID capabilities struct
					HIDP_CAPS caps = {};
					if (HIDP_STATUS_SUCCESS == HidP_GetCaps(p_prepdata, &caps))
					{
						LOG(LOGLEVEL_NORMAL, ".. retrieved HID capabilities: "
							" link collection nodes %d, output report length %d\n",
							caps.NumberLinkCollectionNodes, caps.OutputReportByteLength);

						// Apply heuristic filters:
						//
						// 1. Output report byte length
						// The LedWiz command interface has an eight byte output report.
						// Note that the Windows HID drivers always include a one-byte
						// "report ID" prefix in reports read or written through the
						// driver.  The LedWiz itself doesn't transmit the prefix byte
						// because (per USB HID conventions) it's never included by
						// devices that have only one report type, as is the case for
						// an LedWiz.  However, the Windows HID drivers normalize this
						// by including the prefix byte to user programs whether it's
						// in the physical reports or not.  For consistency, Windows
						// HID also normalizes the report length seen in the HID caps,
						// so the byte length we're looking for is 9.
						//
						// 2. USB Usage
						// Test that this is NOT a keyboard interface (USB usage page 
						// 1, usage 6).  The Pinscape controller presents a keyboard
						// interface in addition its joystick interface, which looks
						// to the HID scan like a completely separate device.  We want
						// to skip that virtual device since it doesn't accept LedWiz 
						// output reports.  (Note that it would filter out more false
						// positives if we "ruled in" specific HID usages rather than
						// only "ruling out" the keyboard, but that would also be less
						// flexible at recognizing future product updates from GGG and
						// future clones and emulators.  In practice, false positives
						// from random third-party devices don't actually seem to
						// happen, so on balance it seems much better to err on the
						// side of filtering in unknown devices that pass our other
						// tests.)
						//
						// 3. Link collection count (REMOVED)
						// In the past, we also checked the link collection count to
						// make sure caps.NumberLinkCollectionNodes == 1.  This was a
						// further ad hoc check that the original LedWiz device and
						// LWCloneU2 devices both passed, and which the Pinscape device
						// deliberately passed because it was known that LWCloneU2 did
						// this test.  However, this test is now too restrictive in 
						// that a newer real LedWiz product, the LedWiz+GP, fails the
						// test.  So I'm removing the link collection node test.  That
						// test was purely speculative anyway: as far as I know, there
						// are no actual false positives that it filtered out, so it
						// was just there *in case* something came along that spoofed
						// an LedWiz as far as all of the other tests go.
						if (caps.OutputReportByteLength == 9
							&& !(caps.UsagePage == 1 && caps.Usage == 6)) // USB keyboard = page 1/usage 6
						{
							LOG(LOGLEVEL_NORMAL, ".. link collection node count, report length, and USB usage match LedWiz\n");

							// presume it's a real LedWiz or some clone/emulation we don't
							// handle specially
							device_tmp.device_type = LWZ_DEVICE_TYPE_LEDWIZ;

							// Remember the input report (device to host) length.  Note that
							// the length in the caps is normalized to include the synthesized
							// report ID, so subtract one to get the actual device report size.
							device_tmp.input_rpt_len = caps.InputReportByteLength - 1;

							// presume it has the standard LedWiz complement of 32 ports
							device_tmp.num_outputs = 32;
							device_tmp.supports_sbx_pbx = false;

							// If it's using the zebsboard VID, make sure the manufacturer ID looks right
							if (attrib.VendorID == VendorID_Zebs)
							{
								// get the manufacturer ID string, in lower-case, for further testing
								wchar_t manustr[256] = { 0 };
								HidD_GetManufacturerString(usbdev_handle(device_tmp.hudev), manustr, 256);
								_wcslwr_s(manustr);

								if (wcsstr(manustr, L"zebsboards") != NULL)
								{
									// mark it as a zeb's output control device
									LOG(LOGLEVEL_NORMAL, ".. ZB Output Control detected\n");
									device_tmp.device_type = LWZ_DEVICE_TYPE_ZB;

									// this device doesn't need USB delays
									usbdev_set_min_write_interval(device_tmp.hudev, 0);
								}
								else
								{
									// it's not a Zebsboards unit, so it must not be an LedWiz
									// emulator after all
									LOG(LOGLEVEL_NORMAL, ".. Device uses VID 0x20A0, but manufacturer string doesn't contain 'zebsboards' - rejecting\n");
									device_tmp.device_type = LWZ_DEVICE_TYPE_NONE;
								}
							}

							// get the product ID string, so that we can further identify
							// whether the device is a real LedWiz or one of the specific
							// types of clones we know about
							wchar_t prodstr[256];
							device_tmp.device_name[0] = '\0';
							if (HidD_GetProductString(usbdev_handle(device_tmp.hudev), prodstr, 256))
							{
								// save the product string
								size_t retlen;
								wcstombs_s(&retlen, device_tmp.device_name, sizeof(device_tmp.device_name),
									prodstr, _TRUNCATE);

								// check for the special device types
								if (wcsstr(prodstr, L"Pinscape Controller") != 0)
								{
									// It's a Pinscape unit
									LOG(LOGLEVEL_NORMAL, ".. Pinscape Controller identified\n");
									device_tmp.device_type = LWZ_DEVICE_TYPE_PINSCAPE;

									// Pinscape doesn't need USB delays
									usbdev_set_min_write_interval(device_tmp.hudev, 0);

									// Query the number of outputs by sending a QUERY CONFIGURATION
									// special request (65 4).  Clear the input buffer before making
									// the request, since the input buffer could be full of regular
									// joystick reports.  We could time out before getting to the
									// config report reply if we don't clear out old joystick
									// reports first.
									char qbuf[8] = { 65, 4, 0, 0, 0, 0, 0, 0 };
									usbdev_clear_input(device_tmp.hudev, caps.InputReportByteLength);
									usbdev_write(device_tmp.hudev, qbuf, 8);

									// wait for the proper reply; retry a few times if necessary
									BYTE rbuf[65];
									for (int i = 0 ; i < 64 ; ++i)
									{
										// Read a report, and check for a CONFIGURATION REPORT
										// reply (00 88 ...).  We're interested in the number of
										// outputs at bytes 2:3, and the bit flags at byte 11.
										if (usbdev_read(device_tmp.hudev, rbuf, device_tmp.input_rpt_len) > 0
											&& (rbuf[0] == 0x00 && rbuf[1] == 0x88))
										{
											// It's the configuration report.
											//
											// If byte 11 has bit 0x02 set, the installed firmware
											// supports the SBX/PBX protocol extensions that we need
											// to access ports beyond the first 32.
											if ((rbuf[11] & 0x02) != 0)
											{
												// SBX/PBX are supported, so we can access all
												// output ports.  Note that actual number of ports.
												device_tmp.supports_sbx_pbx = true;
												device_tmp.num_outputs = rbuf[2] | (rbuf[3] << 8);
											}

											// add the pinscape unit number to the name
											char unitno[20];
											_snprintf_s(unitno, sizeof(unitno), _TRUNCATE, " (Unit %d)", int(rbuf[4] + 1));
											safe_strcat(device_tmp.device_name, sizeof(device_tmp.device_name), unitno);

											// we can stop looking for a report now
											break;
										}
									}
								}
								else if (wcslen(prodstr) >= 9
									&& memcmp(prodstr, L"LWCloneU2", 9*sizeof(wchar_t)) == 0)
								{
									// It's an LWCloneU2 unit
									LOG(LOGLEVEL_NORMAL, ".. LWCloneU2 identified\n");
									device_tmp.device_type = LWZ_DEVICE_TYPE_LWCLONEU2;

									// LWCloneU2 doesn't need USB delays
									usbdev_set_min_write_interval(device_tmp.hudev, 0);
								}
							}

							// if we decided to keep this device, add it
							if (device_tmp.device_type != LWZ_DEVICE_TYPE_NONE)
							{
								LOG(LOGLEVEL_NORMAL, ".. attempting to add device\n");

								// If this slot contains a Pinscape virtual LedWiz interface,
								// remove the virtual device so that we can use the slot for
								// the real device.  Real devices always override virtual ones.
								if (h->devices[indx].device_type == LWZ_DEVICE_TYPE_PINSCAPE_VIRT)
								{
									// remove the virtual interface and notify the user callback
									LOG(LOGLEVEL_NORMAL, ".. this slot has a Pinscape virtual LedWiz; this real device overrides that\n");
									h->devices[indx].device_type = LWZ_DEVICE_TYPE_NONE;
									lwz_remove(h, indx);
								}

								// if this slot isn't populated yet, add the device
								if (h->devices[indx].hudev == NULL)
								{
									// copy the temp device struct to the active device list entry
									h->devices[indx] = std::move(device_tmp);

									// the device list entry now owns the file handle, so forget it
									// in the temp struct
									device_tmp.hudev = NULL;

									LOG(LOGLEVEL_NORMAL, ".. device added successfully, %d devices total\n", num_new_devices);

									// add it to our list of new devices found on this search
									if (num_new_devices < LWZ_MAX_DEVICES)
										new_devices[num_new_devices++] = indx;

								}
								else
								{
									LOG(LOGLEVEL_NORMAL, ".. unit slot already in use; device not added\n");
								}
							}
						}
					}

					HidD_FreePreparsedData(p_prepdata);
				}
			}

			// If the temp struct has a valid file handle, close it.  Note that if
			// we decided the entry was a valid device, the temp struct won't have
			// a file handle because the active device list entry took ownership
			// of it, and nulled the reference here.
			if (device_tmp.hudev != NULL)
			{
				usbdev_release(device_tmp.hudev);
				device_tmp.hudev = NULL;
			}
		}
	}

	// done with the HID device list
	if (hDevInfo != NULL)
		SetupDiDestroyDeviceInfoList(hDevInfo);

	// Set up any needed Pinscape virtual LedWiz interfaces.  For each
	// Pinscape unit with more than 32 outputs, we'll set up one virtual
	// LedWiz object for each block of 32 outputs beyond the first 32.
	for (int i = 0 ; i < num_new_devices ; ++i)
	{
		// get the added device
		int newidx = new_devices[i];
		lwz_device_t *newdev = &h->devices[newidx];

		// check if it's an LedWiz with more than 32 ports
		if (newdev->device_type == LWZ_DEVICE_TYPE_PINSCAPE && newdev->num_outputs > 32)
		{
			// add a virtual device for each additional block of ports
			for (int vidx = newidx + 1, portno = 32 ;
				vidx < LWZ_MAX_DEVICES && portno < newdev->num_outputs ;
				++vidx, portno += 32)
			{
				// if this slot isn't already populated with a real device,
				// add the virtual device
				lwz_device_t *vdev = &h->devices[vidx];
				if (vdev->device_type == LWZ_DEVICE_TYPE_NONE)
				{
					// set it up as a virtual LedWiz for this block of
					// ports, referring back to the real Pinscape device
					vdev->device_type = LWZ_DEVICE_TYPE_PINSCAPE_VIRT;
					vdev->ps_info.base_unit = newidx;
					vdev->ps_info.first_port_num = portno;

					// synthesize a name based on the base unit name
					_snprintf_s(vdev->device_name, _TRUNCATE, "%s Ports %d-%d", 
						h->devices[newidx].device_name, portno+1, portno+32);

					// count this as a new device in the notification list
					if (num_new_devices < LWZ_MAX_DEVICES)
						new_devices[num_new_devices++] = vidx;
				}
			}
		}
	}

	// search for Pinscape Pico devices
	std::list<PinscapePico::FeedbackControllerInterface::Desc> picos;
	if (SUCCEEDED(PinscapePico::FeedbackControllerInterface::Enumerate(picos)))
	{
		// add each LedWiz-enabled Pico
		for (auto &pico : picos)
		{
			// if the desired LedWiz unit number is non-zero, LedWiz emulation is 
			// enabled and the device has at least one logical output port
			if (pico.ledWizUnitNum != 0 && pico.numOutputPorts != 0)
			{
				// it's enabled
				LOG(LOGLEVEL_NORMAL, ". Found Pinscape Pico unit #%d (%s, HWID %s), virtual LedWiz unit #%d, %d ports\n",
					pico.unitNum, pico.unitName.c_str(), pico.hwId.ToString().c_str(), pico.ledWizUnitNum, pico.numOutputPorts);

				// open the interface
				std::shared_ptr<PinscapePico::FeedbackControllerInterface> fci(PinscapePico::FeedbackControllerInterface::Open(pico));
				if (fci == nullptr)
				{
					LOG(LOGLEVEL_NORMAL, ". Unable to open feedback controller interface\n");
					continue;
				}

				// synthesize a name for the virtual devices
				char baseName[128];
				_snprintf_s(baseName, _TRUNCATE, "Pinscape Pico #%d (%s)", pico.unitNum, pico.unitName.c_str());

				// populate one virtual LedWiz unit per 32 output ports
				for (int devIndex = pico.ledWizUnitNum - 1, basePortNum = 1 ; 
					basePortNum <= pico.numOutputPorts && devIndex < 16 ;
					++devIndex, basePortNum += 32)
				{
					// only populate the virtual device if it's not already occupied
					auto &d = h->devices[devIndex];
					if (d.device_type == LWZ_DEVICE_TYPE_NONE)
					{
						// figure the ending output number
						int lastPortNum = min(basePortNum + 31, pico.numOutputPorts);

						// add the device
						d.device_type = LWZ_DEVICE_TYPE_PINSCAPE_PICO;
						d.ps_info.base_unit = pico.ledWizUnitNum - 1;
						d.ps_info.first_port_num = basePortNum;

						// store the interface pointer
						d.psPico = fci;

						// synthesize a name
						_snprintf_s(d.device_name, _TRUNCATE, "%s Ports %d-%d", 
							baseName, basePortNum, lastPortNum);

						// add it to the new items notification list
						if (num_new_devices < LWZ_MAX_DEVICES)
							new_devices[num_new_devices++] = devIndex;

						// log it
						LOG(LOGLEVEL_NORMAL, ".. Adding virtual LedWiz unit #%d for Pinscape Pico ports %d..%d\n", 
							devIndex + 1, basePortNum, lastPortNum);
					}
					else
					{
						// slot already taken - the original device overrides the Pinscape Pico
						LOG(LOGLEVEL_NORMAL, ".. LedWiz unit #%d is already populated as %s; cannot add a Pinscape Pico virtual unit here\n",
							devIndex + 1, d.device_name);
					}
				}
			}
		}
	}

	// add all of the newly found devices
	lwz_add(h, num_new_devices, new_devices);
}

static void lwz_freelist(lwz_context_t *h)
{
	for (int i = 0; i < LWZ_MAX_DEVICES; i++)
	{
		auto &d = h->devices[i];
		if (d.hudev != NULL)
		{
			usbdev_release(d.hudev);
			d.hudev = NULL;
		}
		if (d.psPico != nullptr)
		{
			d.psPico.reset();
		}
	}
}

// simple FIFO to move the WriteFile() calls to a separate thread
struct fifo_msg_t
{
	HUDEV hudev = NULL;
	std::shared_ptr<PinscapePico::FeedbackControllerInterface> psPico;
	int starting_port_num = 0;
	packet_type_t typ = PACKET_TYPE_NONE;
	size_t ndata = 0;
	uint8_t data[64];		// maximum USB HID packet size
};

// The Pinscape KL25Z and Pinscape Pico are USB 1.1 "full speed" devices,
// which can transact one frame per millisecond, or 1000 frames per second.
// I don't know the USB level of the Arduino that LWCloneU2 originally
// targeted; the original author's comments here said that "the device"
// had a bandwidth of 2kbps, but I don't know if "the device" in question
// is the genuine LedWiz or the Arduino-based clone.  Both are plausible,
// and the LedWiz empirically can't handle more than about one command
// every 16ms, or about 64 commands per second.  The Pinscape devices,
// and probably other newer clones like the Zebsboard devices, can handle
// much higher rates, so let's allow for a fairly large backlog in the
// queue.  It's probably not desirable to make the queue *too* big, though,
// since a long queue could turn into significant latency in the effects 
// triggering on the device.  It's probably better to stall the client 
// application before the backlog grows too large, in case the client is 
// in some kind of pathological situation where it's sending updates at 
// an unthrottled pace.
#define QUEUE_LENGTH 256

typedef struct {
	int rpos;
	int wpos;
	int level;
	int state;
	HANDLE hthread;
	CRITICAL_SECTION cs;
	HANDLE hrevent;
	HANDLE hwevent;
	HANDLE heevent;
	HANDLE hqevent;
	bool rblocked;
	bool wblocked;
	bool eblocked;
	fifo_msg_t buf[QUEUE_LENGTH];
} queue_t;


static DWORD WINAPI QueueThreadProc(LPVOID lpParameter)
{
	queue_t * const h = (queue_t*)lpParameter;

	for (;;)
	{
		uint8_t buffer[64];

		packet_type_t packet_type;
		HUDEV hudev = NULL;
		std::shared_ptr<PinscapePico::FeedbackControllerInterface> psPico;
		int starting_port_num;
		size_t ndata = queue_shift(h, packet_type, &hudev, psPico, starting_port_num, &buffer[0], sizeof(buffer));

		// exit thread if requested by an empty packet
		if (ndata == 0)
			break;

		// check the underlying device type
		if (hudev != NULL)
		{
			// direct HID device handle
			usbdev_write(hudev, &buffer[0], ndata);
			usbdev_release(hudev);
		}
		else if (psPico != nullptr)
		{
			// Pinscape Pico interface
			switch (packet_type)
			{
			case PACKET_TYPE_SBA:
				// parse the queued SBA message data and pass to the Pico unit
				psPico->LedWizSBA(starting_port_num, buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], 1000);
				break;

			case PACKET_TYPE_PBA:
				// parse the queued PBA message, which simply consists of the 32 "profile"
				// bytes, and pass it to the Pico
				psPico->LedWizPBA(starting_port_num, 32, buffer, 1000);
				break;

			case PACKET_TYPE_RAW:
				// pass the buffer to the Pico as a raw write
				psPico->Write(buffer, ndata, 1000);
				break;
			}
		}
	}

	SetEvent(h->hqevent);

	return 0;
}

static void queue_close(HQUEUE hqueue, bool unload)
{
	queue_t * const h = (queue_t*)hqueue;

	if (h == NULL)
		return;

	if (h->hthread != NULL)
	{
		// Add a special "quit" item to the queue, identified by a zero
		// data length.  The thread quits when it reads this item.
		queue_push(h, NULL, nullptr, 0, PACKET_TYPE_RAW, NULL, 0);

		if (unload)
		{
			// we can *not* wait for the thread itself
			// if we are closed within the DLL unload.
			// this would result in a deadlock
			// instead we sync with the 'hqevent' that is set at the end of the thread routine
			WaitForSingleObject(h->hqevent, INFINITE);
			CloseHandle(h->hthread);
			h->hthread = NULL;
		}
		else
		{
			WaitForSingleObject(h->hthread, INFINITE);
			CloseHandle(h->hthread);
			h->hthread = NULL;
		}
	}

	// make sure that all queue elements are cleared
	for (auto &c : h->buf)
		c.psPico.reset();

	if (h->hrevent)
	{
		CloseHandle(h->hrevent);
		h->hrevent = NULL;
	}

	if (h->hwevent)
	{
		CloseHandle(h->hwevent);
		h->hwevent = NULL;
	}

	if (h->hqevent)
	{
		CloseHandle(h->hqevent);
		h->hqevent = NULL;
	}

	DeleteCriticalSection(&h->cs);

	free(h);
}

static HQUEUE queue_open(void)
{
	queue_t * const h = (queue_t*)malloc(sizeof(queue_t));

	if (h == NULL) {
		return NULL;
	}

	memset(h, 0x00, sizeof(queue_t));

	InitializeCriticalSection(&h->cs);

	h->hrevent = CreateEvent(NULL, FALSE, FALSE, NULL);
	h->hwevent = CreateEvent(NULL, FALSE, FALSE, NULL);
	h->heevent = CreateEvent(NULL, FALSE, FALSE, NULL);
	h->hqevent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (h->hrevent == NULL ||
		h->hwevent == NULL ||
		h->heevent == NULL ||
		h->hqevent == NULL )
	{
		goto Failed;
	}

	h->hthread = CreateThread(NULL, 0, QueueThreadProc, (void*)h, 0, NULL);

	if (h->hthread == NULL) {
		goto Failed;
	}

	return h;

Failed:
	queue_close(h, false);
	return NULL;
}

static void queue_wait_empty(HQUEUE hqueue)
{
	queue_t * const h = (queue_t*)hqueue;

	for (;;)
	{
		{
			AUTOLOCK(h->cs);

			if (h->state != 0) {
				return;
			}

			if (h->level == 0 && h->rblocked)
			{
				h->eblocked = false;
				return;
			}

			h->eblocked = true;
		}

		WaitForSingleObject(h->heevent, INFINITE);
	}
}

static size_t queue_push(HQUEUE hqueue, 
	HUDEV hudev, std::shared_ptr<PinscapePico::FeedbackControllerInterface> *psPico,
	int starting_port_num, packet_type_t typ, uint8_t const *pdata, size_t ndata)
{
	queue_t * const h = (queue_t*)hqueue;

	if (pdata == NULL || ndata == 0 || ndata > sizeof(h->buf[0].data)) 
	{
		// push empty chunk to signal shutdown
		pdata = NULL;
		ndata = 0;
		hudev = NULL;
	}

	for (;;)
	{
		bool do_wait = false;
		bool do_unblock = false;

		// check if there is some space to write into the queue
		{
			AUTOLOCK(h->cs);

			if (h->state != 0) {
				return 0;
			}

			int const nfree = QUEUE_LENGTH - h->level;
			bool combined = false;

			// If this is a PBA message, overwrite any PBA message already in
			// the queue with the new message rather than adding the new one
			// as a separate message.  A PBA overwrites all brightness levels,
			// so a newer message always supersedes a previous one.  If there's
			// one in the queue, it means that we haven't even tried sending
			// the last one to the device yet, so commands are coming faster
			// than the device can accept them.  It's better to apply the
			// latest update in this case than to apply all of the intermediate
			// updates getting here.  This makes fades less smooth, but that's
			// better than latency.
			if (typ == PACKET_TYPE_PBA)
			{
				for (int i = 0, pos = h->rpos ; i < h->level ;
					 ++i, pos = (pos + 1) % QUEUE_LENGTH)
				{
					fifo_msg_t *chunk = &h->buf[pos];
					if (chunk->hudev == hudev)
					{
						if (chunk->typ == PACKET_TYPE_PBA)
						{
							memcpy(chunk->data, pdata, ndata);
							combined = true;
							break;
						}
					}
				}
			}

			// If this is an SBA message, we can overwrite the last SBA in
			// the queue, but only if there's no PBA following it in the queue.
			// As with PBA, an SBA message sets all outputs, so each SBA message
			// effectively wipes out all traces of past SBA messages.  Further,
			// SBA and PBA messages are orthogonal, so the final state is always
			// the combination of the last SBA plus the last PBA so far, and isn't
			// affected by the order of execution of the final SBA and PBA.
			//
			// However, SBA and PBA have a subtle interaction that can be visible
			// to users.  An SBA that turns a port ON does so at the port's last
			// brightness setting.  Some clients (e.g., DOF) therefore are careful
			// to set the brightness for a port that's to be newly turned on
			// *before* turning the switch on - i.e., they send a PBA before
			// the SBA.  But the client might also have already sent an earlier
			// SBA that we haven't processed yet.  To handle this case correctly,
			// we need to make sure that we only overwrite an SBA if there are
			// no PBA messages later in the queue.
			if (typ == PACKET_TYPE_SBA)
			{
				// search for the last queued SBA not followed by a PBA
				int last_sba_pos = -1;
				for (int i = 0, pos = h->rpos ; i < h->level ;
					 ++i, pos = (pos + 1) % QUEUE_LENGTH)
				{
					// If this is an SBA, note it as the last one so far.  If
					// it's a PBA, forget any previous SBA, since we don't want
					// to overwrite an SBA followed by a PBA.
					fifo_msg_t *chunk = &h->buf[pos];
					if (chunk->hudev == hudev)
					{
						if (chunk->typ == PACKET_TYPE_SBA)
							last_sba_pos = pos;
						if (chunk->typ == PACKET_TYPE_PBA)
							last_sba_pos = -1;
					}
				}

				// if we found a suitable queued SBA, replace it
				if (last_sba_pos >= 0)
				{
					fifo_msg_t *chunk = &h->buf[last_sba_pos];
					memcpy(chunk->data, pdata, ndata);
					combined = true;
				}
			}

			if (combined)
			{
				// we combined this message with a prior message, so
				// there's no need to write it separately - we're done
			}
			else if (nfree <= 0)
			{
				h->wblocked = true;
				do_wait = true;
			}
			else
			{
				fifo_msg_t * const pc = &h->buf[h->wpos];

				if (hudev != NULL)
					usbdev_addref(hudev);

				pc->hudev = hudev;
				if (psPico != nullptr) pc->psPico = *psPico;
				pc->ndata = ndata;
				pc->typ = typ;
				pc->starting_port_num = starting_port_num;

				if (pdata != NULL)
					memcpy(&pc->data[0], pdata, ndata);

				h->wpos = (h->wpos + 1) % QUEUE_LENGTH;
				h->level += 1;

				h->wblocked = false;
				do_unblock = h->rblocked;
			}
		}

		// if the reader is blocked (because the queue was empty), signal that there is now data available
		if (do_unblock)
			SetEvent(h->hwevent);

		if (!do_wait)
			return ndata;

		// if we are here, the queue is full and we have to wait until the consumer reads something
		WaitForSingleObject(h->hrevent, INFINITE);
	}
}

static size_t queue_shift(HQUEUE hqueue, packet_type_t &packet_type,
	HUDEV *phudev, std::shared_ptr<PinscapePico::FeedbackControllerInterface> &psPico,
	int &starting_port_num, uint8_t *pbuffer, size_t nsize)
{
	queue_t * const h = (queue_t*)hqueue;

	if (phudev == NULL || pbuffer == NULL || nsize == 0 || nsize < sizeof(h->buf[0].data))
		return 0;

	for (;;)
	{
		bool do_wait = false;
		bool do_unblock = false;
		size_t nread = 0;

		// check if there is some data to read from the queue
		{
			AUTOLOCK(h->cs);

			if (h->state != 0)
				return 0;

			if (h->level <= 0)
			{
				h->rblocked = true;
				do_wait = true;

				if (h->eblocked)
					SetEvent(h->heevent);
			}
			else
			{
				fifo_msg_t *pc = &h->buf[h->rpos];

				// transfer the device handle
				*phudev = pc->hudev;
				pc->hudev = NULL;

				// transfer the Pinscape Pico handle
				psPico = pc->psPico;
				pc->psPico.reset();

				packet_type = pc->typ;
				starting_port_num = pc->starting_port_num;

				if (pc->ndata > 0) 
					memcpy(pbuffer, &pc->data[0], pc->ndata);
				else 
					h->state = 1; 

				nread = pc->ndata;

				h->rpos = (h->rpos + 1) % QUEUE_LENGTH;
				h->level -= 1;

				h->rblocked = false;
				do_unblock = h->wblocked;
			}
		}

		// if the writer is blocked (because the queue was full), signal that there is now some free space
		if (do_unblock)
			SetEvent(h->hrevent);

		if (!do_wait)
			return nread;

		// if we are here, the queue is empty and we have to wait until the producer writes something
		WaitForSingleObject(h->hwevent, INFINITE);
	}
}
