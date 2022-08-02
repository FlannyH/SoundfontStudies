#pragma once
#include "common.h"
#include <cmath>

struct fixed_32_32 {
	i32 integer;
	u32 fraction;
	fixed_32_32() {
		integer = 0;
		fraction = 0;
	}
	fixed_32_32(const double& rhs) {
		*this = rhs;
	}
	fixed_32_32 operator=(const double& rhs) {
		integer = (i32)rhs;
		fraction = (u32)(fmod(rhs, 1.0f) * ((float)UINT32_MAX + 1.0f));
		return *this;
	}
	fixed_32_32 operator=(const fixed_32_32& rhs) {
		integer = rhs.integer;
		fraction = rhs.fraction;
	}
	bool operator==(const fixed_32_32& rhs) const {
		return integer == rhs.integer && fraction == rhs.fraction;
	}
	fixed_32_32 operator+(const i32& rhs) { return integer + rhs; }
	fixed_32_32 operator-(const i32& rhs) { return integer - rhs; }
	fixed_32_32 operator*(const i32& rhs) { return *(i64*)this * rhs; }
	fixed_32_32 operator/(const i32& rhs) { return *(i64*)this / rhs; }
	fixed_32_32 operator+(const fixed_32_32& rhs) { *(i64*)this += *(i64*)&rhs; }
	fixed_32_32 operator-(const fixed_32_32& rhs) { *(i64*)this -= *(i64*)&rhs; }
	fixed_32_32 operator*(const fixed_32_32& rhs) {
		i64 step1 = (i64)integer * (i64)rhs.integer;	//results in 0xIIIIIIII_IIIIIIII
		i64 step2 = (i64)integer * (i64)rhs.fraction;	//results in 0xIIIIIIII_FFFFFFFF
		i64 step3 = (i64)fraction * (i64)rhs.integer;	//results in 0xIIIIIIII_FFFFFFFF
		i64 step4 = (i64)fraction * (i64)rhs.fraction;	//results in 0xFFFFFFFF_FFFFFFFF
		fixed_32_32 result;
		result.integer = (i32)(step1 + ((step2 + step3) >> 32));
		result.fraction = (u32)(((step2 + step3) & 0xFFFFFFFF) + (step4 >> 32));
		return result;
	}
};