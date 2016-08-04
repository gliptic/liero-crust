#ifndef BMP_HPP
#define BMP_HPP 1

#include <tl/stream.hpp>
#include <tl/image.hpp>

int read_bmp(
	tl::source& src,
	tl::Image& img,
	tl::Palette& pal);

#endif // BMP_HPP
