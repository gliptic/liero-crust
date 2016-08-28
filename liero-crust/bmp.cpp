#include "bmp.hpp"
#include <tl/memory.h>
#include <tl/bits.h>

TL_PACKED_STRUCT(BmpHeader)
	u16 magic;
	u32 file_size;
	u16 reserved0;
	u16 reserved1;
	u32 offset;
	u32 hsz;
TL_PACKED_STRUCT_END();

using tl::read_le;

#define CHECK(c) if(!(c)) goto fail

static int shiftsigned(u32 v, i32 shift, u32 bits) {
   
   if (shift < 0) v <<= -shift;
   else v >>= shift;
   u32 result = v;

   u32 z = bits;
   while (z < 8) {
	  result += v >> z;
	  z += bits;
   }

   return result;
}

int read_bmp(
	tl::Source& src,
	tl::Image& img,
	tl::Palette& pal) {

	u32 hsz, offset;

	{
		auto hdr = src.window(sizeof(BmpHeader));
		CHECK(!hdr.empty());
		BmpHeader const* header = (BmpHeader const*)hdr.begin();

		hsz = read_le(header->hsz);
		offset = read_le(header->offset);
	}

	if (hsz != 12 && hsz != 40 && hsz != 56 && hsz != 108 && hsz != 124) 
		return 1;

	u32 mr = 0, mg = 0, mb = 0, ma = 0;
	u32 dim_x, dim_y;
	u32 bitspp;
	u32 all_a = 0;

	{
		auto dib_hdr = src.window(hsz - 4);

		CHECK(!dib_hdr.empty());

		if (hsz == 12) {
			dim_x = dib_hdr.unsafe_get_le<u16>();
			dim_y = dib_hdr.unsafe_get_le<u16>();
		} else {
			dim_x = dib_hdr.unsafe_get_le<u32>();
			dim_y = dib_hdr.unsafe_get_le<u32>();
		}

		CHECK(dib_hdr.unsafe_get_le<u16>() == 1);

		bitspp = dib_hdr.unsafe_get_le<u16>();

		if (hsz <= 12) goto read_image;

		u32 compression = dib_hdr.unsafe_get_le<u32>();

		CHECK(compression != 1 && compression != 2);
		dib_hdr.unsafe_cut_front(4 * 5);

		if (hsz == 56) {
			dib_hdr.unsafe_cut_front(4 * 4);
		}

		if (hsz <= 56) {
			if (bitspp == 24) {
				mr = 0xff << 16;
				mg = 0xff << 8;
				mb = 0xff << 0;
			} else if (bitspp == 16 || bitspp == 32) {
				if (compression == 0) {
					if (bitspp == 32) {
						mr = 0xffu << 16u;
						mg = 0xffu << 8u;
						mb = 0xffu << 0u;
						ma = 0xffu << 24u;
					} else {
						mr = 31 << 10;
						mg = 31 << 5;
						mb = 31 << 0;
					}
				} else if (compression == 3) {
					mr = dib_hdr.unsafe_get_le<u32>();
					mg = dib_hdr.unsafe_get_le<u32>();
					mb = dib_hdr.unsafe_get_le<u32>();
				} else {
					goto fail;
				}
			}
		} else {
			mr = dib_hdr.unsafe_get_le<u32>();
			mg = dib_hdr.unsafe_get_le<u32>();
			mb = dib_hdr.unsafe_get_le<u32>();
			ma = dib_hdr.unsafe_get_le<u32>();
			dib_hdr.unsafe_cut_front(4 + 4 * 12);
		}

	read_image: ;
	}

	bool flip = true;
	if (dim_y >= 0x80000000) {
		flip = false;
		dim_y = u32(0) - dim_y;
	}

	u32 bytespp = bitspp > 8 ? 4 : 1;

	img.alloc_uninit(dim_x, dim_y, bytespp);

	if (bitspp >= 16) {
		src.skip(offset - 14 - hsz);

		i32 rshift = (i32)tl_fls(mr) - 7; u32 rcount = tl_popcount(mr);
		i32 gshift = (i32)tl_fls(mg) - 7; u32 gcount = tl_popcount(mg);
		i32 bshift = (i32)tl_fls(mb) - 7; u32 bcount = tl_popcount(mb);
		i32 ashift = (i32)tl_fls(ma) - 7; u32 acount = tl_popcount(ma);

		u32 line_size = dim_x * (bitspp >> 3);
		u32 pad = (0 - line_size) & 3;

		line_size += pad;

		u8* dest = img.data();

		for (u32 j = 0; j < dim_y; ++j) {

			auto line = src.window(line_size);
		 
			for (u32 i = 0; i < dim_x; ++i) {
				u32 p;
				if (bitspp == 16) p = (u32)line.unsafe_get_le<u16>();
				else if (bitspp == 32) p = line.unsafe_get_le<u32>();
				else {
					p = line.unsafe_pop_front();
					p |= (u32)line.unsafe_pop_front() << 8;
					p |= (u32)line.unsafe_pop_front() << 16;
				}

				*dest++ = (u8)shiftsigned(p & mr, rshift, rcount);
				*dest++ = (u8)shiftsigned(p & mg, gshift, gcount);
				*dest++ = (u8)shiftsigned(p & mb, bshift, bcount);

				u32 a = ma ? shiftsigned(p & ma, ashift, acount) : 255;
				all_a |= a;
				if (bytespp == 4) *dest++ = (u8)a;
			}
		}
	} else {
		u32 psize = 0;
		if (hsz == 12) {
			if (bitspp < 24) // ??? psize is only used if bitspp < 16
				psize = (offset - 14 - 24) / 3;
		} else {
			if (bitspp < 16)
				psize = (offset - 14 - hsz) >> 2;
		}

		if (psize == 0 || psize > 256) goto fail;

		for (u32 i = 0; i < psize; ++i) {
			u8 b = src.get_u8_def();
			u8 g = src.get_u8_def();
			u8 r = src.get_u8_def();
			pal.entries[i] = tl::Color(r, g, b, 255);

			if (hsz != 12) src.get_u8_def();
		}

		src.skip(offset - 14 - hsz - psize * (hsz == 12 ? 3 : 4));

		if (bitspp != 8) goto fail;

		u32 line_size = dim_x * (bitspp >> 3);
		u32 pad = (0 - line_size) & 3;
		line_size += pad;

		u8* dest = img.data();

		for (u32 j = 0; j < dim_y; ++j) {
			auto line = src.window(line_size);
			for (u32 i = 0; i < dim_x; ++i) {
				u8 v = line.unsafe_pop_front();
				*dest++ = v;
			}
		}
	}

	if (bytespp == 4 && all_a == 0) {
		u8* dest = img.data();
		for (u32 i = 3, e = 4 * dim_x * dim_y; i < e; i += 4) {
			dest[i] = 255;
		}
	}

	if (flip) {
		u8* dest = img.data();

		for (u32 j = 0; j < dim_y >> 1; ++j) {
			u8* p1 = dest + j * dim_x * bytespp;
			u8* p2 = dest + (dim_y - 1 - j) * dim_x * bytespp;
			for (u32 i = 0; i < dim_x * bytespp; ++i) {
				u8 temp = p1[i];
				p1[i] = p2[i];
				p2[i] = temp;
			}
		}
	}

	return 0;

fail:
	return 1;
}
