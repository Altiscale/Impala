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

#include "exprs/anyval-util.h"

#include "common/object-pool.h"

using namespace std;
using namespace impala_udf;

namespace impala {

AnyVal* CreateAnyVal(ObjectPool* pool, const ColumnType& type) {
  switch(type.type) {
    case TYPE_NULL: return pool->Add(new AnyVal);
    case TYPE_BOOLEAN: return pool->Add(new BooleanVal);
    case TYPE_TINYINT: return pool->Add(new TinyIntVal);
    case TYPE_SMALLINT: return pool->Add(new SmallIntVal);
    case TYPE_INT: return pool->Add(new IntVal);
    case TYPE_BIGINT: return pool->Add(new BigIntVal);
    case TYPE_FLOAT: return pool->Add(new FloatVal);
    case TYPE_DOUBLE: return pool->Add(new DoubleVal);
    case TYPE_STRING: return pool->Add(new StringVal);
    case TYPE_TIMESTAMP: return pool->Add(new TimestampVal);
    case TYPE_DECIMAL: return pool->Add(new DecimalVal);
    default:
      DCHECK(false) << "Unsupported type: " << type;
      return NULL;
  }
}

void AnyValUtil::ColumnTypeToTypeDesc(
    const ColumnType& type, FunctionContext::TypeDesc* out) {
  switch (type.type) {
    case TYPE_BOOLEAN:
      out->type = FunctionContext::TYPE_BOOLEAN;
      break;
    case TYPE_TINYINT:
      out->type = FunctionContext::TYPE_TINYINT;
      break;
    case TYPE_SMALLINT:
      out->type = FunctionContext::TYPE_SMALLINT;
      break;
    case TYPE_INT:
      out->type = FunctionContext::TYPE_INT;
      break;
    case TYPE_BIGINT:
      out->type = FunctionContext::TYPE_BIGINT;
      break;
    case TYPE_FLOAT:
      out->type = FunctionContext::TYPE_FLOAT;
      break;
    case TYPE_DOUBLE:
      out->type = FunctionContext::TYPE_DOUBLE;
      break;
    case TYPE_TIMESTAMP:
      out->type = FunctionContext::TYPE_TIMESTAMP;
      break;
    case TYPE_STRING:
      out->type = FunctionContext::TYPE_STRING;
      break;
    case TYPE_CHAR:
      out->type = FunctionContext::TYPE_FIXED_BUFFER;
      break;
    case TYPE_DECIMAL:
      out->type = FunctionContext::TYPE_DECIMAL;
      out->precision = type.precision;
      out->scale = type.scale;
      break;
    default:
      DCHECK(false) << "Unknown type: " << type;
  }
}

}
