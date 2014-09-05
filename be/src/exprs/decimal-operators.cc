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

#include "exprs/decimal-operators.h"

#include <iomanip>
#include <sstream>
#include <math.h>

#include "exprs/case-expr.h"
#include "exprs/expr.h"
#include "exprs/expr-inline.h"
#include "runtime/tuple-row.h"
#include "util/decimal-util.h"
#include "util/string-parser.h"

using namespace std;

namespace impala {

#define RETURN_IF_OVERFLOW(e, o) \
  do {\
    if (UNLIKELY(o)) {\
      e->LogOverflow();\
      return NULL;\
    }\
  } while (false)

inline void* DecimalOperators::SetDecimalVal(Expr* e, int64_t val) {
  DCHECK_EQ(e->type().type, TYPE_DECIMAL);
  bool overflow = false;
  switch (e->type().GetByteSize()) {
    case 4:
      e->result_.decimal4_val = Decimal4Value::FromInt(e->type(), val, &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal4_val;
    case 8:
      e->result_.decimal8_val = Decimal8Value::FromInt(e->type(), val, &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal8_val;
    case 16:
      e->result_.decimal16_val = Decimal16Value::FromInt(e->type(), val, &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal16_val;
    default:
      DCHECK(false);
      return NULL;
  }
}

inline void* DecimalOperators::SetDecimalVal(Expr* e, double val) {
  DCHECK_EQ(e->type().type, TYPE_DECIMAL);
  bool overflow = false;
  switch (e->type().GetByteSize()) {
    case 4:
      e->result_.decimal4_val = Decimal4Value::FromDouble(e->type(), val, &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal4_val;
    case 8:
      e->result_.decimal8_val = Decimal8Value::FromDouble(e->type(), val, &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal8_val;
    case 16:
      e->result_.decimal16_val = Decimal16Value::FromDouble(e->type(), val, &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal16_val;
    default:
      DCHECK(false);
      return NULL;
  }
}

// Converting from one decimal type to another requires two steps.
// - Converting between the decimal types (e.g. decimal8 -> decimal16)
// - Adjusting the scale.
// When going from a larger type to a smaller type, we need to adjust the scales first
// (since it can reduce the magnitude of the value) to minimize cases where we overflow.

inline void* DecimalOperators::SetDecimalVal(Expr* e, const ColumnType& val_type,
    const Decimal4Value& val) {
  DCHECK_EQ(e->type().type, TYPE_DECIMAL);
  DCHECK_EQ(val_type.type, TYPE_DECIMAL);
  bool overflow = false;
  switch (e->type().GetByteSize()) {
    case 4:
      e->result_.decimal4_val = val.ScaleTo(val_type, e->type(), &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal4_val;
    case 8: {
      Decimal8Value val8 = Decimal4ToDecimal8(val, &overflow);
      e->result_.decimal8_val = val8.ScaleTo(val_type, e->type(), &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal8_val;
    }
    case 16: {
      Decimal16Value val16 = Decimal4ToDecimal16(val, &overflow);
      e->result_.decimal16_val = val16.ScaleTo(val_type, e->type(), &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal16_val;
    }
    default:
      DCHECK(false);
      return NULL;
  }
}

inline void* DecimalOperators::SetDecimalVal(Expr* e, const ColumnType& val_type,
    const Decimal8Value& val) {
  DCHECK_EQ(e->type().type, TYPE_DECIMAL);
  DCHECK_EQ(val_type.type, TYPE_DECIMAL);
  bool overflow = false;
  switch (e->type().GetByteSize()) {
    case 4: {
      Decimal8Value val8 = val.ScaleTo(val_type, e->type(), &overflow);
      e->result_.decimal4_val = Decimal8ToDecimal4(val8, &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal4_val;
    }
    case 8:
      e->result_.decimal8_val = val.ScaleTo(val_type, e->type(), &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal8_val;
    case 16: {
      Decimal16Value val16 = Decimal8ToDecimal16(val, &overflow);
      e->result_.decimal16_val = val16.ScaleTo(val_type, e->type(), &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal16_val;
    }
    default:
      DCHECK(false);
      return NULL;
  }
}

inline void* DecimalOperators::SetDecimalVal(Expr* e, const ColumnType& val_type,
    const Decimal16Value& val) {
  DCHECK_EQ(e->type().type, TYPE_DECIMAL);
  DCHECK_EQ(val_type.type, TYPE_DECIMAL);
  bool overflow = false;
  Decimal16Value val_scaled = val.ScaleTo(val_type, e->type(), &overflow);
  switch (e->type().GetByteSize()) {
    case 4: {
      e->result_.decimal4_val = Decimal16ToDecimal4(val_scaled, &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal4_val;
    }
    case 8: {
      e->result_.decimal8_val = Decimal16ToDecimal8(val_scaled, &overflow);
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal8_val;
    }
    case 16:
      e->result_.decimal16_val = val_scaled;
      RETURN_IF_OVERFLOW(e, overflow);
      return &e->result_.decimal16_val;
    default:
      DCHECK(false);
      return NULL;
  }
}

static inline Decimal4Value GetDecimal4Val(void* v, const ColumnType& type,
    bool* overflow) {
  DCHECK_EQ(type.type, TYPE_DECIMAL);
  switch (type.GetByteSize()) {
    case 4: return *reinterpret_cast<Decimal4Value*>(v);
    case 8: return Decimal8ToDecimal4(*reinterpret_cast<Decimal8Value*>(v), overflow);
    case 16: return Decimal16ToDecimal4(*reinterpret_cast<Decimal16Value*>(v), overflow);
    default:
      DCHECK(false);
      break;
  }
  return Decimal4Value();
}

static inline Decimal8Value GetDecimal8Val(void* v, const ColumnType& type,
    bool* overflow) {
  DCHECK_EQ(type.type, TYPE_DECIMAL);
  switch (type.GetByteSize()) {
    case 4: return Decimal4ToDecimal8(*reinterpret_cast<Decimal4Value*>(v), overflow);
    case 8: return *reinterpret_cast<Decimal8Value*>(v);
    case 16: return Decimal16ToDecimal8(*reinterpret_cast<Decimal16Value*>(v), overflow);
    default:
      DCHECK(false);
      break;
  }
  return Decimal8Value();
}

static inline Decimal16Value GetDecimal16Val(void* v, const ColumnType& type,
    bool* overflow) {
  DCHECK_EQ(type.type, TYPE_DECIMAL);
  switch (type.GetByteSize()) {
    case 4: return Decimal4ToDecimal16(*reinterpret_cast<Decimal4Value*>(v), overflow);
    case 8: return Decimal8ToDecimal16(*reinterpret_cast<Decimal8Value*>(v), overflow);
    case 16: return *reinterpret_cast<Decimal16Value*>(v);
    default:
      DCHECK(false);
      break;
  }
  return Decimal16Value();
}

#define CAST_INT_TO_DECIMAL(FN_NAME, TYPE) \
  void* DecimalOperators::FN_NAME(Expr* e, TupleRow* row) {\
    DCHECK_EQ(e->GetNumChildren(), 1);\
    void* v = e->GetChild(0)->GetValue(row);\
    if (v == NULL) return NULL;\
    int64_t val = *reinterpret_cast<TYPE*>(v);\
    return SetDecimalVal(e, val);\
  }

#define CAST_FLOAT_TO_DECIMAL(FN_NAME, TYPE) \
  void* DecimalOperators::FN_NAME(Expr* e, TupleRow* row) { \
    DCHECK_EQ(e->GetNumChildren(), 1);\
    void* v = e->GetChild(0)->GetValue(row);\
    if (v == NULL) return NULL;\
    return SetDecimalVal(e, *reinterpret_cast<TYPE*>(v));\
  }

#define CAST_DECIMAL_TO_INT(FN_NAME, TYPE, FIELD) \
  void* DecimalOperators::FN_NAME(Expr* e, TupleRow* row) {\
    DCHECK_EQ(e->GetNumChildren(), 1);\
    Expr* c = e->GetChild(0);\
    DCHECK_EQ(c->type().type, TYPE_DECIMAL);\
    void* v = c->GetValue(row);\
    if (v == NULL) return NULL;\
    TYPE result = 0;\
    switch (c->type().GetByteSize()) {\
      case 4:\
        result = static_cast<TYPE>(\
            reinterpret_cast<Decimal4Value*>(v)->whole_part(c->type()));\
        break;\
      case 8:\
        result = static_cast<TYPE>(\
            reinterpret_cast<Decimal8Value*>(v)->whole_part(c->type()));\
        break;\
      case 16:\
        result = static_cast<TYPE>(\
            reinterpret_cast<Decimal16Value*>(v)->whole_part(c->type()));\
        break;\
      default:\
        return NULL;\
    }\
    e->result_.FIELD = result;\
    return &e->result_.FIELD;\
  }

#define CAST_DECIMAL_TO_FLOAT(FN_NAME, TYPE, FIELD) \
  void* DecimalOperators::FN_NAME(Expr* e, TupleRow* row) {\
    Expr* c = e->GetChild(0);\
    DCHECK_EQ(c->type().type, TYPE_DECIMAL);\
    void* v = c->GetValue(row);\
    if (v == NULL) return NULL;\
    TYPE result = 0;\
    switch (c->type().GetByteSize()) {\
      case 4:\
        result = static_cast<TYPE>(\
            reinterpret_cast<Decimal4Value*>(v)->ToDouble(c->type()));\
        break;\
      case 8:\
        result = static_cast<TYPE>(\
            reinterpret_cast<Decimal8Value*>(v)->ToDouble(c->type()));\
        break;\
      case 16:\
        result = static_cast<TYPE>(\
            reinterpret_cast<Decimal16Value*>(v)->ToDouble(c->type()));\
        break;\
      default:\
        return NULL;\
    }\
    e->result_.FIELD = result;\
    return &e->result_.FIELD;\
  }

CAST_INT_TO_DECIMAL(Cast_char_decimal, int8_t)
CAST_INT_TO_DECIMAL(Cast_short_decimal, int16_t)
CAST_INT_TO_DECIMAL(Cast_int_decimal, int32_t)
CAST_INT_TO_DECIMAL(Cast_long_decimal, int64_t)
CAST_FLOAT_TO_DECIMAL(Cast_float_decimal, float)
CAST_FLOAT_TO_DECIMAL(Cast_double_decimal, double)

CAST_DECIMAL_TO_INT(Cast_decimal_char, int8_t, tinyint_val)
CAST_DECIMAL_TO_INT(Cast_decimal_short, int16_t, smallint_val)
CAST_DECIMAL_TO_INT(Cast_decimal_int, int32_t, int_val)
CAST_DECIMAL_TO_INT(Cast_decimal_long, int64_t, bigint_val)
CAST_DECIMAL_TO_FLOAT(Cast_decimal_float, float, float_val)
CAST_DECIMAL_TO_FLOAT(Cast_decimal_double, double, double_val)

void* DecimalOperators::RoundDecimalNegativeScale(Expr* e, TupleRow* row,
    const DecimalRoundOp& op, int rounding_scale) {
  DCHECK_GT(rounding_scale, 0);
  DCHECK_EQ(e->GetNumChildren(), 2);
  Expr* c = e->GetChild(0);
  void* v = c->GetValue(row);
  if (v == NULL) return NULL;

  // Switch on the parent type. 'result' holds the value prior to rounding.
  void* result = NULL;

  // Switch on the child type.
  switch (c->type().GetByteSize()) {
    case 4:
      result = SetDecimalVal(e, c->type(), *reinterpret_cast<Decimal4Value*>(v));
      break;
    case 8:
      result = SetDecimalVal(e, c->type(), *reinterpret_cast<Decimal8Value*>(v));
      break;
    case 16:
      result = SetDecimalVal(e, c->type(), *reinterpret_cast<Decimal16Value*>(v));
      break;
    default:
      DCHECK(false);
      return NULL;
  }

  // This can return NULL if the value overflowed.
  if (result == NULL) return result;

  // We've done the cast portion of the computation. Now round it.
  switch (e->type().GetByteSize()) {
    case 4: {
      Decimal4Value* r = reinterpret_cast<Decimal4Value*>(result);
      int32_t base = DecimalUtil::GetScaleMultiplier<int32_t>(rounding_scale);
      int32_t d = RoundDelta(*r, 0, -rounding_scale, op);
      r->value() -= (r->value() % base);
      r->value() += d * base;
      break;
    }
    case 8: {
      Decimal8Value* r = reinterpret_cast<Decimal8Value*>(result);
      int64_t base = DecimalUtil::GetScaleMultiplier<int64_t>(rounding_scale);
      int64_t d = RoundDelta(*r, 0, -rounding_scale, op);
      r->value() -= (r->value() % base);
      r->value() += d * base;
      break;
    }
    case 16: {
      Decimal16Value* r = reinterpret_cast<Decimal16Value*>(result);
      int128_t base = DecimalUtil::GetScaleMultiplier<int128_t>(rounding_scale);
      int128_t d = RoundDelta(*r, 0, -rounding_scale, op);
      int128_t delta = d * base - (r->value() % base);
      // Need to check for overflow. This can't happen in the other cases since the
      // FE should have picked a high enough precision.
      if (DecimalUtil::MAX_UNSCALED_DECIMAL - abs(delta) < abs(r->value())) {
        e->LogOverflow();
        return NULL;
      }
      r->value() += delta;
      break;
    }
    default:
      DCHECK(false);
      return NULL;
  }

  return result;
}

void* DecimalOperators::RoundDecimal(Expr* e, TupleRow* row,
    const DecimalRoundOp& op) {
  DCHECK_GE(e->GetNumChildren(), 1);
  Expr* c = e->GetChild(0);
  void* v = c->GetValue(row);
  if (v == NULL) return NULL;

  // Switch on the child type.
  void* result = NULL;
  int delta = 0;
  switch (c->type().GetByteSize()) {
    case 4:
      result = SetDecimalVal(e, c->type(), *reinterpret_cast<Decimal4Value*>(v));
      delta = RoundDelta(*reinterpret_cast<Decimal4Value*>(v),
          c->type().scale, e->type().scale, op);
      break;
    case 8:
      result = SetDecimalVal(e, c->type(), *reinterpret_cast<Decimal8Value*>(v));
      delta = RoundDelta(*reinterpret_cast<Decimal8Value*>(v),
          c->type().scale, e->type().scale, op);
      break;
    case 16:
      result = SetDecimalVal(e, c->type(), *reinterpret_cast<Decimal16Value*>(v));
      delta = RoundDelta(*reinterpret_cast<Decimal16Value*>(v),
          c->type().scale, e->type().scale, op);
      break;
    default:
      DCHECK(false);
      return NULL;
  }

  // This can return NULL if the value overflowed.
  if (result == NULL) return result;

  // At this point result is the first part of the round operation. It has just
  // done the cast.
  if (delta == 0) return result;

  // Switch on the parent type. The value in 'result' is before the rounding has
  // occurred.
  switch (e->type().GetByteSize()) {
    case 4:
      reinterpret_cast<Decimal4Value*>(result)->value() += delta;
      break;
    case 8:
      reinterpret_cast<Decimal8Value*>(result)->value() += delta;
      break;
    case 16:
      // This can't overflow. Rounding to a non-negative scale means at least one
      // digit is dropped if rounding occurred and the round can add at most one digit
      // before the decimal.
      reinterpret_cast<Decimal16Value*>(result)->value() += delta;
      break;
  }
  return result;
}

// Cast is just RoundDecimal(TRUNCATE).
// TODO: how we handle cast to a smaller scale is an implementation detail in the spec.
// We could also choose to cast by doing ROUND.
void* DecimalOperators::Cast_decimal_decimal(Expr* e, TupleRow* row) {
  return RoundDecimal(e, row, TRUNCATE);
}

void* DecimalOperators::Cast_StringValue_decimal(Expr* e, TupleRow* row) {
  DCHECK_EQ(e->GetNumChildren(), 1);
  void* v = e->GetChild(0)->GetValue(row);
  if (v == NULL) return NULL;
  StringValue* sv = reinterpret_cast<StringValue*>(v);
  StringParser::ParseResult result;
  void* ptr = NULL;
  DCHECK_EQ(e->type().type, TYPE_DECIMAL);
  switch (e->type().GetByteSize()) {
    case 4:
      e->result_.decimal4_val = StringParser::StringToDecimal<int32_t>(
          sv->ptr, sv->len, e->type(), &result);
      ptr = &e->result_.decimal4_val;
      break;
    case 8:
      e->result_.decimal8_val = StringParser::StringToDecimal<int64_t>(
          sv->ptr, sv->len, e->type(), &result);
      ptr = &e->result_.decimal8_val;
      break;
    case 16:
      e->result_.decimal16_val = StringParser::StringToDecimal<int128_t>(
          sv->ptr, sv->len, e->type(), &result);
      ptr = &e->result_.decimal16_val;
      break;
  }
  // Like all the cast functions, we return the truncated value on underflow and NULL
  // on overflow.
  // TODO: log warning on underflow.
  if (result == StringParser::PARSE_SUCCESS || result == StringParser::PARSE_UNDERFLOW) {
    return ptr;
  }
  return NULL;
}

void* DecimalOperators::Cast_decimal_StringValue(Expr* e, TupleRow* row) {
  DCHECK_EQ(e->GetNumChildren(), 1);
  Expr* c = e->GetChild(0);
  void* v = c->GetValue(row);
  if (v == NULL) return NULL;
  string result;
  DCHECK_EQ(c->type().type, TYPE_DECIMAL);
  switch (c->type().GetByteSize()) {
    case 4:
      result = reinterpret_cast<Decimal4Value*>(v)->ToString(c->type());
      break;
    case 8:
      result = reinterpret_cast<Decimal8Value*>(v)->ToString(c->type());
      break;
    case 16:
      result = reinterpret_cast<Decimal16Value*>(v)->ToString(c->type());
      break;
    default:
      return NULL;
  }
  e->result_.SetStringVal(result);
  return &e->result_.string_val;
}

void* DecimalOperators::Cast_decimal_TimestampValue(Expr* e, TupleRow* row) {
  DCHECK_EQ(e->GetNumChildren(), 1);
  Expr* c = e->GetChild(0);
  void* v = c->GetValue(row);
  if (v == NULL) return NULL;
  DCHECK_EQ(c->type().type, TYPE_DECIMAL);
  switch (c->type().GetByteSize()) {
    case 4:
      e->result_.timestamp_val =
          reinterpret_cast<Decimal4Value*>(v)->ToDouble(c->type());
      break;
    case 8:
      e->result_.timestamp_val =
          reinterpret_cast<Decimal8Value*>(v)->ToDouble(c->type());
      break;
    case 16:
      e->result_.timestamp_val =
          reinterpret_cast<Decimal16Value*>(v)->ToDouble(c->type());
      break;
    default:
      return NULL;
  }
  return &e->result_.timestamp_val;
}

void* DecimalOperators::Cast_decimal_bool(Expr* e, TupleRow* row) {
  DCHECK_EQ(e->GetNumChildren(), 1);
  Expr* c = e->GetChild(0);
  void* v = c->GetValue(row);
  if (v == NULL) return NULL;
  DCHECK_EQ(c->type().type, TYPE_DECIMAL);
  switch (c->type().GetByteSize()) {
    case 4:
      e->result_.bool_val = reinterpret_cast<Decimal4Value*>(v)->value() != 0;
      break;
    case 8:
      e->result_.bool_val = reinterpret_cast<Decimal8Value*>(v)->value() != 0;
      break;
    case 16:
      e->result_.bool_val = reinterpret_cast<Decimal16Value*>(v)->value() != 0;
      break;
    default:
      return NULL;
  }
  return &e->result_.bool_val;
}

#define DECIMAL_ARITHMETIC_OP(FN_NAME, OP_FN) \
  void* DecimalOperators::FN_NAME(Expr* e, TupleRow* row) {\
    DCHECK_EQ(e->GetNumChildren(), 2);\
    Expr* c1 = e->GetChild(0);\
    Expr* c2 = e->GetChild(1);\
    void* x = c1->GetValue(row);\
    void* y = c2->GetValue(row);\
    DCHECK_EQ(c1->type().type, TYPE_DECIMAL);\
    DCHECK_EQ(c2->type().type, TYPE_DECIMAL);\
    if (x == NULL || y == NULL) return NULL;\
    bool overflow = false;\
    switch (e->type().GetByteSize()) {\
      case 4: {\
        Decimal4Value x_val = GetDecimal4Val(x, c1->type(), &overflow);\
        Decimal4Value y_val = GetDecimal4Val(y, c2->type(), &overflow);\
        e->result_.decimal4_val = x_val.OP_FN<int32_t>(\
            c1->type(), y_val, c2->type(), e->type().scale, &overflow);\
        DCHECK(!overflow) << "Cannot overflow except with Decimal16Value";\
        return &e->result_.decimal4_val;\
      }\
      case 8: {\
        Decimal8Value x_val = GetDecimal8Val(x, c1->type(), &overflow);\
        Decimal8Value y_val = GetDecimal8Val(y, c2->type(), &overflow);\
        e->result_.decimal8_val = x_val.OP_FN<int64_t>(\
            c1->type(), y_val, c2->type(), e->type().scale, &overflow);\
        DCHECK(!overflow) << "Cannot overflow except with Decimal16Value";\
        return &e->result_.decimal8_val;\
      }\
      case 16: {\
        Decimal16Value x_val = GetDecimal16Val(x, c1->type(), &overflow);\
        Decimal16Value y_val = GetDecimal16Val(y, c2->type(), &overflow);\
        e->result_.decimal16_val = x_val.OP_FN<int128_t>(\
            c1->type(), y_val, c2->type(), e->type().scale, &overflow);\
        RETURN_IF_OVERFLOW(e, overflow);\
        return &e->result_.decimal16_val;\
      }\
      default:\
        break;\
    }\
    return NULL;\
  }

#define DECIMAL_ARITHMETIC_OP_CHECK_NAN(FN_NAME, OP_FN) \
  void* DecimalOperators::FN_NAME(Expr* e, TupleRow* row) {\
    DCHECK_EQ(e->GetNumChildren(), 2);\
    Expr* c1 = e->GetChild(0);\
    Expr* c2 = e->GetChild(1);\
    void* x = c1->GetValue(row);\
    void* y = c2->GetValue(row);\
    DCHECK_EQ(c1->type().type, TYPE_DECIMAL);\
    DCHECK_EQ(c2->type().type, TYPE_DECIMAL);\
    if (x == NULL || y == NULL) return NULL;\
    bool overflow = false;\
    bool is_nan = false;\
    switch (e->type().GetByteSize()) {\
      case 4: {\
        Decimal4Value x_val = GetDecimal4Val(x, c1->type(), &overflow);\
        Decimal4Value y_val = GetDecimal4Val(y, c2->type(), &overflow);\
        e->result_.decimal4_val = x_val.OP_FN<int32_t>(\
            c1->type(), y_val, c2->type(), e->type().scale, &is_nan, &overflow);\
        DCHECK(!overflow) << "Cannot overflow except with Decimal16Value";\
        if (is_nan) return NULL;\
        return &e->result_.decimal4_val;\
      }\
      case 8: {\
        Decimal8Value x_val = GetDecimal8Val(x, c1->type(), &overflow);\
        Decimal8Value y_val = GetDecimal8Val(y, c2->type(), &overflow);\
        e->result_.decimal8_val = x_val.OP_FN<int64_t>(\
            c1->type(), y_val, c2->type(), e->type().scale, &is_nan, &overflow);\
        DCHECK(!overflow) << "Cannot overflow except with Decimal16Value";\
        if (is_nan) return NULL;\
        return &e->result_.decimal8_val;\
      }\
      case 16: {\
        Decimal16Value x_val = GetDecimal16Val(x, c1->type(), &overflow);\
        Decimal16Value y_val = GetDecimal16Val(y, c2->type(), &overflow);\
        e->result_.decimal16_val = x_val.OP_FN<int128_t>(\
            c1->type(), y_val, c2->type(), e->type().scale, &is_nan, &overflow);\
        RETURN_IF_OVERFLOW(e, overflow);\
        if (is_nan) return NULL;\
        return &e->result_.decimal16_val;\
      }\
      default:\
        break;\
    }\
    return NULL;\
  }

#define DECIMAL_BINARY_OP(FN_NAME, OP_FN) \
  void* DecimalOperators::FN_NAME(Expr* e, TupleRow* row) {\
    DCHECK_EQ(e->GetNumChildren(), 2);\
    Expr* c1 = e->GetChild(0);\
    Expr* c2 = e->GetChild(1);\
    DCHECK_EQ(c1->type().type, TYPE_DECIMAL);\
    DCHECK_EQ(c2->type().type, TYPE_DECIMAL);\
    void* x = c1->GetValue(row);\
    void* y = c2->GetValue(row);\
    bool dummy = false;\
    if (x == NULL || y == NULL) return NULL;\
    int byte_size = ::max(c1->type().GetByteSize(), c2->type().GetByteSize());\
    switch (byte_size) {\
      case 4: {\
        Decimal4Value x_val = GetDecimal4Val(x, c1->type(), &dummy);\
        Decimal4Value y_val = GetDecimal4Val(y, c2->type(), &dummy);\
        e->result_.bool_val = x_val.OP_FN(c1->type(), y_val, c2->type());\
        return &e->result_.bool_val;\
      }\
      case 8: {\
        Decimal8Value x_val = GetDecimal8Val(x, c1->type(), &dummy);\
        Decimal8Value y_val = GetDecimal8Val(y, c2->type(), &dummy);\
        e->result_.bool_val = x_val.OP_FN(c1->type(), y_val, c2->type());\
        return &e->result_.bool_val;\
      }\
      case 16: {\
        Decimal16Value x_val = GetDecimal16Val(x, c1->type(), &dummy);\
        Decimal16Value y_val = GetDecimal16Val(y, c2->type(), &dummy);\
        e->result_.bool_val = x_val.OP_FN(c1->type(), y_val, c2->type());\
        return &e->result_.bool_val;\
      }\
      default:\
        break;\
    }\
    return NULL;\
  }

DECIMAL_ARITHMETIC_OP(Add_decimal_decimal, Add)
DECIMAL_ARITHMETIC_OP(Subtract_decimal_decimal, Subtract)
DECIMAL_ARITHMETIC_OP(Multiply_decimal_decimal, Multiply)
DECIMAL_ARITHMETIC_OP_CHECK_NAN(Divide_decimal_decimal, Divide)
DECIMAL_ARITHMETIC_OP_CHECK_NAN(Mod_decimal_decimal, Mod)

DECIMAL_BINARY_OP(Eq_decimal_decimal, Eq)
DECIMAL_BINARY_OP(Ne_decimal_decimal, Ne)
DECIMAL_BINARY_OP(Ge_decimal_decimal, Ge)
DECIMAL_BINARY_OP(Gt_decimal_decimal, Gt)
DECIMAL_BINARY_OP(Le_decimal_decimal, Le)
DECIMAL_BINARY_OP(Lt_decimal_decimal, Lt)

void* DecimalOperators::Case_decimal(Expr* e, TupleRow* row) {
  CaseExpr* expr = static_cast<CaseExpr*>(e);
  int num_children = e->GetNumChildren();
  int loop_end = (expr->has_else_expr()) ? num_children - 1 : num_children;
  // Make sure we set the right compute function.
  DCHECK_EQ(expr->has_case_expr(), true);
  // Need at least case, when and then expr, and optionally an else.
  DCHECK_GE(num_children, (expr->has_else_expr()) ? 4 : 3);
  // All case and when exprs return the same type (we guaranteed that during analysis).
  void* case_val = e->children()[0]->GetValue(row);
  int num_bytes = e->children()[0]->type().GetByteSize();
  if (case_val == NULL) {
    if (expr->has_else_expr()) {
      // Return else value.
      return e->children()[num_children - 1]->GetValue(row);
    } else {
      return NULL;
    }
  }
  for (int i = 1; i < loop_end; i += 2) {
    void* when_val = e->children()[i]->GetValue(row);
    if (when_val == NULL) continue;
    switch (num_bytes) {
      case 4:
        if (*reinterpret_cast<Decimal4Value*>(when_val) ==
            *reinterpret_cast<Decimal4Value*>(case_val)) {
          return e->children()[i + 1]->GetValue(row);
        }
        break;
      case 8:
        if (*reinterpret_cast<Decimal8Value*>(when_val) ==
            *reinterpret_cast<Decimal8Value*>(case_val)) {
          return e->children()[i + 1]->GetValue(row);
        }
        break;
      case 16:
        if (*reinterpret_cast<Decimal16Value*>(when_val) ==
            *reinterpret_cast<Decimal16Value*>(case_val)) {
          return e->children()[i + 1]->GetValue(row);
        }
        break;
      default:
        DCHECK(false) << e->children()[0]->type();
        return NULL;
    }
  }
  if (expr->has_else_expr()) {
    // Return else value.
    return e->children()[num_children - 1]->GetValue(row);
  }
  return NULL;
}


}

