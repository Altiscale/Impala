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

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <limits>
#include <gtest/gtest.h>
#include <boost/cstdint.hpp>
#include <boost/lexical_cast.hpp>
#include "runtime/decimal-value.h"
#include "util/string-parser.h"

using namespace std;
using namespace boost;

namespace impala {

// Compare decimal result against double.
static const double MAX_ERROR = 0.0001;


template <typename T>
void VerifyEquals(const DecimalValue<T>& t1, const DecimalValue<T>& t2) {
  if (t1 != t2) {
    LOG(ERROR) << t1 << " != " << t2;
    EXPECT_TRUE(false);
  }
}

template <typename T>
void VerifyParse(const string& s, const ColumnType& t,
    const DecimalValue<T>& expected_val, StringParser::ParseResult expected_result) {
  StringParser::ParseResult parse_result;
  DecimalValue<T> val = StringParser::StringToDecimal<T>(
      s.c_str(), s.size(), t, &parse_result);
  EXPECT_EQ(parse_result, expected_result);
  if (expected_result == StringParser::PARSE_SUCCESS) {
    VerifyEquals(expected_val, val);
  }
}

template<typename T>
void VerifyToString(const T& decimal, const ColumnType& t, const string& expected) {
  EXPECT_EQ(decimal.ToString(t), expected);
}

void StringToAllDecimals(const string& s, const ColumnType& t, int32_t val,
    StringParser::ParseResult result) {
  VerifyParse(s, t, Decimal4Value(val), result);
  VerifyParse(s, t, Decimal8Value(val), result);
  VerifyParse(s, t, Decimal16Value(val), result);
}

TEST(StringToDecimal, Basic) {
  ColumnType t1 = ColumnType::CreateDecimalType(10, 0);
  ColumnType t2 = ColumnType::CreateDecimalType(10, 2);
  ColumnType t3 = ColumnType::CreateDecimalType(2, 0);
  ColumnType t4 = ColumnType::CreateDecimalType(10, 5);

  StringToAllDecimals("1234", t1, 1234, StringParser::PARSE_SUCCESS);
  StringToAllDecimals("1234", t2, 123400, StringParser::PARSE_SUCCESS);
  StringToAllDecimals("-1234", t2, -123400, StringParser::PARSE_SUCCESS);
  StringToAllDecimals("123", t3, 123, StringParser::PARSE_OVERFLOW);
  StringToAllDecimals("  12  ", t3, 12, StringParser::PARSE_SUCCESS);
  StringToAllDecimals("000", t3, 0, StringParser::PARSE_SUCCESS);
  StringToAllDecimals("00012.3", t2, 1230, StringParser::PARSE_SUCCESS);
  StringToAllDecimals("-00012.3", t2, -1230, StringParser::PARSE_SUCCESS);

  StringToAllDecimals("123.45", t2, 12345, StringParser::PARSE_SUCCESS);
  StringToAllDecimals(".45", t2, 45, StringParser::PARSE_SUCCESS);
  StringToAllDecimals("-.45", t2, -45, StringParser::PARSE_SUCCESS);
  StringToAllDecimals(" 123.4 ", t4, 12340000, StringParser::PARSE_SUCCESS);
  StringToAllDecimals("-123.45", t4, -12345000, StringParser::PARSE_SUCCESS);
  StringToAllDecimals("-123.456", t2, -123456, StringParser::PARSE_UNDERFLOW);
}

TEST(StringToDecimal, LargeDecimals) {
  ColumnType t1 = ColumnType::CreateDecimalType(1, 0);
  ColumnType t2 = ColumnType::CreateDecimalType(10, 10);
  ColumnType t3 = ColumnType::CreateDecimalType(8, 3);

  StringToAllDecimals("1", t1, 1, StringParser::PARSE_SUCCESS);
  StringToAllDecimals("-1", t1, -1, StringParser::PARSE_SUCCESS);
  StringToAllDecimals(".1", t1, -1, StringParser::PARSE_UNDERFLOW);
  StringToAllDecimals("10", t1, 10, StringParser::PARSE_OVERFLOW);
  StringToAllDecimals("-10", t1, -10, StringParser::PARSE_OVERFLOW);

  VerifyParse(".1234567890", t2,
      Decimal8Value(1234567890L), StringParser::PARSE_SUCCESS);
  VerifyParse("-.1234567890", t2,
      Decimal8Value(-1234567890L), StringParser::PARSE_SUCCESS);
  VerifyParse(".12345678900", t2,
      Decimal8Value(12345678900L), StringParser::PARSE_UNDERFLOW);
  VerifyParse("-.12345678900", t2,
      Decimal8Value(-12345678900L), StringParser::PARSE_UNDERFLOW);
  VerifyParse(".1234567890", t2,
      Decimal16Value(1234567890L), StringParser::PARSE_SUCCESS);
  VerifyParse("-.1234567890", t2,
      Decimal16Value(-1234567890L), StringParser::PARSE_SUCCESS);
  VerifyParse(".12345678900", t2,
      Decimal16Value(12345678900L), StringParser::PARSE_UNDERFLOW);
  VerifyParse("-.12345678900", t2,
      Decimal16Value(-12345678900L), StringParser::PARSE_UNDERFLOW);

  // Up to 8 digits with 5 before the decimal and 3 after.
  VerifyParse("12345.678", t3,
      Decimal8Value(12345678L), StringParser::PARSE_SUCCESS);
  VerifyParse("-12345.678", t3,
      Decimal8Value(-12345678L), StringParser::PARSE_SUCCESS);
  VerifyParse("123456.78", t3,
      Decimal8Value(12345678L), StringParser::PARSE_OVERFLOW);
  VerifyParse("1234.5678", t3,
      Decimal8Value(12345678L), StringParser::PARSE_UNDERFLOW);
  VerifyParse("12345.678", t3,
      Decimal16Value(12345678L), StringParser::PARSE_SUCCESS);
  VerifyParse("-12345.678", t3,
      Decimal16Value(-12345678L), StringParser::PARSE_SUCCESS);
  VerifyParse("123456.78", t3,
      Decimal16Value(12345678L), StringParser::PARSE_OVERFLOW);
  VerifyParse("1234.5678", t3,
      Decimal16Value(12345678L), StringParser::PARSE_UNDERFLOW);
}

enum Op {
  ADD,
  SUBTRACT,
  MULTIPLY,
  DIVIDE,
  MOD,
};

// Implementation of decimal rules. This is handled in the planner in the normal
// execution paths.
ColumnType GetResultType(const ColumnType& t1, const ColumnType& t2, Op op) {
  switch (op) {
    case ADD:
    case SUBTRACT:
      return ColumnType::CreateDecimalType(
          max(t1.scale, t2.scale) +
              max(t1.precision - t1.scale, t2.precision - t2.scale) + 1,
          max(t1.scale, t2.scale));
    case MULTIPLY:
      return ColumnType::CreateDecimalType(
          t1.precision + t2.precision + 1, t1.scale + t2.scale);
    case DIVIDE:
      return ColumnType::CreateDecimalType(
          t1.precision - t1.scale + t2.scale + max(4, t1.scale + t2.precision + 1),
          max(4, t1.scale + t2.precision + 1));
    case MOD:
      return ColumnType::CreateDecimalType(
          min(t1.precision - t1.scale, t2.precision - t2.scale) + max(t1.scale, t2.scale),
          max(t1.scale, t2.scale));
    default:
      DCHECK(false);
      return ColumnType();
  }
}

template<typename T>
void VerifyFuzzyEquals(const T& actual, const ColumnType& t, double expected) {
  double actual_d = actual.ToDouble(t);
  EXPECT_TRUE(fabs(actual_d - expected) < MAX_ERROR)
    << actual_d << " != " << expected;
}

TEST(DecimalArithmetic, Basic) {
  ColumnType t1 = ColumnType::CreateDecimalType(5, 4);
  ColumnType t2 = ColumnType::CreateDecimalType(8, 3);
  ColumnType t1_plus_2 = GetResultType(t1, t2, ADD);
  ColumnType t1_times_2 = GetResultType(t1, t2, MULTIPLY);

  Decimal4Value d1(123456789);
  Decimal4Value d2(23456);
  Decimal4Value d3(-23456);
  double d1_double = d1.ToDouble(t1);
  double d2_double = d2.ToDouble(t2);
  double d3_double = d3.ToDouble(t2);

  // TODO: what's the best way to author a bunch of tests like this?
  VerifyFuzzyEquals(d1.Add<int64_t>(t1, d2, t2, t1_plus_2.scale),
      t1_plus_2, d1_double + d2_double);
  VerifyFuzzyEquals(d1.Add<int64_t>(t1, d3, t2, t1_plus_2.scale),
      t1_plus_2, d1_double + d3_double);
  VerifyFuzzyEquals(d1.Subtract<int64_t>(
      t1, d2, t2, t1_plus_2.scale), t1_plus_2, d1_double - d2_double);
  VerifyFuzzyEquals(d1.Subtract<int64_t>(
      t1, d3, t2, t1_plus_2.scale), t1_plus_2, d1_double - d3_double);
  VerifyFuzzyEquals(d1.Multiply<int128_t>(
      t1, d2, t2, t1_times_2.scale), t1_times_2, d1_double * d2_double);
  VerifyFuzzyEquals(d1.Multiply<int64_t>(
      t1, d3, t2, t1_times_2.scale), t1_times_2, d1_double * d3_double);
}

TEST(DecimalArithmetic, Divide) {
  // Exhaustively test precision and scale for 4 byte decimals. The logic errors tend
  // to be by powers of 10 so not testing the other decimal types is okay.
  Decimal4Value x(123456789);
  Decimal4Value y(234);
  for (int numerator_p = 1; numerator_p <= 9; ++numerator_p) {
    for (int numerator_s = 0; numerator_s <= numerator_p; ++numerator_s) {
      for (int denominator_p = 1; denominator_p <= 3; ++denominator_p) {
        for (int denominator_s = 0; denominator_s <= denominator_p; ++denominator_s) {
          ColumnType t1 = ColumnType::CreateDecimalType(numerator_p, numerator_s);
          ColumnType t2 = ColumnType::CreateDecimalType(denominator_p, denominator_s);
          ColumnType t3 = GetResultType(t1, t2, DIVIDE);
          bool is_nan;
          Decimal8Value r = x.Divide<int64_t>(t1, y, t2, t3.scale, &is_nan);
          double approx_x = x.ToDouble(t1);
          double approx_y = y.ToDouble(t2);
          double approx_r = r.ToDouble(t3);
          double expected_r = approx_x / approx_y;

          EXPECT_TRUE(!is_nan);
          if (fabs(approx_r - expected_r) > MAX_ERROR) {
            LOG(ERROR) << approx_r << " " << expected_r;
            LOG(ERROR) << x.ToString(t1) << "/" << y.ToString(t2)
                       << "=" << r.ToString(t3);
            EXPECT_TRUE(false);
          }
        }
      }
    }
  }

  // Divide by 0
  bool is_nan;
  Decimal8Value r = x.Divide<int64_t>(ColumnType::CreateDecimalType(10, 0),
      Decimal4Value(0), ColumnType::CreateDecimalType(2,0), 4, &is_nan);
  EXPECT_TRUE(is_nan) << "Expected NaN, got: " << r;
}

template<typename T>
DecimalValue<T> RandDecimal(int max_precision) {
  T val = 0;
  int precision = rand() % max_precision;
  for (int i = 0; i < precision; ++i) {
    int digit = rand() % 10;
    val = val * 10 + digit;
  }
  return DecimalValue<T>(rand() % 2 == 0 ? val : -val);
}

int DoubleCompare(double x, double y) {
  if (x < y) return -1;
  if (x > y) return 1;
  return 0;
}

// Randomly test decimal operations, comparing the result with a double ground truth.
TEST(DecimalArithmetic, RandTesting) {
  int NUM_ITERS = 1000000;
  int seed = time(0);
  LOG(ERROR) << "Seed: " << seed;
  for (int i = 0; i < NUM_ITERS; ++i) {
    // TODO: double is too imprecise so we can't test with high scales.
    int p1 = rand() % 12 + 1;
    int s1 = rand() % min(4, p1);
    int p2 = rand() % 12 + 1;
    int s2 = rand() % min(4, p2);

    DecimalValue<int64_t> dec1 = RandDecimal<int64_t>(p1);
    DecimalValue<int64_t> dec2 = RandDecimal<int64_t>(p2);
    ColumnType t1 = ColumnType::CreateDecimalType(p1, s1);
    ColumnType t2 = ColumnType::CreateDecimalType(p2, s2);
    double t1_d = dec1.ToDouble(t1);
    double t2_d = dec2.ToDouble(t2);

    ColumnType add_t = GetResultType(t1, t2, ADD);

    VerifyFuzzyEquals(dec1.Add<int64_t>(
        t1, dec2, t2, add_t.scale), add_t, t1_d + t2_d);
    VerifyFuzzyEquals(dec1.Subtract<int64_t>(
        t1, dec2, t2, add_t.scale), add_t, t1_d - t2_d);
#if 0
    TODO: doubles are not precise enough for this
    ColumnType multiply_t = GetResultType(t1, t2, MULTIPLY);
    ColumnType divide_t = GetResultType(t1, t2, DIVIDE);
    // double is too imprecise to generate the right result.
    // TODO: compare against the ground truth using the multi precision float library.
    VerifyFuzzyEquals(dec1.Multiply<int64_t>(
        t1, dec2, t2, multiply_t.scale), multiply_t, t1_d * t2_d);
    if (dec2.value() != 0) {
      VerifyFuzzyEquals(dec1.Divide<int64_t>(
          t1, dec2, t2, divide_t.scale), divide_t, t1_d / t2_d);
    }
#endif

    EXPECT_EQ(dec1.Compare(t1, dec2, t2), DoubleCompare(t1_d, t2_d));
    EXPECT_TRUE(dec1.Compare(t1, dec1, t1) == 0);
    EXPECT_TRUE(dec2.Compare(t2, dec2, t2) == 0);
  }
}

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
