#ifndef TGA_HPP
#define TGA_HPP 1

#include <tl/stream.hpp>
#include <tl/Image.hpp>

int read_tga(
	tl::source& src,
	tl::Image& img,
	tl::Palette& pal);

#endif // TGA_HPP
