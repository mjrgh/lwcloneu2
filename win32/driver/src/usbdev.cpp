/*
 *   LWCloneU2 Copyright (C) 2013 Andreas Dittrich <lwcloneu2@cithraidt.de>
 *   
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the
 *   Free Software Foundation; either version 2 of the License, or (at your
 *   option) any later version.
 *   
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *   
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <crtdbg.h>
#include <windows.h>
#include "usbdev.h"


static void usbdev_close_internal(HUDEV hudev);

// maximum wait time for reading/writing, in milliseconds
#define USB_READ_TIMEOUT_MS             500
#define USB_WRITE_TIMEOUT_MS            500

// minimum interval between consecutive writes for a real LedWiz unit, in milliseconds
#define LEDWIZ_MIN_WRITE_INTERVAL_MS    5


struct CAutoLockCS  // helper class to lock a critical section, and unlock it automatically
{
	CRITICAL_SECTION *m_pcs;
	CAutoLockCS(CRITICAL_SECTION *pcs) { m_pcs = pcs; EnterCriticalSection(m_pcs); };
	~CAutoLockCS() { LeaveCriticalSection(m_pcs); };
};

#define AUTOLOCK(cs) CAutoLockCS lock_##__LINE__##__(&cs)   // helper macro for using the helper class



typedef struct {
	CRITICAL_SECTION cslock;
	HANDLE hrevent;
	HANDLE hwevent;
	HANDLE hdev;
	LONG refcount;

	// The firmware in real LedWiz units seems to have a serious bug
	// in its USB interface that allows an incoming packet to overwite
	// the previous packet while the previous packet is still being
	// decoded.  The bug is triggered if writes are sent too quickly.
	// It manifests as output ports being set to random brightness
	// levels and random on/off values, since the packet decoder
	// incorrectly tries to read some of the bytes from the newer
	// packet as though they were part of the previous packet.
	// There's no way to completely avoid this bug, since we don't
	// have control over the exact timing of data going out on the
	// USB wire; there are many layers of Windows APIs and device
	// drivers between us and the wire.  However, it does help to
	// throttle our write rate.  This at least mitigates the bug
	// considerably.  A delay time of 5-10 ms generally makes the
	// bug infrequent enough not to be bothersome, although the
	// padding needed can increase depending on what other software
	// is running in the system, probably because other processes can
	// affect the actual wire data timing somewhat.

	//
	// LedWiz emulators (such as an Arduino running LWCloneU2, or a
	// KL25Z running the Pinscape controller software) generally do
	// NOT suffer from this problem.  The bug clearly seems to be in
	// the real LedWiz firmware.  It's NOT a bug in Windows, the USB
	// drivers, the USB hardware, or any other potential suspects.
	// USB is such a black box that it's easy to think this is just
	// some general USB flakiness, but it can easily be shown that
	// it's not, such as by comparing Pinscape unit behavior.  The
	// known emulators don't have the bug and dno't need the delay.
	// We therefore allow the caller to set/ the timing per device,
	// in case it wants to look at the USB HID descriptors to
	// determine exactly what kind of physical device we're
	// addressing and choose the delay timing accordingly.
	// For a real LedWiz, the timing should be 5 to 10 ms.  For 
	// an emulator, it can be 0 ms.

	DWORD last_write_ticks;				// system tick count (milliseconds) at time of last write operation
	unsigned int min_write_interval;	// minimum delay time between consecutive writes
} usbdev_context_t;


HUDEV usbdev_create(LPCSTR devicepath)
{
	// create context

	usbdev_context_t * const h = (usbdev_context_t*)malloc(sizeof(usbdev_context_t));

	if (h == NULL)
		return NULL;

	memset(h, 0x00, sizeof(*h));
	h->hdev = INVALID_HANDLE_VALUE;

	// presume this is a real LedWiz, so set the minimum write interval
	h->min_write_interval = LEDWIZ_MIN_WRITE_INTERVAL_MS;
	h->last_write_ticks = GetTickCount();

	InitializeCriticalSection(&h->cslock);

	h->hrevent = CreateEvent(NULL, TRUE, FALSE, NULL);
	h->hwevent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (h->hrevent == NULL ||
		h->hwevent == NULL)
	{
		goto Failed;
	}

	// open device

	HANDLE hdev  = CreateFileA(
		devicepath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		NULL);

	if (hdev != INVALID_HANDLE_VALUE)
	{
		h->hdev = hdev;
		h->refcount = 1;
		return h;
	}

	Failed:
	usbdev_close_internal(h);
	return NULL;
}

void usbdev_set_min_write_interval(HUDEV hudev, unsigned int interval_ms)
{
	usbdev_context_t * const h = (usbdev_context_t*)hudev;
	if (h != NULL)
	{
		h->min_write_interval = interval_ms;
	}
}

static void usbdev_close_internal(HUDEV hudev)
{
	usbdev_context_t * const h = (usbdev_context_t*)hudev;

	if (h == NULL)
		return;

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

	if (h->hdev != INVALID_HANDLE_VALUE)
	{
		CloseHandle(h->hdev);
		h->hdev = INVALID_HANDLE_VALUE;
	}

	DeleteCriticalSection(&h->cslock);

	free(h);
}

void usbdev_release(HUDEV hudev)
{
	usbdev_context_t * const h = (usbdev_context_t*)hudev;

	if (h != NULL)
	{
		LONG refcount_new = InterlockedDecrement(&h->refcount);

		if (refcount_new <= 0)
		{
			usbdev_close_internal(h);
		}
	}
}

void usbdev_addref(HUDEV hudev)
{
	usbdev_context_t * const h = (usbdev_context_t*)hudev;

	if (h != NULL)
	{
		InterlockedIncrement(&h->refcount);
	}
}

HANDLE usbdev_handle(HUDEV hudev)
{
	usbdev_context_t * const h = (usbdev_context_t*)hudev;

	if (h == NULL)
		return INVALID_HANDLE_VALUE;

	return h->hdev;
}

size_t usbdev_read(HUDEV hudev, void *psrc, size_t ndata)
{
	usbdev_context_t * const h = (usbdev_context_t*)hudev;

	if (h == NULL)
		return NULL;

	BYTE * pdata = (BYTE*)psrc;

	if (pdata == NULL)
		return 0;

	if (ndata > 64)
		ndata = 64;

	AUTOLOCK(h->cslock);

	int res = 0;
	BYTE buffer[65];
	DWORD nread = 0;
	BOOL bres = FALSE;

	OVERLAPPED ol = {};
	ol.hEvent = h->hrevent;

	bres = ReadFile(h->hdev, buffer, ndata + 1, NULL, &ol);

	if (bres != TRUE)
	{
		DWORD dwerror = GetLastError();

		if (dwerror == ERROR_IO_PENDING)
		{
			if (WaitForSingleObject(h->hrevent, USB_READ_TIMEOUT_MS) != WAIT_OBJECT_0)
			{
				CancelIo(h->hdev);
			}

			bres = TRUE;
		}
	}

	if (bres == TRUE)
	{
		bres = GetOverlappedResult(
			h->hdev,
			&ol,
			&nread,
			TRUE);
	}

	if (bres != TRUE)
	{
		DWORD dwerror = GetLastError();
		_ASSERT(0);
	}

	if (nread <= 1 || bres != TRUE)
		return 0;

	nread -= 1; // skip report id

	if (ndata > nread)
		ndata = nread;

	memcpy(pdata, &buffer[1], ndata);

	return ndata;
}

// Clear pending input.  This reads and discards input from the device
// as long as we have buffered input, then returns.  This can be used
// to discard unwanted joystick status reports when preparing to send
// a control request.
void usbdev_clear_input(HUDEV hudev, size_t input_rpt_len)
{
	usbdev_context_t * const h = (usbdev_context_t*)hudev;
	AUTOLOCK(h->cslock);
	for (int i = 0 ; i < 64 ; ++i)
	{
		// make sure the requested length is within range
		BYTE buffer[65];
		if (input_rpt_len > sizeof(buffer))
			input_rpt_len = sizeof(buffer);

		// start a non-blocking read
		OVERLAPPED ol = {};
		ol.hEvent = h->hrevent;
		if (ReadFile(h->hdev, buffer, input_rpt_len, NULL, &ol))
		{
			// success - get the result to complete the I/O, then keep
			// going, since the point is to clear buffered input
			DWORD nread;
			GetOverlappedResult(h->hdev, &ol, &nread, TRUE);
		}
		else
		{
			// The read failed.  If the result is "pending", it means that
			// the read was successfully initiated but that there's nothing
			// available to satisfy the read immediately, hence the buffer
			// is empty.  That's what we're after, so failure == success in
			// this case.  Cancel the pending read, since we didn't actually
			// want the data, we just wanted to know that there were no data.
			DWORD dwerror = GetLastError();
			if (dwerror == ERROR_IO_PENDING)
				CancelIo(h->hdev);

			// stop looping - consider the buffer emptied
			break;
		}
	}
}

size_t usbdev_write(HUDEV hudev, void const *pdst, size_t ndata)
{
	usbdev_context_t * const h = (usbdev_context_t*)hudev;

	if (h == NULL)
		return NULL;

	BYTE const * pdata = (BYTE const *)pdst;

	if (pdata == NULL || ndata == 0)
		return 0;

	if (ndata > 32)
		ndata = 32;

	AUTOLOCK(h->cslock);

	int res = 0;
	DWORD nbyteswritten = 0;

	BYTE buf[9]; 
	buf[0] = 0; // report id

	// write the data in 8-byte chunks
	while (ndata > 0)
	{
		int const ncopy = (ndata > 8) ? 8 : ndata;

		memset(&buf[1], 0x00, 8);
		memcpy(&buf[1], pdata, ncopy);
		pdata += ncopy;
		ndata -= ncopy;

		DWORD nwritten = 0;
		DWORD const nwrite = 9;

		OVERLAPPED ol = {};
		ol.hEvent = h->hwevent;

		// make sure we space out writes by the minimum interval
		DWORD now = GetTickCount();
		DWORD dt = now - h->last_write_ticks;
		if (dt < h->min_write_interval)
			Sleep(h->min_write_interval - dt);

		// write the bytes
		BOOL bres = WriteFile(h->hdev, buf, nwrite, NULL, &ol);
		if (!bres)
		{
			DWORD dwerror = GetLastError();
			if (dwerror == ERROR_IO_PENDING)
			{
				bres = TRUE;
				if (WaitForSingleObject(h->hwevent, USB_WRITE_TIMEOUT_MS) != WAIT_OBJECT_0)
				{
					bres = FALSE;
					CancelIo(h->hdev);
				}
			}
		}

		// if the write completed, get the result
		if (bres)
			bres = GetOverlappedResult(h->hdev,	&ol, &nwritten, TRUE);

		// update the last write time
		h->last_write_ticks = GetTickCount();

		// note any failure in debug builds
		if (!bres)
		{
			DWORD dwerror = GetLastError();
			_ASSERT(0);
		}

		// if the write failed, or didn't send the expected number of bytes, stop
		if (nwritten != nwrite || !bres)
			break;

		// success - count the bytes written and continue with anything still pending
		nbyteswritten += ncopy;
	}

	return nbyteswritten;
}

