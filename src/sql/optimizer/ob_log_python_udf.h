#ifndef _OB_LOG_PYTHON_UDF_H_
#define _OB_LOG_PYTHON_UDF_H_
#include "sql/optimizer/ob_logical_operator.h"
#include "sql/optimizer/ob_log_subplan_scan.h"
#include "share/schema/ob_python_udf.h"

namespace oceanbase
{
namespace sql
{
class ObLogPythonUDF : public ObLogSubPlanScan
{
public:
  ObLogPythonUDF(ObLogPlan &plan)
      : ObLogSubPlanScan(plan)
  {}

  ~ObLogPythonUDF() {};

  virtual int get_plan_item_info(PlanText &plan_text,
                                ObSqlPlanItem &plan_item) override;
                                
  virtual int collect_expr_metadata(ObRawExpr *expr,
                                    ObIArray<ObPythonUDFMeta> &metas);
  
private:
  ObSEArray<ObPythonUDFMeta, 4> metas;
  DISALLOW_COPY_AND_ASSIGN(ObLogPythonUDF);
};

} // end of namespace sql
} // end of namespace oceanbase


#endif /* OB_LOG_SUBQUERY_SCAN_H_ */
