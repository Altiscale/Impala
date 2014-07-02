// Copyright 2012 Cloudera Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef IMPALA_RUNTIME_MULTI_PRECISION_H
#define IMPALA_RUNTIME_MULTI_PRECISION_H

// The boost library is for C++11 on a newer version of boost than we use.
// We need to make these #defines to compile for pre c++11
#define BOOST_NOEXCEPT
#define BOOST_NOEXCEPT_IF(Predicate)
#define BOOST_FORCEINLINE inline __attribute__ ((__always_inline__))

#define BOOST_NO_CXX11_CONSTEXPR
#define BOOST_NO_CXX11_DECLTYPE
#define BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS
#define BOOST_NO_CXX11_HDR_ARRAY
#define BOOST_NO_CXX11_RVALUE_REFERENCES
#define BOOST_NO_CXX11_USER_DEFINED_LITERALS
#define BOOST_NO_CXX11_VARIADIC_TEMPLATES

// Finally include the boost library.
#include "boost_multiprecision/cpp_int.hpp"
#include "boost_multiprecision/cpp_dec_float.hpp"

namespace impala {

// We use the c++ int128_t type. This is stored using 16 bytes and very performant.
typedef __int128_t int128_t;

// Define 256 bit int type.
typedef boost::multiprecision::number<
    boost::multiprecision::cpp_int_backend<256, 256,
    boost::multiprecision::signed_magnitude,
    boost::multiprecision::unchecked, void> > int256_t;

// There is no implicit assignment from int128_t to int256_t (or in general, the boost
// multi precision types and __int128_t).
// TODO: look into the perf of this. I think the boost library is very slow with bitwise
// ops but reasonably fast with arithmetic ops so different implementations of this
// could have big perf differences.
inline int256_t ConvertToInt256(const int128_t& x) {
  if (x < 0) {
    uint64_t hi = static_cast<uint64_t>(-x >> 64);
    uint64_t lo = static_cast<uint64_t>(-x);
    int256_t v = hi;
    v <<= 64;
    v |= lo;
    return -v;
  } else {
    uint64_t hi = static_cast<uint64_t>(x >> 64);
    uint64_t lo = static_cast<uint64_t>(x);
    int256_t v = hi;
    v <<= 64;
    v |= lo;
    return v;
  }
}

// Prints v in base 10.
std::ostream& operator<<(std::ostream& os, const int128_t& val);

}

#endif
