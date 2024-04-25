#ifndef OCEANBASE_SUBQUERY_OB_PYTHON_UDF_OP_H_
#define OCEANBASE_SUBQUERY_OB_PYTHON_UDF_OP_H_

#include "sql/engine/ob_operator.h"
#include "sql/engine/subquery/ob_subplan_scan_op.h"
#include "sql/engine/expr/ob_expr_python_udf.h"
//#include <Python.h>

namespace oceanbase
{
namespace sql
{

// buffer for python_udf, based on VectorStore and BatchResultHolder
struct ObVectorBuffer
{
public:
  ObVectorBuffer() : exprs_(NULL), exec_ctx_(NULL), datums_(NULL),
                     max_size_(0), saved_size_(0), inited_(false)
  {}
  int init(const common::ObIArray<ObExpr *> &exprs, ObExecContext &exec_ctx, int64_t max_buffer_size);
  int save(ObEvalCtx &eval_ctx, ObBatchRows &brs_);
  bool is_saved() const { return saved_size_ > 0; }
  int get_size() const { return saved_size_; }
  int get_max_size() const { return max_size_; };
  int load(ObEvalCtx &eval_ctx, ObBatchRows &brs_, int64_t &brs_skip_size_, int64_t batch_size);
  int resize(int64_t size);
  int move(int64_t size); // move datums and cut max_size_
private:
  const common::ObIArray<ObExpr *> *exprs_;
  ObExecContext *exec_ctx_;
  ObDatum *datums_;
  int64_t max_size_;
  int64_t saved_size_;
  bool inited_;
};

class ObPythonUDFSpec : public ObSubPlanScanSpec
{
public:
  ObPythonUDFSpec(common::ObIAllocator &alloc, const ObPhyOperatorType type);

  ~ObPythonUDFSpec();
  
  //void* _save; //for Python Interpreter Thread State
  ExprFixedArray col_exprs_; //input
};

class ObPythonUDFOp : public ObSubPlanScanOp
{
public:
  ObPythonUDFOp(ObExecContext &exec_ctx, const ObOpSpec &spec, ObOpInput *input);

  ~ObPythonUDFOp();

  static int alloc_predict_buffer(ObIAllocator &alloc, ObExpr &expr, ObDatum *&buf_result, int buffer_size);

  static int find_predict_size(ObExpr *expr, int32_t &predict_size);

  virtual int inner_get_next_batch(const int64_t max_row_cnt) override;

  virtual int get_next_batch(const int64_t max_row_cnt, const ObBatchRows *&batch_rows) override;

  int clear_calc_exprs_evaluated_flags();

private:
  ExprFixedArray buf_exprs_; //all exprs with fake frames
  int64_t result_width_; //要进行拷贝的expr数
  int64_t brs_skip_size_;
  ObVectorBuffer input_buffer_;
  ObVectorBuffer output_buffer_;
  ObDatum **buf_results_; // for fake frame
  int predict_size_; //每次python udf计算的元组数
  bool use_input_buf_; 
  bool use_output_buf_;
  bool use_fake_frame_;
};

} // end namespace sql
} // end namespace oceanbase

#endif // OCEANBASE_SUBQUERY_OB_PYTHON_UDF_OP_H_