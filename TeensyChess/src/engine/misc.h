/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Teensy port: a trimmed misc.h with only what the engine core needs
// (no iostream, threads, NUMA or large pages).

#ifndef MISC_H_INCLUDED
#define MISC_H_INCLUDED

// Asserts are compiled out unless SF_DEBUG is defined
#ifndef SF_DEBUG
    #ifndef NDEBUG
        #define NDEBUG
    #endif
#endif

#include <cassert>
#include <cstddef>
#include <cstdint>

// Large tables are placed in the Teensy 4's second RAM bank (OCRAM, 512 KB) so the
// fast DTCM bank stays free for the search stack. DMAMEM is NOT zeroed at boot,
// so every table placed there is cleared explicitly at init.
#if defined(ARDUINO)
    #define SF_BIGMEM __attribute__((section(".dmabuffers"), used))
#else
    #define SF_BIGMEM
#endif

#define sf_always_inline inline __attribute__((always_inline))

namespace Stockfish {

using u64 = std::uint64_t;
using u32 = std::uint32_t;
using u16 = std::uint16_t;
using u8  = std::uint8_t;

using i64 = std::int64_t;
using i32 = std::int32_t;
using i16 = std::int16_t;
using i8  = std::int8_t;

using usize = std::size_t;
using isize = std::ptrdiff_t;

// Milliseconds since boot
using TimePoint = i64;
TimePoint now();

template<typename T, usize MaxSize>
class ValueList {

   public:
    usize size() const { return size_; }
    void  push_back(const T& value) {
        assert(size_ < MaxSize);
        values_[size_++] = value;
    }
    const T* begin() const { return values_; }
    const T* end() const { return values_ + size_; }
    const T& operator[](int index) const { return values_[index]; }

   private:
    T     values_[MaxSize];
    usize size_ = 0;
};

// xorshift64star Pseudo-Random Number Generator
// This class is based on original code written and dedicated
// to the public domain by Sebastiano Vigna (2014).
class PRNG {

    u64 s;

    u64 rand64() {

        s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
        return s * 2685821657736338717LL;
    }

   public:
    PRNG(u64 seed) :
        s(seed) {
        assert(seed);
    }

    template<typename T>
    T rand() {
        return T(rand64());
    }
};

// High 32 bits of (a * b) / 2^32 using only the top half of the key. Used to index
// the transposition table; the low key bits are kept for the in-cluster check.
inline u32 mul_hi64(u64 a, u32 b) { return u32(((a >> 32) * u64(b)) >> 32); }

}  // namespace Stockfish

#endif  // #ifndef MISC_H_INCLUDED
