/**
 * author: watch2bear
 * pull up python udf filter into condition
 * add extra subplan scan
 */

#ifndef OB_TRANSFORM_PULLUP_FILTER_H
#define OB_TRANSFORM_PULLUP_FILTER_H

#include "sql/rewrite/ob_transform_rule.h"
#include "sql/resolver/dml/ob_select_stmt.h"
#include "sql/ob_select_stmt_printer.h"
namespace oceanbase
{
namespace sql
{

class ObTransformPullUpFilter : public ObTransformRule
{
public:
  ObTransformPullUpFilter(ObTransformerCtx *ctx);

  virtual ~ObTransformPullUpFilter();

  virtual int transform_one_stmt(
    ObIArray<ObParentDMLStmt> &parent_stmts,
    ObDMLStmt *&stmt,
    bool &trans_happened) override;

  virtual int generate_child_level_stmt(
    ObSelectStmt *&select_stmt,
    ObSelectStmt *&sub_stmt);

  virtual int generate_parent_level_stmt(
    ObSelectStmt *&select_stmt,
    ObSelectStmt *sub_stmt);

  static int construct_column_items_from_exprs(
    const ObIArray<ObRawExpr*> &column_exprs,
    ObIArray<ColumnItem> &column_items);

  int check_hint_allowed(const ObDMLStmt &stmt,
                         bool &allowed);

private:
  virtual int need_transform(
    const common::ObIArray<ObParentDMLStmt> &parent_stmts,
    const int64_t current_level,
    const ObDMLStmt &stmt,
    bool &need_trans) override;
  
  virtual int construct_transform_hint(ObDMLStmt &stmt, void *trans_params) override;


private:
  ObArenaAllocator allocator_;
};

}
}

#endif // OB_TRANSFORM_PULLUP_FILTER_H