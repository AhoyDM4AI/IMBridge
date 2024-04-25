/**
 * author: watch2bear
 */

#define USING_LOG_PREFIX SQL_REWRITE
#include "sql/rewrite/ob_transform_pullup_filter.h"
#include "sql/rewrite/ob_stmt_comparer.h"
#include "sql/rewrite/ob_transform_utils.h"
#include "sql/optimizer/ob_optimizer_util.h"
#include "sql/resolver/expr/ob_raw_expr_util.h"
#include "sql/rewrite/ob_predicate_deduce.h"
#include "share/schema/ob_table_schema.h"
#include "common/ob_smart_call.h"

#include "objit/include/objit/expr/ob_iraw_expr.h"
#include "sql/resolver/expr/ob_raw_expr.h"

#include "sql/ob_select_stmt_printer.h"
#include "deps/oblib/src/lib/json/ob_json_print_utils.h"

using namespace oceanbase::sql;
using namespace oceanbase::common;

ObTransformPullUpFilter::ObTransformPullUpFilter(ObTransformerCtx *ctx)
    : ObTransformRule(ctx, TransMethod::POST_ORDER, T_PULL_UP_FILTER),
      allocator_("PullUpFilter")
{}

ObTransformPullUpFilter::~ObTransformPullUpFilter()
{}

