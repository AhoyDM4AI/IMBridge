/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#define USING_LOG_PREFIX SQL_OPT

#include "sql/optimizer/ob_opt_est_cost_model_vector.h"

using namespace oceanbase;
using namespace sql;


/**
 * 估算TableScan的代价
 * formula: cost     = io_cost + memtable_cost + memtable_merge_cost + cpu_cost
 *          io_cost  = MICRO_BLOCK_SEQ_COST * num_micro_blocks
 *                     + PROJECT_COLUMN_SEQ_COST * num_column * num_rows
 *          cpu_cost = qual_cost + num_rows * CPU_TUPLE_COST
 */
int ObOptEstVectorCostModel::cost_table_scan_one_batch_inner(double row_count,
                                                            const ObCostTableScanInfo &est_cost_info,
                                                            bool is_scan_index,
                                                            double &cost)
{
  int ret = OB_SUCCESS;
  double project_cost = 0.0;
  const ObIndexMetaInfo &index_meta_info = est_cost_info.index_meta_info_;
  const ObTableMetaInfo *table_meta_info = est_cost_info.table_meta_info_;
  bool is_index_back = index_meta_info.is_index_back_;

  if (OB_ISNULL(table_meta_info) ||
      OB_UNLIKELY(row_count < 0)) {
    ret = OB_INVALID_ARGUMENT;
    LOG_WARN("invalid args", K(row_count), K(ret));
  } else if ((!is_scan_index || !is_index_back) &&
             OB_FAIL(cost_full_table_scan_project(row_count,
                                                  est_cost_info,
                                                  project_cost))) {
    LOG_WARN("failed to cost project", K(ret));
  } else if (is_scan_index &&
             is_index_back &&
             OB_FAIL(cost_project(row_count,
                                  est_cost_info.index_access_column_items_,
                                  true,
                                  project_cost))) {
    LOG_WARN("failed to cost project", K(ret));
  } else {
    //索引总的微块数 = 总大小/微块大小
    double num_micro_blocks = index_meta_info.get_micro_block_numbers();
    //读微块数 = 总微块数 * 读行比例
    double num_micro_blocks_read = 0;
    if (OB_LIKELY(table_meta_info->table_row_count_ > 0) &&
        row_count <= table_meta_info->table_row_count_) {
      num_micro_blocks_read = std::ceil(num_micro_blocks
                                        * row_count
                                        / static_cast<double> (table_meta_info->table_row_count_));
    } else {
      num_micro_blocks_read = num_micro_blocks;
    }

    // revise number of rows if is row sample scan
    // 对于行采样，除了微块扫描数外，其他按比例缩小
    if (est_cost_info.sample_info_.is_row_sample()) {
      row_count *= 0.01 * est_cost_info.sample_info_.percent_;
    }

    // IO代价，主要包括读取微块、反序列化的代价的代价
    double io_cost = 0.0;
    double first_block_cost = cost_params_.MICRO_BLOCK_RND_COST;
    double rows_in_one_block = static_cast<double> (table_meta_info->table_row_count_) / num_micro_blocks;
    rows_in_one_block = rows_in_one_block <= 1 ? 1.000001 : rows_in_one_block;
    if (!est_cost_info.pushdown_prefix_filters_.empty()) {
      if (est_cost_info.can_use_batch_nlj_) {
        first_block_cost = cost_params_.BATCH_NL_SCAN_COST;
      } else {
        first_block_cost = cost_params_.NL_SCAN_COST;
      }
    }
    if (num_micro_blocks_read < 1) {
      io_cost = first_block_cost;
    } else {
      io_cost = first_block_cost + cost_params_.MICRO_BLOCK_SEQ_COST * (num_micro_blocks_read-1);
    }

    // 谓词代价，主要指filter的代价
    double qual_cost = 0.0;
    if (!is_index_back) {
      // 主表扫描
      ObSEArray<ObRawExpr*, 8> filters;
      if (OB_FAIL(append(filters, est_cost_info.postfix_filters_)) ||
          OB_FAIL(append(filters, est_cost_info.table_filters_))) {
        LOG_WARN("failed to append fiilters", K(ret));
      } else {
        qual_cost += cost_quals(row_count, filters);
      }
    } else {
      // 索引扫描
      qual_cost += cost_quals(row_count, est_cost_info.postfix_filters_);
    }
    // CPU代价，包括get_next_row调用的代价和谓词代价
    double cpu_cost = row_count * cost_params_.CPU_TUPLE_COST
                      + qual_cost;
    // 从memtable读取数据的代价，待提供
    double memtable_cost = 0;
    // memtable数据和基线数据合并的代价，待提供
    double memtable_merge_cost = 0;
    //因为存储层有预期，所以去存储层的IO、CPU代价的最大值
    double scan_cpu_cost = row_count * cost_params_.TABLE_SCAN_CPU_TUPLE_COST + project_cost;
    cpu_cost += scan_cpu_cost;
    cost = io_cost + cpu_cost + memtable_cost + memtable_merge_cost;

    LOG_TRACE("OPT:[COST TABLE SCAN INNER]", K(num_micro_blocks), K(table_meta_info->table_row_count_),
              K(cost), K(io_cost), K(cpu_cost), K(memtable_cost), K(memtable_merge_cost), K(qual_cost),
              K(project_cost), K(num_micro_blocks_read), K(row_count));
  }
  return ret;
}
