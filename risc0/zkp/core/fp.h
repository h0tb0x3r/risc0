// Copyright 2022 Risc0, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

/// \file
/// Defines the core finite field data type, Fp, and some free functions on the type.

#include "risc0/zkp/core/devs.h"

// Determine whether to use montgomery representation or direct representation.  Currently
// montgomry seems faster in most cases, might want to revisit with more testing
#define DIRECT 0

namespace risc0 {

/// The Fp class is an element of the finite field F_p, where P is the prime number 15*2^27 + 1.
/// Put another way, Fp is basically integer arithmetic modulo P.
///
/// The 'Fp' datatype is the core type of all of the operations done within the zero knowledge
/// proofs, and is smallest 'addressable' datatype, and the base type of which all composite types
/// are built.  In many ways, one can imagine it as the word size of a very strange architecture.
///
/// This specific prime P was chosen to:
/// - Be less than 2^31 so that it fits within a 32 bit word and doesn't overflow on addition.
/// - Otherwise have as large a power of 2 in the factors of P-1 as possible.
///
/// This last property is useful for number theoretical transforms (the fast fourier transform
/// equivelant on finite fields).  See NTT.h for details.
///
/// The Fp class wraps all the standard arithmatic operations to make the finite field elements look
/// basically like ordinary numbers (which they mostly are).
class Fp {
public:
  /// The value of P, the modulus of Fp.
  static CONSTSCALAR uint32_t P = 15 * (uint32_t(1) << 27) + 1;
  static CONSTSCALAR uint32_t M = 0x88000001;
  static CONSTSCALAR uint32_t R2 = 1172168163;

private:
  // The actual value, always < P.
  uint32_t val;

  // We make 'impls' of the core ops which all the other uses call.  This is done to allow for
  // tweaking of the implementation later, for example switching to montgomery representation or
  // doing inline assembly or some crazy CUDA stuff.

  // Add two numbers
  static DEVSPEC constexpr uint32_t add(uint32_t a, uint32_t b) {
    uint32_t r = a + b;
    return (r >= P ? r - P : r);
  }

  // Subtract two numbers
  static DEVSPEC constexpr uint32_t sub(uint32_t a, uint32_t b) {
    uint32_t r = a - b;
    return (r > P ? r + P : r);
  }

  // Multiply two numbers
  static DEVSPEC constexpr uint32_t mul(uint32_t a, uint32_t b) {
#if DIRECT
    return (uint64_t(a) * uint64_t(b)) % P;
#else
    uint64_t o64 = uint64_t(a) * uint64_t(b);
    uint32_t low = -uint32_t(o64);
    uint32_t red = M * low;
    o64 += uint64_t(red) * uint64_t(P);
    uint32_t ret = o64 >> 32;
    return (ret >= P ? ret - P : ret);
#endif
  }

  // Encode / Decode
  static DEVSPEC constexpr uint32_t encode(uint32_t a) {
#if DIRECT
    return a;
#else
    return mul(R2, a);
#endif
  }
  static DEVSPEC constexpr uint32_t decode(uint32_t a) {
#if DIRECT
    return a;
#else
    return mul(1, a);
#endif
  }

  // A private constructor that take the 'interal' form.
  DEVSPEC constexpr Fp(uint32_t val, bool ignore) : val(val) {}

public:
  /// Default constructor, sets value to 0.
  DEVSPEC constexpr Fp() : val(0) {}
  /// Construct an FP from a uint32_t, wrap if needed
  DEVSPEC constexpr Fp(uint32_t val) : val(encode(val)) {}

  /// Convert to a unit32_t
  DEVSPEC constexpr uint32_t asUInt32() const { return decode(val); }
#ifdef METAL
  constexpr uint32_t asUInt32() device const { return decode(val); }
#endif
  /// Get the largest value, basically P - 1.
  DEVSPEC static constexpr Fp maxVal() { return P - 1; }
  /// Get an 'invalid' Fp value
  DEVSPEC static constexpr Fp invalid() { return Fp(0xfffffffful, true); }
  /// Generate a uniform random value.
  template <typename Rng> static Fp random(DEVADDR Rng& rng) {
    // We use rejection sampling to make sure the results are truely uniform.  Basically, if we are
    // in the final uneven remainder of 2^64 / P, we just pull a new random number.  The propability
    // of such is case is less than 1 in 2^32, so this is a rare event.
    uint64_t val = uint64_t(rng.generate()) << 32 | rng.generate();
    while (val + P < val) { // If we wrap after adding P, we are in the final copy of P
      val = rng.generate(); // Try again.
    }
    return val % uint64_t(P);
  }

