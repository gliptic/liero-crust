#ifndef LIEROSIM_FIXED_HPP
#define LIEROSIM_FIXED_HPP

#include <tl/std.h>

namespace liero {

#define CMPOP_FF(op) bool operator op (Fixed b) const { return this->s op b.s; }
#define BINOP_FF(op) Fixed operator op (Fixed b) const { return Fixed(this->s op b.s, raw_tag()); }

#define BINSETOP_FF(op) Fixed& operator op (Fixed b) { this->s op b.s; return *this; }

struct Fixed {

	Fixed() : s(0) {
	}

	explicit Fixed(i32 i) : s(i << 16) {
	}

	// TODO: Get rid of this
	explicit Fixed(f64 v) : s(i32(v * 65536.0)) {
	}
	
	CMPOP_FF(==)
	CMPOP_FF(!=)
	CMPOP_FF(>=)
	CMPOP_FF(>)
	CMPOP_FF(<=)
	CMPOP_FF(<)
	BINOP_FF(+)
	BINOP_FF(-)

	BINSETOP_FF(+=)
	BINSETOP_FF(-=)

#if 0
	Fixed operator*(Fixed b) const {
		//i64 prod = i64(this->s) * b.s;
		//return Fixed(i32(prod >> 16), raw_tag());
		return Fixed(i32(this->s * (b.s * (1.0 / (1 << 16)))), raw_tag());
	}
	
	Fixed& operator*=(Fixed b) {
		*this = *this * b;
		return *this;
	}
#endif

	Fixed operator*(f64 b) const {
		return Fixed(i32(this->s * b), raw_tag());
	}

	Fixed operator*(i32 b) const {
		return Fixed(this->s * b, raw_tag());
	}

	Fixed& operator*=(f64 b) {
		*this = *this * b;
		return *this;
	}

	Fixed& operator*=(i32 b) {
		*this = *this * b;
		return *this;
	}

	Fixed operator/(i32 b) const {
		return Fixed(this->s / b, raw_tag());
	}

	Fixed operator/(f64 b) const {
		return Fixed(i32(this->s / b), raw_tag());
	}

	Fixed& operator/=(i32 b) {
		*this = *this / b;
		return *this;
	}

	Fixed& operator/=(f64 b) {
		*this = *this / b;
		return *this;
	}

	Fixed operator+(i32 b) const {
		return *this + from_i32(b);
	}
	
	Fixed operator-() const { return Fixed(-this->s, raw_tag()); }

	i32 raw() const { return this->s; }

	explicit operator i32() const {
		return this->s >> 16;
	}

	explicit operator f64() const {
		return this->s * (1.0 / 65536.0);
	}

	static Fixed from_i32(i32 i) {
		return Fixed(i << 16);
	}

	static Fixed from_raw(i32 i) {
		return Fixed(i, raw_tag());
	}

	static Fixed from_frac(i32 nom, i32 denom) {
		return Fixed(i32((i64(nom) << 16) / denom), raw_tag());
	}

private:

	struct raw_tag {};

	Fixed(i32 s_init, raw_tag) : s(s_init) {
	}

	i32 s;
};

#undef BINOP_FF

}

#endif // LIEROSIM_FIXED_HPP
