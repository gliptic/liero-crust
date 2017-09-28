#ifndef LIEROSIM_CONFIG_HPP
#define LIEROSIM_CONFIG_HPP

#include <tl/vector.hpp>
#include <tl/approxmath/am.hpp>

#include "fixed.hpp"

#define UPDATE_POS_IMMEDIATE 1
#define QUEUE_REMOVE_NOBJS 0

namespace liero {

typedef f64 Ratio;
typedef Fixed Scalar;
typedef tl::BasicVector<Scalar, 2> Vector2;

inline Scalar operator"" _lf(unsigned long long v) {
	return Fixed::from_raw(i32(v));
}

inline Vector2 sincos(Scalar a) {
	auto v = tl::sincos_fixed2(a.raw());
	return Vector2(Fixed::from_raw(v.x), Fixed::from_raw(v.y));
}

inline tl::VectorD2 sincos_f64(Scalar a) {
	return tl::sincos_f64(a.raw());
}

inline Vector2 vector2(tl::VectorD2 v) {
	return Vector2(Scalar::from_raw((i32)v.x), Scalar::from_raw((i32)v.y));
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
inline Vector2 rand_max_vector2(Rand& rand, Scalar max) {
	auto v = rand.get_vectori2(max.raw());
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
