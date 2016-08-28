#ifndef LIEROSIM_CONFIG_HPP
#define LIEROSIM_CONFIG_HPP

#include <tl/vector.hpp>
#include <tl/approxmath/am.hpp>

#include "fixed.hpp"

namespace liero {

typedef f64 Ratio;
typedef Fixed Scalar;
typedef tl::BasicVector<Scalar, 2> Vector2;

inline Scalar operator"" _lf(unsigned long long v) {
	return Fixed::from_raw(i32(v));
}

// TODO: Integer-based sincos
inline Vector2 sincos(Scalar a) {
#if 0
	double f = (a.raw() & ((1 << (16 + 7)) - 1)) * ((1.0 / 65536.0) * tl::pi2 / 128.0);
	auto v = tl::sincos(f);
	return v.cast<Scalar>();
#else
	auto v = tl::sincos_fixed2(a.raw());
	return Vector2(Fixed::from_raw(v.x), Fixed::from_raw(v.y));
#endif
}

inline Scalar fabs(Scalar v) { return Fixed::from_raw(abs(v.raw())); }

inline i32 raw(Scalar s) { return s.raw(); }

template<typename Rand>
inline Scalar rand_max(Rand& rand, Scalar max) {
	return Fixed::from_raw(rand.get_i32(max.raw()));
}

template<typename Rand>
inline Vector2 rand_max_vector2(Rand& rand, Ratio max) {
	auto v = rand.get_vectori2_f64(max);
	return Vector2(Fixed::from_raw(v.x), Fixed::from_raw(v.y));
}

template<typename Rand>
inline Vector2 rand_max_vector2(Rand& rand, tl::VectorD2 max) {
	auto v = rand.get_vectori2_f64(max);
	return Vector2(Fixed::from_raw(v.x), Fixed::from_raw(v.y));
}

inline i32 sign(i32 v) {
	return v < 0 ? -1 : (v > 0 ? 1 : 0);
}

}

#endif // LIEROSIM_CONFIG_HPP
