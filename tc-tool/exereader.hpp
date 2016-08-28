#ifndef EXEREADER_HPP
#define EXEREADER_HPP 1

#include <tl/io/stream.hpp>
#include <liero-sim/data/model.hpp>

namespace liero {

bool load_from_exe(ss::Builder& tcdata, tl::Source src);

}

#endif // EXEREADER_HPP