//create subplan and pull up filters
int ObTransformPullUpFilter::transform_one_stmt(
  common::ObIArray<ObParentDMLStmt> &parent_stmts, ObDMLStmt *&stmt, bool &trans_happened)
{
  int ret = OB_SUCCESS;
  trans_happened = false;
  LOG_TRACE("Run transform pull up filter ObTransformPullUpFilter", K(ret));

  ObSelectStmt *select_stmt = NULL;
  ObSelectStmt *sub_stmt = NULL;
  ObSEArray<ObRawExpr *, 4> target_exprs;
  bool allowed = false;

  if (OB_ISNULL(stmt) || OB_ISNULL(ctx_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("stmt is null", K(ret), K(stmt), K(ctx_));
  } else if (!stmt->is_select_stmt()) {
    // do nothing
    OPT_TRACE("not select stmt, can not transform");
  } else if (OB_FAIL(check_hint_allowed(*stmt, allowed))) {
    // 检查query hint异常
    LOG_WARN("failed to check hint", K(ret));
  } else if (!allowed) {
    // do nothing
    OPT_TRACE("hint reject transform");
  } else if (FALSE_IT(select_stmt = static_cast<ObSelectStmt*>(stmt))) {
    //准备进行改写
    LOG_WARN("select stmt is NULL", K(ret));
  } else if (OB_FAIL(generate_child_level_stmt(select_stmt, 
                                               sub_stmt))) {
    LOG_WARN("failed to generate child level sub stmt", K(ret));
  } else if (OB_FAIL(generate_parent_level_stmt(select_stmt, 
                                                sub_stmt))) { 
    LOG_WARN("failed to generate parent level select stmt", K(ret));
  } else if (OB_FAIL(select_stmt->formalize_stmt(ctx_->session_info_))) {
    LOG_WARN("failed to formalize stmt.", K(ret));
  } else {
    trans_happened = true;
    stmt = select_stmt;
  }
  
  //printer
  char buffer[OB_MAX_SQL_LENGTH];
  memset(buffer, '\0', OB_MAX_SQL_LENGTH);
  int64_t pos = 0;
  ObSelectStmtPrinter stmt_printer(buffer, OB_MAX_SQL_LENGTH, &pos, select_stmt, ctx_->schema_checker_->get_schema_guard(), ObObjPrintParams());
  stmt_printer.do_print();
  buffer[pos] = '\0';
  LOG_WARN("Do transform ObTransformPullUpFilter", "select_stmt", buffer, K(ret));
  return ret;
}

int ObTransformPullUpFilter::generate_child_level_stmt(
    ObSelectStmt *&select_stmt,
    ObSelectStmt *&sub_stmt)
{
  int ret = OB_SUCCESS;
  ObSEArray<ObRawExpr *, 4> python_udf_exprs; // for remove
  ObSEArray<ObRawExpr *, 4> select_exprs;
  //deep copy stmt as subplan
  if (OB_ISNULL(ctx_) || OB_ISNULL(ctx_->stmt_factory_) || OB_ISNULL(ctx_->expr_factory_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("select stmt is null.", K(ret));
  } else if (OB_FAIL(ctx_->stmt_factory_->create_stmt<ObSelectStmt>(sub_stmt))) {
    LOG_WARN("failed to create stmt.", K(ret));
  } else if (FALSE_IT(sub_stmt->set_query_ctx(select_stmt->get_query_ctx()))) {
    LOG_WARN("failed to set query ctx.", K(ret));
  } else if (OB_FAIL(sub_stmt->deep_copy(*ctx_->stmt_factory_,
                                         *ctx_->expr_factory_,
                                         *select_stmt))) {
    LOG_WARN("failed to deep copy from nested stmt.", K(ret));
  } else if (OB_FAIL(sub_stmt->adjust_statement_id(ctx_->allocator_,
                                                   ctx_->src_qb_name_,
                                                   ctx_->src_hash_val_))) {
    LOG_WARN("failed to adjust statement id", K(ret));
  } else if (OB_FAIL(ObTransformUtils::extract_python_udf_exprs(sub_stmt->get_condition_exprs(), python_udf_exprs))) {
    LOG_WARN("failed to remove python udf condition exprs.", K(ret));
  } else if (OB_FAIL(sub_stmt->get_select_exprs(select_exprs))) {
    LOG_WARN("failed to get select exprs of child stmt.", K(ret));
  } else if (OB_FAIL(ObTransformUtils::extract_python_udf_exprs(select_exprs, python_udf_exprs))) {
    LOG_WARN("failed to remove python udf projection exprs.", K(ret));
  } else {
    // remove select items
    sub_stmt->get_select_items().reset();
    // remove limit exprs
    sub_stmt->set_limit_offset(NULL, NULL);
    // keep group-by exprs
    ObRawExpr *limit_percent_expr = sub_stmt->get_limit_percent_expr();
    if(OB_NOT_NULL(limit_percent_expr))
      limit_percent_expr->reset();
  }
  //add columnItem exprs into selectItem
  for (int64_t j = 0; OB_SUCC(ret) && j < sub_stmt->get_column_size(); ++j) {
    if (OB_FAIL(ObTransformUtils::create_select_item(*ctx_->allocator_,
                                                     sub_stmt->get_column_item(j)->get_expr(),
                                                     sub_stmt))) {
      LOG_WARN("failed to push back into select item array.", K(ret));
    } else { /*do nothing.*/ }
  }
  // add aggr items into selectItem
  for (int64_t j = 0; OB_SUCC(ret) && j < sub_stmt->get_aggr_item_size(); ++j) {
    if (OB_FAIL(ObTransformUtils::create_select_item(*ctx_->allocator_,
                                                     sub_stmt->get_aggr_item(j),
                                                     sub_stmt))) {
      LOG_WARN("failed to push back into select item array.", K(ret));
    }
  }
  
  return ret;
}

int ObTransformPullUpFilter::generate_parent_level_stmt(ObSelectStmt *&select_stmt,
                                                        ObSelectStmt *sub_stmt)
{
  int ret = OB_SUCCESS;
  TableItem *view_table_item = NULL;
  ObSEArray<ObRawExpr *, 4> old_exprs;
  ObSEArray<ObRawExpr *, 4> new_exprs;
  ObSEArray<ObRawExpr *, 4> condition_exprs;
  ObSEArray<ColumnItem, 4> column_items;
  
  if (OB_ISNULL(select_stmt)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("select stmt is null", K(ret));
  } else if (OB_FAIL(select_stmt->get_column_exprs(old_exprs))) {
    LOG_WARN("old exprs is null", K(ret));
  //} else if (extract_old_exprs(old_exprs, select_stmt)) {
  } else if (append(old_exprs, select_stmt->get_aggr_items())) {
    LOG_WARN("failed to extract aggr items into old exprs is null", K(ret));
  } else {
    // clear select stmt, keep conditions and projections 
    select_stmt->get_table_items().reset();
    select_stmt->get_joined_tables().reset();
    select_stmt->get_from_items().reset();
    select_stmt->get_having_exprs().reset();
    select_stmt->get_order_items().reset();
    select_stmt->get_group_exprs().reset(); // remove group-by exprs & aggr exprs
    select_stmt->get_aggr_items().reset();
    select_stmt->get_rollup_exprs().reset();
    select_stmt->get_column_items().reset();
    select_stmt->get_part_exprs().reset();
    select_stmt->get_check_constraint_items().reset();
    select_stmt->get_grouping_sets_items().reset();
    select_stmt->get_multi_rollup_items().reset();
    if (OB_FAIL(ObTransformUtils::add_new_table_item(ctx_,
                                                     select_stmt,
                                                     sub_stmt,
                                                     view_table_item))) {
      LOG_WARN("failed to add new table item.", K(ret));
    } else if (OB_FAIL(select_stmt->add_from_item(view_table_item->table_id_))) {
      LOG_WARN("failed to add from item", K(ret));
    } else if (OB_FAIL(ObTransformUtils::create_columns_for_view(ctx_,
                                                                 *view_table_item,
                                                                 select_stmt,
                                                                 new_exprs))) {
      LOG_WARN("failed to get select exprs from grouping sets view.", K(ret));
    } else if (OB_FAIL(construct_column_items_from_exprs(new_exprs, column_items))) {
      LOG_WARN("failed to construct column items from exprs.", K(ret));
    } else if (OB_FAIL(select_stmt->replace_relation_exprs(old_exprs, new_exprs))) {
      LOG_WARN("failed to replace inner stmt exprs.", K(ret));
    } else if (OB_FAIL(select_stmt->get_column_items().assign(column_items))) {
      LOG_WARN("failed to assign column items.", K(ret));
    } else if (OB_FAIL(select_stmt->rebuild_tables_hash())) {
      LOG_WARN("failed to rebuild tables hash.", K(ret));
    } else if (OB_FAIL(select_stmt->update_column_item_rel_id())) {
      LOG_WARN("failed to update column items rel id.", K(ret));
    } else if (OB_FAIL(select_stmt->formalize_stmt(ctx_->session_info_))) {
      LOG_WARN("failed to formalized stmt.", K(ret));
    } else if (OB_FAIL(ObTransformUtils::extract_python_udf_exprs(select_stmt->get_condition_exprs(), condition_exprs))) {
      LOG_WARN("failed to extract python udf filters.", K(ret));
    } else {
      // reset parent stmt conditons
      select_stmt->get_condition_exprs().reset();
      append(select_stmt->get_condition_exprs(), condition_exprs);
    }
  }
  return ret;
}

int ObTransformPullUpFilter::construct_column_items_from_exprs(
  const ObIArray<ObRawExpr*> &column_exprs,
  ObIArray<ColumnItem> &column_items)
{
  int ret = OB_SUCCESS;
  for (int64_t i = 0; OB_SUCC(ret) && i < column_exprs.count(); ++i) {
    ColumnItem column_item;
    ObColumnRefRawExpr* expr = static_cast<ObColumnRefRawExpr*>(column_exprs.at(i));
    column_item.expr_ = expr;
    column_item.table_id_ = expr->get_table_id();
    column_item.column_id_ = expr->get_column_id();
    column_item.column_name_ = expr->get_expr_name();
    if (OB_FAIL(column_items.push_back(column_item))) {
      LOG_WARN("failed to push back into temp column items.", K(ret));
    } else { /*do nothing.*/ }
  }
  return ret;
}

int ObTransformPullUpFilter::need_transform(const common::ObIArray<ObParentDMLStmt> &parent_stmts,
  const int64_t current_level,
  const ObDMLStmt &stmt,
  bool &need_trans)
{
  int ret = OB_SUCCESS;
  LOG_TRACE("Check need transform of ObTransformPullUpFilter.", K(ret));
  need_trans = false;
  ObSEArray<ObSelectStmt*, 16> child_stmts;
  ObSEArray<ObRawExpr *, 4> select_exprs;
  if (!stmt.is_select_stmt()) {
    // do nothing
    OPT_TRACE("not select stmt, can not transform");
  /*} else if (OB_FAIL(stmt.get_child_stmts(child_stmts))) {
    LOG_WARN("get child stmt failed.", K(ret));
  } else if (!child_stmts.empty()) {
    LOG_WARN("exist child stmts.", K(ret));*/
  } else if (OB_FAIL(static_cast<const ObSelectStmt &>(stmt).get_select_exprs(select_exprs))) {
    LOG_WARN("get select exprs failed.", K(ret));
  } else if (select_exprs.empty()) {
    LOG_WARN("get none select exprs.", K(ret));
  } else {
    // check stmt condition exprs
    for(int32_t i = 0; i < stmt.get_condition_size(); i++) {
      if(ObTransformUtils::expr_contain_type(const_cast<ObRawExpr *>(stmt.get_condition_expr(i)), T_FUN_SYS_PYTHON_UDF)) {
        need_trans = true;
        LOG_WARN("python udf in condition exprs.", K(ret));
        break;
      }
    }
    // check stmt projection exprs
    for(int32_t i = 0; i < select_exprs.count(); i++) {
      if(ObTransformUtils::expr_contain_type(select_exprs.at(i), T_FUN_SYS_PYTHON_UDF)) {
        need_trans = true;
        LOG_WARN("python udf in select exprs.", K(ret));
        break;
      }
    }
  }
  return ret;
}

int ObTransformPullUpFilter::check_hint_allowed(const ObDMLStmt &stmt,
                                                bool &allowed)
{
  int ret = OB_SUCCESS;
  allowed = true;
  const ObQueryHint *query_hint = NULL;
  const ObHint *myhint = get_hint(stmt.get_stmt_hint());
  bool is_disable = (NULL != myhint) && myhint->is_disable_hint();
  const ObHint *no_rewrite = stmt.get_stmt_hint().get_no_rewrite_hint();
  if (OB_ISNULL(ctx_) || OB_ISNULL(query_hint = stmt.get_stmt_hint().query_hint_)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("unexpected null", K(ret), K(ctx_), K(query_hint));
  } else if (query_hint->has_outline_data()) {
    // outline data allowed unnest
    allowed = query_hint->is_valid_outline_transform(ctx_->trans_list_loc_, myhint);
  } else if (NULL != myhint && myhint->is_enable_hint()) {
    allowed = true;
  } else if (is_disable || NULL != no_rewrite) {
    allowed = false;
    if (OB_FAIL(ctx_->add_used_trans_hint(no_rewrite))) {
      LOG_WARN("failed to add used trans hint", K(ret));
    } else if (is_disable && OB_FAIL(ctx_->add_used_trans_hint(myhint))) {
      LOG_WARN("failed to add used trans hint", K(ret));
    }
  }
  allowed = true;
  return ret;
}

int ObTransformPullUpFilter::construct_transform_hint(ObDMLStmt &stmt, void *trans_params) 
{
  int ret = OB_SUCCESS;
  ObIArray<ObSelectStmt*> *transed_stmts = static_cast<ObIArray<ObSelectStmt*>*>(trans_params);
  if (OB_ISNULL(ctx_) || OB_ISNULL(ctx_->allocator_) || OB_ISNULL(transed_stmts)) {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("unexpected null", K(ret), K(ctx_));
  } else if (OB_FAIL(ctx_->add_src_hash_val(ctx_->src_qb_name_))) {
    LOG_WARN("failed to add src hash val", K(ret));
  } else {
    ObTransHint *hint = NULL;
    ObString qb_name;
    ObSelectStmt *cur_stmt = NULL;
    for (int64_t i = 0; OB_SUCC(ret) && i < transed_stmts->count(); i++) {
      if (OB_ISNULL(cur_stmt = transed_stmts->at(i))) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("transed_stmt is null", K(ret), K(i));
      } else if (OB_FAIL(ctx_->add_used_trans_hint(get_hint(cur_stmt->get_stmt_hint())))) {
        LOG_WARN("failed to add used hint", K(ret));
      } else if (OB_FAIL(ObQueryHint::create_hint(ctx_->allocator_, get_hint_type(), hint))) {
        LOG_WARN("failed to create hint", K(ret));
      } else if (OB_FAIL(ctx_->outline_trans_hints_.push_back(hint))) {
        LOG_WARN("failed to push back hint", K(ret));
      } else if (OB_FAIL(cur_stmt->get_qb_name(qb_name))) {
        LOG_WARN("failed to get qb name", K(ret));
      } else if (OB_FAIL(cur_stmt->adjust_qb_name(ctx_->allocator_,
                                                  qb_name,
                                                  ctx_->src_hash_val_))) {
        LOG_WARN("adjust stmt id failed", K(ret));
      } else {
        hint->set_qb_name(qb_name);
      }
    }
  }
  return ret;
}