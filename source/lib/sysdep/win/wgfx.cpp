/**
 * =========================================================================
 * File        : wgfx.cpp
 * Project     : 0 A.D.
 * Description : graphics card detection on Windows.
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#include "precompiled.h"
#include "lib/sysdep/gfx.h"

#include "dll_ver.h"	// dll_list_*
#include "win_internal.h"

#if MSC_VERSION
#pragma comment(lib, "advapi32.lib")	// registry
#endif


// useful for choosing a video mode.
// if we fail, outputs are unchanged (assumed initialized to defaults)
LibError gfx_get_video_mode(int* xres, int* yres, int* bpp, int* freq)
{
	// don't use EnumDisplaySettingsW - BoundsChecker reports it causes
	// a memory overrun (even if called as the very first thing, before
	// static CRT initialization).

	DEVMODEA dm;
	memset(&dm, 0, sizeof(dm));
	dm.dmSize = sizeof(dm);
	// dm.dmDriverExtra already set to 0 by memset

	if(!EnumDisplaySettingsA(0, ENUM_CURRENT_SETTINGS, &dm))
		WARN_RETURN(ERR::FAIL);

	if(dm.dmFields & (DWORD)DM_PELSWIDTH && xres)
		*xres = (int)dm.dmPelsWidth;
	if(dm.dmFields & (DWORD)DM_PELSHEIGHT && yres)
		*yres = (int)dm.dmPelsHeight;
	if(dm.dmFields & (DWORD)DM_BITSPERPEL && bpp)
		*bpp  = (int)dm.dmBitsPerPel;
	if(dm.dmFields & (DWORD)DM_DISPLAYFREQUENCY && freq)
		*freq = (int)dm.dmDisplayFrequency;

	return INFO::OK;
}


// useful for determining aspect ratio.
// if we fail, outputs are unchanged (assumed initialized to defaults)
LibError gfx_get_monitor_size(int& width_mm, int& height_mm)
{
	// (DC for the primary monitor's entire screen)
	HDC dc = GetDC(0);
	width_mm = GetDeviceCaps(dc, HORZSIZE);
	height_mm = GetDeviceCaps(dc, VERTSIZE);
	ReleaseDC(0, dc);
	return INFO::OK;
}


//-----------------------------------------------------------------------------

// EnumDisplayDevices is not available on Win95 or NT.
// try to import it manually here; return -1 if not available.
static BOOL (WINAPI *pEnumDisplayDevicesA)(LPCSTR, DWORD, LPDISPLAY_DEVICEA, DWORD);
static LibError import_EnumDisplayDevices()
{
	if(!pEnumDisplayDevicesA)
	{
		static HMODULE hUser32Dll = LoadLibrary("user32.dll");
		*(void**)&pEnumDisplayDevicesA = GetProcAddress(hUser32Dll, "EnumDisplayDevicesA");

		// do not free the DLL reference! if this happens to be the first
		// use of user32 (possible if it's delay-loaded or otherwise unused),
		// it would actually be unloaded. that apparently breaks the
		// Windows cursor.
		// unfortunately there's no way to get at the reference count,
		// so this resource leak is unavoidable.
	}

	return pEnumDisplayDevicesA? INFO::OK : ERR::FAIL;
}


static LibError win_get_gfx_card()
{
	// make sure EnumDisplayDevices is available (as pEnumDisplayDevicesA)
	if(import_EnumDisplayDevices() >= 0)
	{
		// loop through all display adapters and put the primary device's
		// identifier in gfx_card.
		// (we don't bother returning a list of all of them because it's
		// likely to be redundant, e.g. "Radeon" and "Radeon Secondary";
		// we're only interested in the 'main' card anyway)
		DISPLAY_DEVICEA dd = { sizeof(DISPLAY_DEVICEA) };
		for(DWORD dev_num = 0; ; dev_num++)
		{
			if(!pEnumDisplayDevicesA(0, dev_num, &dd, 0))
				break;

			// ignore mirror pseudo-devices (installed e.g. by NetMeeting)
			if(dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER)
				continue;

			if(dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
			{
				strcpy_s(gfx_card, ARRAY_SIZE(gfx_card), (const char*)dd.DeviceString);
				return INFO::OK;
			}
		}
	}

	WARN_RETURN(ERR::FAIL);
}


// note: this implementation doesn't require OpenGL to be initialized.
static LibError win_get_gfx_drv_ver()
{
	// don't overwrite existing information
	if(gfx_drv_ver[0] != '\0')
		return INFO::SKIPPED;

	// rationale:
	// - we could easily determine the 2d driver via EnumDisplaySettings,
	//   but we want to query the actual OpenGL driver. see
	//   http://www.opengl.org/discussion_boards/ubb/Forum3/HTML/009679.html ;
	//   in short, we need the exact OpenGL driver version because some
	//   driver packs (e.g. Omega) mix and match DLLs.
	// - an alternative implementation would be to enumerate all
	//   DLLs loaded into the process, and check for a glBegin export.
	//   that requires toolhelp/PSAPI (a bit complicated) and telling
	//   ICD/opengl32.dll apart (not future-proof).
	// - therefore, we stick with the OpenGLDrivers approach. since there is
	//   no good way to determine which of the subkeys (e.g. nvoglnt) is
	//   active (several may exist due to previously removed drivers),
	//   we just display all of them. it is obvious from looking at
	//   gfx_card which one is correct; we thus avoid driver-specific
	//   name checks and reporting incorrectly.

	LibError ret  = ERR::FAIL;	// single point of exit (for RegCloseKey)
	DWORD i;
	char drv_name[MAX_PATH+1];

	dll_list_init(gfx_drv_ver, GFX_DRV_VER_LEN);

	HKEY hkOglDrivers;
	const char* key = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\OpenGLDrivers";
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hkOglDrivers) != 0)
		WARN_RETURN(ERR::FAIL);

	// for each subkey (i.e. set of installed OpenGL drivers):
	for(i = 0; ; i++)
	{
		char set_name[32];
		DWORD set_name_len = ARRAY_SIZE(set_name);
		LONG err = RegEnumKeyEx(hkOglDrivers, i, set_name, &set_name_len, 0, 0,0, 0);
		if(err != ERROR_SUCCESS)	// error or no more items - bail
			break;

		HKEY hkSet;
		if(RegOpenKeyEx(hkOglDrivers, set_name, 0, KEY_QUERY_VALUE, &hkSet) == 0)
		{
			DWORD drv_name_len = ARRAY_SIZE(drv_name)-5;	// for ".dll"
			if(RegQueryValueEx(hkSet, "Dll", 0, 0, (LPBYTE)drv_name, &drv_name_len) == 0)
				ret = dll_list_add(drv_name);

			RegCloseKey(hkSet);
		}
	}	// for each subkey

	// for each value:
	// (some old drivers, e.g. S3 Super Savage, store their ICD name in a
	// single REG_SZ value. we therefore include those as well.)
	for(i = 0; ; i++)
	{
		char value_name[100];	// we don't need this, but RegEnumValue fails otherwise.
		DWORD value_name_len = ARRAY_SIZE(value_name);
		DWORD type;
		DWORD drv_name_len = ARRAY_SIZE(drv_name)-5;	// for ".dll"
		DWORD err = RegEnumValue(hkOglDrivers, i, value_name,&value_name_len,
			0, &type, (LPBYTE)drv_name,&drv_name_len);
		if(err != ERROR_SUCCESS)	// error or no more items - bail
			break;
		if(type == REG_SZ)
			ret = dll_list_add(drv_name);
	}	// for each value

	RegCloseKey(hkOglDrivers);
	return ret;
}


LibError win_get_gfx_info()
{
	LibError err1 = win_get_gfx_card();
	LibError err2 = win_get_gfx_drv_ver();

	// don't exit before trying both
	RETURN_ERR(err1);
	RETURN_ERR(err2);
	return INFO::OK;
}
