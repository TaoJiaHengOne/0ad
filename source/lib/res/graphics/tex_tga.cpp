#include "precompiled.h"

#include "lib/byte_order.h"
#include "tex_codec.h"


#pragma pack(push, 1)

enum TgaImgType
{
	TGA_TRUE_COLOUR = 2,	// uncompressed 24 or 32 bit direct RGB
	TGA_GREY        = 3		// uncompressed 8 bit direct greyscale
};

enum TgaImgDesc
{
	TGA_RIGHT_TO_LEFT = BIT(4),
	TGA_TOP_DOWN      = BIT(5),
};

typedef struct
{
	u8 img_id_len;			// 0 - no image identifier present
	u8 colour_map_type;		// 0 - no colour map present
	u8 img_type;			// see TgaImgType
	u8 colour_map[5];		// unused

	u16 x_origin;			// unused
	u16 y_origin;			// unused

	u16 w;
	u16 h;
	u8 bpp;					// bits per pixel

	u8 img_desc;
}
TgaHeader;

// TGA file: header [img id] [colour map] image data

#pragma pack(pop)


static int tga_transform(Tex* UNUSED(t), uint UNUSED(transforms))
{
	return TEX_CODEC_CANNOT_HANDLE;
}


static bool tga_is_hdr(const u8* file)
{
	TgaHeader* hdr = (TgaHeader*)file;

	// the first TGA header doesn't have a magic field;
	// we can only check if the first 4 bytes are valid
	// .. not direct colour
	if(hdr->colour_map_type != 0)
		return false;
	// .. wrong colour type (not uncompressed greyscale or RGB)
	if(hdr->img_type != TGA_TRUE_COLOUR && hdr->img_type != TGA_GREY)
		return false;

	// note: we can't check img_id_len or colour_map[0] - they are
	// undefined and may assume any value.

	return true;
}


static bool tga_is_ext(const char* ext)
{
	return !stricmp(ext, "tga");
}


static size_t tga_hdr_size(const u8* file)
{
	size_t hdr_size = sizeof(TgaHeader);
	if(file)
	{
		TgaHeader* hdr = (TgaHeader*)file;
		hdr_size += hdr->img_id_len;
	}
	return hdr_size;
}


// requirements: uncompressed, direct colour, bottom up
static int tga_decode(DynArray* da, Tex* t)
{
	u8* file         = da->base;
	size_t file_size = da->cur_size;

	TgaHeader* hdr = (TgaHeader*)file;
	const u8 type  = hdr->img_type;
	const uint w   = read_le16(&hdr->w);
	const uint h   = read_le16(&hdr->h);
	const uint bpp = hdr->bpp;
	const u8 desc  = hdr->img_desc;

	int flags = 0;
	flags |= (desc & TGA_TOP_DOWN)? TEX_TOP_DOWN : TEX_BOTTOM_UP;
	if(desc & 0x0f)	// alpha bits
		flags |= TEX_ALPHA;
	if(bpp == 8)
		flags |= TEX_GREY;
	if(type == TGA_TRUE_COLOUR)
		flags |= TEX_BGR;

	// sanity checks
	// .. storing right-to-left is just stupid;
	//    we're not going to bother converting it.
	if(desc & TGA_RIGHT_TO_LEFT)
		return ERR_TEX_INVALID_LAYOUT;

	t->w     = w;
	t->h     = h;
	t->bpp   = bpp;
	t->flags = flags;
	return 0;
}


static int tga_encode(Tex* t, DynArray* da)
{
	u8 img_desc = 0;
	if(t->flags & TEX_TOP_DOWN)
		img_desc |= TGA_TOP_DOWN;
	if(t->bpp == 32)
		img_desc |= 8;	// size of alpha channel
	TgaImgType img_type = (t->flags & TEX_GREY)? TGA_GREY : TGA_TRUE_COLOUR;

	int transforms = t->flags;
	transforms &= ~TEX_ORIENTATION;	// no flip needed - we can set top-down bit.
	transforms ^= TEX_BGR;			// TGA is native BGR.

	const TgaHeader hdr =
	{
		0,				// no image identifier present
		0,				// no colour map present
		(u8)img_type,
		{0,0,0,0,0},	// unused (colour map)
		0, 0,			// unused (origin)
		t->w,
		t->h,
		t->bpp,
		img_desc
	};
	const size_t hdr_size = sizeof(hdr);
	return tex_codec_write(t, transforms, &hdr, hdr_size, da);
}

TEX_CODEC_REGISTER(tga);
