/**
 * =========================================================================
 * File        : byte_order.h
 * Project     : 0 A.D.
 * Description : byte order (endianness) support routines.
 * =========================================================================
 */

// license: GPL; see lib/license.txt

#ifndef INCLUDED_BYTE_ORDER
#define INCLUDED_BYTE_ORDER

#include "lib/sysdep/cpu.h"

// detect byte order via predefined macros.
#ifndef BYTE_ORDER
# define LITTLE_ENDIAN 0x4321
# define BIG_ENDIAN    0x1234
# if ARCH_IA32 || ARCH_IA64 || ARCH_AMD64 || ARCH_ALPHA || ARCH_ARM || ARCH_MIPS || defined(__LITTLE_ENDIAN__)
#  define BYTE_ORDER LITTLE_ENDIAN
# else
#  define BYTE_ORDER BIG_ENDIAN
# endif
#endif


/**
 * convert 4 characters to u32 (at compile time) for easy comparison.
 * output is in native byte order; e.g. FOURCC_LE can be used instead.
 **/
#define FOURCC(a,b,c,d)	// real definition is below
#undef  FOURCC

// implementation rationale:
// - can't pass code as string, and use s[0]..s[3], because
//   VC6/7 don't realize the macro is constant
//   (it should be usable as a switch{} expression)
// - the casts are ugly but necessary. u32 is required because u8 << 8 == 0;
//   the additional u8 cast ensures each character is treated as unsigned
//   (otherwise, they'd be promoted to signed int before the u32 cast,
//   which would break things).

/// big-endian version of FOURCC
#define FOURCC_BE(a,b,c,d) ( ((u32)(u8)a) << 24 | ((u32)(u8)b) << 16 | \
	((u32)(u8)c) << 8  | ((u32)(u8)d) << 0  )

/// little-endian version of FOURCC
#define FOURCC_LE(a,b,c,d) ( ((u32)(u8)a) << 0  | ((u32)(u8)b) << 8  | \
	((u32)(u8)c) << 16 | ((u32)(u8)d) << 24 )

#if BYTE_ORDER == BIG_ENDIAN
# define FOURCC FOURCC_BE
#else
# define FOURCC FOURCC_LE
#endif


/// convert a little-endian number to/from native byte order.
extern u16 to_le16(u16 x);
extern u32 to_le32(u32 x);	/// see to_le16
extern u64 to_le64(u64 x);	/// see to_le16

/// convert a big-endian number to/from native byte order.
extern u16 to_be16(u16 x);
extern u32 to_be32(u32 x);	/// see to_be16
extern u64 to_be64(u64 x);	/// see to_be16

/// read a little-endian number from memory into native byte order.
extern u16 read_le16(const void* p);
extern u32 read_le32(const void* p);	/// see read_le16
extern u64 read_le64(const void* p);	/// see read_le16

/// read a big-endian number from memory into native byte order.
extern u16 read_be16(const void* p);
extern u32 read_be32(const void* p);	/// see read_be16
extern u64 read_be64(const void* p);	/// see read_be16

/// write a little-endian number to memory in native byte order.
extern void write_le16(void* p, u16 x);
extern void write_le32(void* p, u32 x);	/// see write_le16
extern void write_le64(void* p, u64 x);	/// see write_le16

/// write a big-endian number to memory in native byte order.
extern void write_be16(void* p, u16 x);
extern void write_be32(void* p, u32 x);	/// see write_be16
extern void write_be64(void* p, u64 x);	/// see write_be16

/**
 * zero-extend <size> (truncated to 8) bytes of little-endian data to u64,
 * starting at address <p> (need not be aligned).
 **/
extern u64 movzx_le64(const u8* p, size_t size);
extern u64 movzx_be64(const u8* p, size_t size);

/**
 * sign-extend <size> (truncated to 8) bytes of little-endian data to i64,
 * starting at address <p> (need not be aligned).
 **/
extern i64 movsx_le64(const u8* p, size_t size);
extern i64 movsx_be64(const u8* p, size_t size);


// Debug-mode ICC doesn't like the intrinsics, so only use them
// for MSVC and non-debug ICC.
#if MSC_VERSION && !( defined(__INTEL_COMPILER) && !defined(NDEBUG) )
extern unsigned short _byteswap_ushort(unsigned short);
extern unsigned long _byteswap_ulong(unsigned long);
extern unsigned __int64 _byteswap_uint64(unsigned __int64);
#pragma intrinsic(_byteswap_ushort)
#pragma intrinsic(_byteswap_ulong)
#pragma intrinsic(_byteswap_uint64)
# define swap16 _byteswap_ushort
# define swap32 _byteswap_ulong
# define swap64 _byteswap_uint64
#else
extern u16 swap16(const u16 x);
extern u32 swap32(const u32 x);
extern u64 swap64(const u64 x);
#endif	// no swap intrinsics


#endif	// #ifndef INCLUDED_BYTE_ORDER