  // Implement all the various overloads
  DEVSPEC constexpr Fp operator+(Fp rhs) const { return Fp(add(val, rhs.val), true); }
  DEVSPEC constexpr Fp operator-() const { return Fp(sub(0, val), true); }
  DEVSPEC constexpr Fp operator-(Fp rhs) const { return Fp(sub(val, rhs.val), true); }
  DEVSPEC constexpr Fp operator*(Fp rhs) const { return Fp(mul(val, rhs.val), true); }
#ifdef METAL
  constexpr Fp operator+(Fp rhs) device const { return Fp(add(val, rhs.val), true); }
  constexpr Fp operator-() device const { return Fp(sub(0, val), true); }
  constexpr Fp operator-(Fp rhs) device const { return Fp(sub(val, rhs.val), true); }
  constexpr Fp operator*(Fp rhs) device const { return Fp(mul(val, rhs.val), true); }
#endif

  DEVSPEC constexpr Fp operator+=(Fp rhs) {
    val = add(val, rhs.val);
    return *this;
  }
  DEVSPEC constexpr Fp operator-=(Fp rhs) {
    val = sub(val, rhs.val);
    return *this;
  }
  DEVSPEC constexpr Fp operator*=(Fp rhs) {
    val = mul(val, rhs.val);
    return *this;
  }
  DEVSPEC constexpr bool operator==(Fp rhs) const { return val == rhs.val; }
  DEVSPEC constexpr bool operator!=(Fp rhs) const { return val != rhs.val; }
  DEVSPEC constexpr bool operator<(Fp rhs) const { return decode(val) < decode(rhs.val); }
  DEVSPEC constexpr bool operator<=(Fp rhs) const { return decode(val) <= decode(rhs.val); }
  DEVSPEC constexpr bool operator>(Fp rhs) const { return decode(val) > decode(rhs.val); }
  DEVSPEC constexpr bool operator>=(Fp rhs) const { return decode(val) >= decode(rhs.val); }
#ifdef METAL
  constexpr bool operator==(Fp rhs) device const { return val == rhs.val; }
  constexpr bool operator!=(Fp rhs) device const { return val != rhs.val; }
  constexpr bool operator<(Fp rhs) device const { return decode(val) < decode(rhs.val); }
  constexpr bool operator<=(Fp rhs) device const { return decode(val) <= decode(rhs.val); }
  constexpr bool operator>(Fp rhs) device const { return decode(val) > decode(rhs.val); }
  constexpr bool operator>=(Fp rhs) device const { return decode(val) >= decode(rhs.val); }
#endif

  // Post-inc/dec
  DEVSPEC constexpr Fp operator++(int) {
    Fp r = *this;
    val = add(val, encode(1));
    return r;
  }
  DEVSPEC constexpr Fp operator--(int) {
    Fp r = *this;
    val = sub(val, encode(1));
    return r;
  }

  // Pre-inc/dec
  DEVSPEC constexpr Fp operator++() {
    val = add(val, encode(1));
    return *this;
  }
  DEVSPEC constexpr Fp operator--() {
    val = sub(val, encode(1));
    return *this;
  }

#ifdef CPU
  std::string str() { return std::to_string(decode(val)); }
#endif
};

/// ostream support for Fp values, only for CPU
#ifdef CPU
inline std::ostream& operator<<(std::ostream& os, const Fp& x) {
  os << x.asUInt32();
  return os;
}
#endif

/// Raise an value to a power
DEVSPEC constexpr inline Fp pow(Fp x, size_t n) {
  Fp tot = 1;
  while (n != 0) {
    if (n % 2 == 1) {
      tot *= x;
    }
    n = n / 2;
    x *= x;
  }
  return tot;
}

/// Compute the multiplicative inverse of x, or `1/x` in finite field terms.  Since `x^(P-1) == 1
/// (mod P)` for any x != 0 (as a consequence of Fermat's little therorm), it follows that `x *
/// x^(P-2) == 1 (mod P)` for x != 0.  That is, `x^(P-2)` is the multiplicative inverse of x.
/// Computed this way, the 'inverse' of zero comes out as zero, which is convient in many cases, so
/// we leave it.
DEVSPEC constexpr inline Fp inv(Fp x) {
  return pow(x, Fp::P - 2);
}

} // namespace risc0
