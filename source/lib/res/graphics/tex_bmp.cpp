/**
 * =========================================================================
 * File        : tex_bmp.cpp
 * Project     : 0 A.D.
 * Description : Windows BMP codec
 *
 * @author Jan.Wassenberg@stud.uni-karlsruhe.de
 * =========================================================================
 */

/*
 * Copyright (c) 2004 Jan Wassenberg
 *
 * Redistribution and/or modification are also permitted under the
 * terms of the GNU General Public License as published by the
 * Free Software Foundation (version 2 or later, at your option).
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "precompiled.h"

#include "lib/byte_order.h"
#include "tex_codec.h"

#pragma pack(push, 1)

struct BmpHeader
{ 
	// BITMAPFILEHEADER
	u16 bfType;			// "BM"
	u32 bfSize;			// of file
	u16 bfReserved1;
	u16 bfReserved2;
	u32 bfOffBits;		// offset to image data

	// BITMAPINFOHEADER
	u32 biSize;
	long biWidth;
	long biHeight;
	u16 biPlanes;
	u16 biBitCount;
	u32 biCompression;
	u32 biSizeImage;
	// the following are unused and zeroed when writing:
	long biXPelsPerMeter;
	long biYPelsPerMeter;
	u32 biClrUsed;
	u32 biClrImportant;
};

#pragma pack(pop)

#define BI_RGB 0		// biCompression


static LibError bmp_transform(Tex* UNUSED(t), uint UNUSED(transforms))
{
	return INFO_TEX_CODEC_CANNOT_HANDLE;
}


static bool bmp_is_hdr(const u8* file)
{
	// check header signature (bfType == "BM"?).
	// we compare single bytes to be endian-safe.
	return (file[0] == 'B' && file[1] == 'M');
}


static bool bmp_is_ext(const char* ext)
{
	return !stricmp(ext, "bmp");
}


static size_t bmp_hdr_size(const u8* file)
{
	const size_t hdr_size = sizeof(BmpHeader);
	if(file)
	{
		BmpHeader* hdr = (BmpHeader*)file;
		const u32 ofs = read_le32(&hdr->bfOffBits);
		debug_assert(ofs >= hdr_size && "bmp_hdr_size invalid");
		return ofs;
	}
	return hdr_size;
}


// requirements: uncompressed, direct colour, bottom up
static LibError bmp_decode(DynArray* restrict da, Tex* restrict t)
{
	u8* file         = da->base;

	const BmpHeader* hdr = (const BmpHeader*)file;
	const long w       = (long)read_le32(&hdr->biWidth);
	const long h_      = (long)read_le32(&hdr->biHeight);
	const u16 bpp      = read_le16(&hdr->biBitCount);
	const u32 compress = read_le32(&hdr->biCompression);

	const long h = abs(h_);

	uint flags = 0;
	flags |= (h_ < 0)? TEX_TOP_DOWN : TEX_BOTTOM_UP;
	if(bpp > 16)
		flags |= TEX_BGR;
	if(bpp == 32)
		flags |= TEX_ALPHA;

	// sanity checks
	if(compress != BI_RGB)
		WARN_RETURN(ERR_TEX_COMPRESSED);

	t->w     = w;
	t->h     = h;
	t->bpp   = bpp;
	t->flags = flags;
	return ERR_OK;
}


static LibError bmp_encode(Tex* restrict t, DynArray* restrict da)
{
	const size_t hdr_size = sizeof(BmpHeader);	// needed for BITMAPFILEHEADER
	const size_t img_size = tex_img_size(t);
	const size_t file_size = hdr_size + img_size;
	const long h = (t->flags & TEX_TOP_DOWN)? -(long)t->h : t->h;

	int transforms = t->flags;
	transforms &= ~TEX_ORIENTATION;	// no flip needed - we can set top-down bit.
	transforms ^= TEX_BGR;			// BMP is native BGR.

	const BmpHeader hdr =
	{
		// BITMAPFILEHEADER
		0x4d42,				// bfType = 'B','M'
		(u32)file_size,		// bfSize
		0, 0,				// bfReserved1,2
		hdr_size,			// bfOffBits

		// BITMAPINFOHEADER
		40,					// biSize = sizeof(BITMAPINFOHEADER)
		t->w,
		h,
		1,					// biPlanes
		t->bpp,
		BI_RGB,				// biCompression
		(u32)img_size,		// biSizeImage
		0, 0, 0, 0			// unused (bi?PelsPerMeter, biClr*)
	};
	return tex_codec_write(t, transforms, &hdr, hdr_size, da);
}

TEX_CODEC_REGISTER(bmp);
