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

package com.cloudera.impala.planner;

import java.util.List;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.cloudera.impala.analysis.Analyzer;
import com.cloudera.impala.analysis.Expr;
import com.cloudera.impala.analysis.SlotDescriptor;
import com.cloudera.impala.analysis.SortInfo;
import com.cloudera.impala.common.InternalException;
import com.cloudera.impala.service.BackendConfig;
import com.cloudera.impala.thrift.TExplainLevel;
import com.cloudera.impala.thrift.TPlanNode;
import com.cloudera.impala.thrift.TPlanNodeType;
import com.cloudera.impala.thrift.TQueryOptions;
import com.cloudera.impala.thrift.TSortInfo;
import com.cloudera.impala.thrift.TSortNode;
import com.google.common.base.Joiner;
import com.google.common.base.Objects;
import com.google.common.base.Preconditions;
import com.google.common.collect.Lists;

/**
 * Node that implements a sort with or without a limit. useTopN_ is true for sorts
 * with limits that are implemented by a TopNNode in the backend. SortNode is used
 * otherwise.
 * Will always materialize the new tuple info_.sortTupleDesc_.
 */
public class SortNode extends PlanNode {
  private final static Logger LOG = LoggerFactory.getLogger(SortNode.class);

  // The sort information produced in analysis.
  private final SortInfo info_;
  // info_.sortTupleSlotExprs_ substituted with the baseTblSmap_ for materialized slots
  // in init().
  List<Expr> baseTblMaterializedTupleExprs_;
  private final boolean useTopN_;
  // The offset of the first row to return.
  protected long offset_;

  public SortNode(PlanNodeId id, PlanNode input, SortInfo info, boolean useTopN,
      long offset) {
    super(id, Lists.newArrayList(info.getSortTupleDescriptor().getId()),
        getDisplayName(useTopN, false));
    info_ = info;
    useTopN_ = useTopN;
    children_.add(input);
    offset_ = offset;
  }

  public long getOffset() { return offset_; }
  public void setOffset(long offset) { offset_ = offset; }
  public boolean hasOffset() { return offset_ > 0; }

  public boolean useTopN() { return useTopN_; }

  public SortInfo getSortInfo() { return info_; }

  @Override
  public void setCompactData(boolean on) { compactData_ = on; }

  @Override
  public boolean isBlockingNode() { return true; }

  @Override
  public void init(Analyzer analyzer) throws InternalException {
    assignConjuncts(analyzer);
    // Compute the memory layout for the generated tuple.
    computeMemLayout(analyzer);
    computeStats(analyzer);
    createDefaultSmap();
    List<SlotDescriptor> sortTupleSlots = info_.getSortTupleDescriptor().getSlots();
    List<Expr> slotExprs = info_.getSortTupleSlotExprs();
    Preconditions.checkState(sortTupleSlots.size() == slotExprs.size());
    baseTblMaterializedTupleExprs_ = Lists.newArrayList();
    for (int i = 0; i < slotExprs.size(); ++i) {
      if (sortTupleSlots.get(i).isMaterialized()) {
        baseTblMaterializedTupleExprs_.add(slotExprs.get(i).clone(baseTblSmap_));
      }
    }
  }

  @Override
  protected void computeStats(Analyzer analyzer) {
    super.computeStats(analyzer);
    cardinality_ = capAtLimit(getChild(0).cardinality_);
    LOG.debug("stats Sort: cardinality=" + Long.toString(cardinality_));
  }

  @Override
  protected String debugString() {
    List<String> strings = Lists.newArrayList();
    for (Boolean isAsc : info_.getIsAscOrder()) {
      strings.add(isAsc ? "a" : "d");
    }
    return Objects.toStringHelper(this)
        .add("ordering_exprs", Expr.debugString(info_.getOrderingExprs()))
        .add("is_asc", "[" + Joiner.on(" ").join(strings) + "]")
        .add("nulls_first", "[" + Joiner.on(" ").join(info_.getNullsFirst()) + "]")
        .add("offset_", offset_)
        .addValue(super.debugString())
        .toString();
  }

