// ReSharper disable CppInconsistentNaming
#pragma once
// ReSharper disable once CppUnusedIncludeDirective
#include <cstdint>
#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define i8 int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t
#define f32 float
#define f64 double

namespace Flan {
    template<typename T>
    T lerp(T a, T b, float t) {
        return a + (b - a) * t;
    }
}