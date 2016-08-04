#ifndef LIEROSIM_CONFIG_HPP
#define LIEROSIM_CONFIG_HPP

#define LIERO_FIXED 1

#include <tl/vector.hpp>
#include <tl/approxmath/am.hpp>

#if LIERO_FIXED
#include "fixed.hpp"
#else

#endif

namespace liero {

#if LIERO_FIXED
typedef Fixed Scalar;
typedef tl::BasicVector<Scalar, 2> Vector2;

inline Scalar operator"" _lf(unsigned long long v) {
	return Fixed::from_raw(i32(v));
}

inline Scalar operator"" _lspeed(unsigned long long v) {
	return Fixed::from_raw(i32((v << 16) / 100));
}

inline Scalar operator"" _langrel(unsigned long long v) {
	return Fixed::from_raw(i32(v));
}

static Scalar const liero_eps = Fixed(0);

inline Vector2 sincos(Scalar a) {
	double f = a.raw() * (1.0 / 65536.0) * tl::pi2 / 128.0;
	auto v = tl::sincos(f);
	return v.cast<Scalar>();
}

inline Scalar fabs(Scalar v) { return Fixed::from_raw(abs(v.raw())); }

inline i32 raw(Scalar s) { return s.raw(); }

template<typename Rand>
inline Scalar rand_max(Rand& rand, Scalar max) {
	return Fixed::from_raw(rand.get_unsigned(u32(max.raw())));
}

#else

using tl::sincos;

typedef f64 Scalar;
typedef tl::VectorD2 Vector2;

inline Scalar operator"" _lf(unsigned long long v) {
	return v / 65536.0;
}

inline Scalar operator"" _lspeed(unsigned long long v) {
	return v / 100.0;
}

inline Scalar operator"" _langrel(unsigned long long v) {
	return v * (tl::pi2 / 128.0 / 65536.0);
}

static Scalar const liero_eps = 0.5 / 65536.0;

inline Scalar raw(Scalar s) { return s; }

template<typename Rand>
inline Scalar rand_max(Rand& rand, Scalar max) {
	return rand.get_u01() * max;
}

#endif

}

#endif // LIEROSIM_CONFIG_HPP
