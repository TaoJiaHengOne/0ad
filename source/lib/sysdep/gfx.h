/**
 * =========================================================================
 * File        : gfx.h
 * Project     : 0 A.D.
 * Description : graphics card detection.
 *
 * @author Jan.Wassenberg@stud.uni-karlsruhe.de
 * =========================================================================
 */

/*
 * Copyright (c) 2004-2005 Jan Wassenberg
 *
 * Redistribution and/or modification are also permitted under the
 * terms of the GNU General Public License as published by the
 * Free Software Foundation (version 2 or later, at your option).
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef GFX_H__
#define GFX_H__

#ifdef __cplusplus
extern "C" {
#endif

const size_t GFX_CARD_LEN = 128;
/**
 * description of graphics card.
 * initial value is "".
 **/
extern char gfx_card[GFX_CARD_LEN];

// note: increased from 64 by Joe Cocovich; this large size is necessary
// because there must be enough space to list the versions of all drivers
// mentioned in the registry (including unused remnants).
const size_t GFX_DRV_VER_LEN = 256;
/**
 * (OpenGL) graphics driver identification and version.
 * initial value is "".
 **/
extern char gfx_drv_ver[GFX_DRV_VER_LEN];

/**
 * approximate amount of graphics memory [MiB]
 **/
extern int gfx_mem;

/**
 * detect graphics card and set the above information.
 **/
extern void gfx_detect(void);


/**
 * get current video mode.
 *
 * this is useful when choosing a new video mode.
 *
 * @param xres, yres (optional out) resolution [pixels]
 * @param bpp (optional out) bits per pixel
 * @param freq (optional out) vertical refresh rate [Hz]
 * @return LibError; INFO_OK unless: some information was requested
 * (i.e. pointer is non-NULL) but cannot be returned.
 * on failure, the outputs are all left unchanged (they are
 * assumed initialized to defaults)
 **/
extern LibError gfx_get_video_mode(int* xres, int* yres, int* bpp, int* freq);

/**
 * get monitor dimensions.
 *
 * this is useful for determining aspect ratio.
 *
 * @param width_mm (out) screen width [mm]
 * @param height_mm (out) screen height [mm]
 * @return LibError. on failure, the outputs are all left unchanged
 * on failure, the outputs are all left unchanged (they are
 * assumed initialized to defaults)
 **/
extern LibError gfx_get_monitor_size(int& width_mm, int& height_mm);


#ifdef __cplusplus
}
#endif

#endif	// #ifndef GFX_H__