  @Override
  protected void toThrift(TPlanNode msg) {
    msg.node_type = TPlanNodeType.SORT_NODE;
    TSortInfo sort_info = new TSortInfo(Expr.treesToThrift(info_.getOrderingExprs()),
        info_.getIsAscOrder(), info_.getNullsFirst());
    Preconditions.checkState(tupleIds_.size() == 1,
        "Incorrect size for tupleIds_ in SortNode");
    sort_info.sort_tuple_slot_exprs = Expr.treesToThrift(baseTblMaterializedTupleExprs_);
    TSortNode sort_node = new TSortNode(sort_info, useTopN_);
    sort_node.setOffset(offset_);
    msg.sort_node = sort_node;
  }

  @Override
  protected String getNodeExplainString(String prefix, String detailPrefix,
      TExplainLevel detailLevel) {
    StringBuilder output = new StringBuilder();
    output.append(String.format("%s%s:%s%s\n", prefix, id_.toString(),
        displayName_, getNodeExplainDetail(detailLevel)));
    if (detailLevel.ordinal() >= TExplainLevel.STANDARD.ordinal()) {
      output.append(detailPrefix + "order by: ");
      for (int i = 0; i < info_.getOrderingExprs().size(); ++i) {
        if (i > 0) output.append(", ");
        output.append(info_.getOrderingExprs().get(i).toSql() + " ");
        output.append(info_.getIsAscOrder().get(i) ? "ASC" : "DESC");

        Boolean nullsFirstParam = info_.getNullsFirstParams().get(i);
        if (nullsFirstParam != null) {
          output.append(nullsFirstParam ? " NULLS FIRST" : " NULLS LAST");
        }
      }
      output.append("\n");
    }
    return output.toString();
  }

  private String getNodeExplainDetail(TExplainLevel detailLevel) {
    if (!hasLimit()) return "";
    if (hasOffset()) {
      return String.format(" [LIMIT=%s OFFSET=%s]", limit_, offset_);
    } else {
      return String.format(" [LIMIT=%s]", limit_);
    }
  }

  @Override
  protected String getOffsetExplainString(String prefix) {
    return offset_ != 0 ? prefix + "offset: " + Long.toString(offset_) + "\n" : "";
  }

  @Override
  public void computeCosts(TQueryOptions queryOptions) {
    Preconditions.checkState(hasValidStats());
    if (useTopN_) {
      perHostMemCost_ = (long) Math.ceil((cardinality_ + offset_) * avgRowSize_);
      return;
    }

    // For an external sort, set the memory cost to be what is required for a 2-phase
    // sort. If the input to be sorted would take up N blocks in memory, then the
    // memory required for a 2-phase sort is sqrt(N) blocks. A single run would be of
    // size sqrt(N) blocks, and we could merge sqrt(N) such runs with sqrt(N) blocks
    // of memory.
    double fullInputSize = getChild(0).cardinality_ * avgRowSize_;
    boolean hasVarLenSlots = false;
    for (SlotDescriptor slotDesc: info_.getSortTupleDescriptor().getSlots()) {
      if (slotDesc.isMaterialized() && !slotDesc.getType().isFixedLengthType()) {
        hasVarLenSlots = true;
        break;
      }
    }

    // The block size used by the sorter is the same as the configured I/O read size.
    long blockSize = BackendConfig.INSTANCE.getReadSize();
    // The external sorter writes fixed-len and var-len data in separate sequences of
    // blocks on disk and reads from both sequences when merging. This effectively
    // doubles the block size when there are var-len columns present.
    if (hasVarLenSlots) blockSize *= 2;
    double numInputBlocks = Math.ceil(fullInputSize / blockSize);
    perHostMemCost_ = blockSize * (long) Math.ceil(Math.sqrt(numInputBlocks));
  }

  private static String getDisplayName(boolean isTopN, boolean isMergeOnly) {
    if (isTopN) {
      return "TOP-N";
    } else {
      return "SORT";
    }
  }
}
