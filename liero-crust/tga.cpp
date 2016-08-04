#include "tga.hpp"
#include <tl/memory.h>

TL_PACKED_STRUCT(TgaHeader {
	u8 id_len;
	u8 dummy0;
	u8 dummy1;
	u16 pal_beg;
	u16 pal_end;
	u8 pal_bits;

	u16 dummy2;
	u16 dummy3;
	u16 width;
	u16 height;
	u8 dummy4;
	u8 dummy5;
});

TL_STATIC_ASSERT(sizeof(TgaHeader) == 3 + 5 + 4 + 4 + 2);

#define CHECK(c) if(!(c)) goto fail

using tl::read_le;

int read_tga(
	tl::source& src,
	tl::Image& img,
	tl::Palette& pal) {

	//CHECK(!src.ensure(sizeof(TgaHeader)));

	auto hdr = src.window(sizeof(TgaHeader));

	TgaHeader const* header = (TgaHeader const*)hdr.begin();
	
	u8 id_len = read_le(header->id_len);
	CHECK(read_le(header->dummy0) == 1);
	CHECK(read_le(header->dummy1) == 1);

	CHECK(read_le(header->pal_beg) == 0);
	CHECK(read_le(header->pal_end) == 256);
	CHECK(read_le(header->pal_bits) == 24);

	CHECK(read_le(header->dummy2) == 0);
	CHECK(read_le(header->dummy3) == 0);

	u16 image_width = read_le(header->width),
		image_height = read_le(header->height);

	CHECK(read_le(header->dummy4) == 8);
	CHECK(read_le(header->dummy5) == 0);

	//src.skip(sizeof(TgaHeader));
	hdr.done();

	src.skip(id_len);

	CHECK(!src.ensure(256 * 3));

	u8 const* p = src.begin();

	for (auto& entry : pal.entries) {
		u8 b = *p++;
		u8 g = *p++;
		u8 r = *p++;
		entry = tl::Color(r, g, b);
	}

	src.skip(256 * 3);

	img.alloc_uninit(image_width, image_height, 1);

	// TODO: This is only for 8-bit
	u8* pixels = img.data();
	u32 row_size = image_width * 1;
	for (u32 y = image_height; y-- > 0; ) {
		src.ensure(row_size);

		u8* dest = &pixels[y * row_size];
		memcpy(dest, src.begin(), row_size);

		src.skip(row_size);
	}

	return 0;

fail:
	return 1;
}

