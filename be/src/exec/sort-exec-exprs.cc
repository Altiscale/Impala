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

#include "exec/sort-exec-exprs.h"

using namespace std;

namespace impala {

Status SortExecExprs::Init(const TSortInfo& sort_info, ObjectPool* pool) {
  RETURN_IF_ERROR(
      Expr::CreateExprTrees(pool, sort_info.ordering_exprs, &lhs_ordering_exprs_));
  RETURN_IF_ERROR(
      Expr::CreateExprTrees(pool, sort_info.ordering_exprs, &rhs_ordering_exprs_));

  if (sort_info.__isset.sort_tuple_slot_exprs) {
    materialize_tuple_ = true;
    RETURN_IF_ERROR(Expr::CreateExprTrees(pool, sort_info.sort_tuple_slot_exprs,
        &sort_tuple_slot_exprs_));
  } else {
    materialize_tuple_ = false;
  }
  return Status::OK;
}

Status SortExecExprs::Prepare(RuntimeState* state, const RowDescriptor& child_row_desc,
    const RowDescriptor& output_row_desc) {
  if (materialize_tuple_) {
    RETURN_IF_ERROR(
        Expr::Prepare(sort_tuple_slot_exprs_, state, child_row_desc));
  }
  RETURN_IF_ERROR(Expr::Prepare(lhs_ordering_exprs_, state, output_row_desc));
  RETURN_IF_ERROR(Expr::Prepare(rhs_ordering_exprs_, state, output_row_desc));
  return Status::OK;
}

Status SortExecExprs::Open(RuntimeState* state) {
  if (materialize_tuple_) {
    RETURN_IF_ERROR(Expr::Open(sort_tuple_slot_exprs_, state));
  }
  RETURN_IF_ERROR(Expr::Open(lhs_ordering_exprs_, state));
  RETURN_IF_ERROR(Expr::Open(rhs_ordering_exprs_, state));
  return Status::OK;
}

void SortExecExprs::Close(RuntimeState* state) {
  if (materialize_tuple_) {
    Expr::Close(sort_tuple_slot_exprs_, state);
  }
  Expr::Close(lhs_ordering_exprs_, state);
  Expr::Close(rhs_ordering_exprs_, state);
}

} //namespace impala
