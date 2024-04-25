#define USING_LOG_PREFIX SQL_OPT
#include "sql/optimizer/ob_log_python_udf.h"
#include "sql/optimizer/ob_log_plan.h"
#include "sql/optimizer/ob_opt_est_cost.h"
#include "sql/optimizer/ob_join_order.h"
#include "common/ob_smart_call.h"
using namespace oceanbase::sql;
using namespace oceanbase::common;

int ObLogPythonUDF::get_plan_item_info(PlanText &plan_text,
                                       ObSqlPlanItem &plan_item)
{
  int ret = OB_SUCCESS;
  // print subplan operator info
  if (OB_FAIL(ObLogSubPlanScan::get_plan_item_info(plan_text, plan_item))) {
    LOG_WARN("failed to get SubplanScan Logical Operator item info", K(ret));
  } else if (metas.empty()) {
    // collect python udf metadata
    const ObIArray<ObRawExpr *> &filter = get_filter_exprs();
    const ObIArray<ObRawExpr*> &output = get_output_exprs();
    FOREACH_CNT_X(e, filter, OB_SUCC(ret)) 
      OZ(collect_expr_metadata((*e), metas));
    FOREACH_CNT_X(e, output, OB_SUCC(ret)) 
      OZ(collect_expr_metadata((*e), metas)); 
  }
  // print python udf metadata
  int64_t N = -1;
  BEGIN_BUF_PRINT;
  if (OB_FAIL(ret)) { /* Do nothing */
  } else if (OB_FAIL(BUF_PRINTF("("))) { /* Do nothing */
  } else if (FALSE_IT(N = metas.count())) { /* Do nothing */
  } else if (N == 0) { /* Do nothing */
  } else {
    for (int64_t i = 0; OB_SUCC(ret) && i < N; ++i) {
      ObPythonUDFMeta meta = metas.at(i);
      if(OB_FAIL(BUF_PRINTF("["))) { /* Do nothing */
      } else if (OB_FAIL(BUF_PRINTF("name: "))) { 
      } else if (OB_FAIL(BUF_PRINTF("%.*s", meta.name_.length(), meta.name_.ptr()))) { 
      //} else if (OB_FAIL(BUF_PRINTF(", ret: "))) { 
      //} else if (OB_FAIL(BUF_PRINTF(PyUdfRetType_to_string(meta.ret_)))) { 
      } else if (OB_FAIL(BUF_PRINTF(", pycall: "))) { 
      } else if (OB_FAIL(BUF_PRINTF("%.*s", meta.pycall_.length(), meta.pycall_.ptr()))) {
      } else if (OB_FAIL(BUF_PRINTF("]"))) { 
      } else if (i < N - 1) {                                                               
        ret = BUF_PRINTF(", ");    
      } else { /* Do nothing */ }
    }
  }
  if (OB_SUCC(ret)) { /* Do nothing */                                                      
    ret = BUF_PRINTF(")"); 
  } else { /* Do nothing */ }

  END_BUF_PRINT(plan_item.pyudf_metadata_,
                plan_item.pyudf_metadata_len_);
  return ret;
}

int ObLogPythonUDF::collect_expr_metadata(
    ObRawExpr *expr,
    ObIArray<ObPythonUDFMeta> &metas)
{
  int ret = OB_SUCCESS;
  if(expr->get_expr_type() == T_FUN_SYS_PYTHON_UDF) {
    OZ(metas.push_back(static_cast<ObPythonUdfRawExpr *>(expr)->get_udf_meta()));
  }
  for (int32_t i = 0; OB_SUCC(ret) && i < expr->get_param_count(); i++) {
    OZ(collect_expr_metadata(expr->get_param_expr(i), metas));
  }
  return ret;
}
