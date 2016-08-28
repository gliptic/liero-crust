#ifndef BMP_HPP
#define BMP_HPP 1

#include <tl/io/stream.hpp>
#include <tl/gfx/image.hpp>

int read_bmp(
	tl::Source& src,
	tl::Image& img,
	tl::Palette& pal);

#endif // BMP_HPP
