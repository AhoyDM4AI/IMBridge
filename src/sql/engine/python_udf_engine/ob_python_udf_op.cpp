#define USING_LOG_PREFIX SQL_ENG

#include "ob_python_udf_op.h"
#include "sql/engine/ob_physical_plan.h"
#include "sql/engine/ob_exec_context.h"

namespace oceanbase
{
using namespace common;
namespace sql
{

static int max_buffer_size_ = 8192;

ObPythonUDFSpec::ObPythonUDFSpec(ObIAllocator &alloc, const ObPhyOperatorType type)
    : ObSubPlanScanSpec(alloc, type), col_exprs_(alloc) {}

ObPythonUDFSpec::~ObPythonUDFSpec() {}

ObPythonUDFOp::ObPythonUDFOp(
    ObExecContext &exec_ctx, const ObOpSpec &spec, ObOpInput *input)
  : ObSubPlanScanOp(exec_ctx, spec, input), buf_exprs_(exec_ctx.get_allocator())
{
  int ret = OB_SUCCESS;
  brs_skip_size_ = MY_SPEC.max_batch_size_;
  predict_size_ = 256;
  //predict_size_ = MY_SPEC.max_batch_size_; //default

  //max_buffer_size_ = 8192;
  
  use_input_buf_ = true;
  use_output_buf_ = true;
  use_fake_frame_ = true;
  //if (predict_size_ > MY_SPEC.max_batch_size_)
    //use_fake_frame_ = true;
  //else
    //predict_size_ += MY_SPEC.max_batch_size_;
  if (use_input_buf_) {  
    // init input buffer
    input_buffer_.init(MY_SPEC.col_exprs_, exec_ctx, 2 * max_buffer_size_);
  }
  if (use_output_buf_) {
    // init output buffer
    output_buffer_.init(MY_SPEC.output_, exec_ctx, 2 * max_buffer_size_);
  }
  if (use_fake_frame_) {
    //extra buf_exprs_
    result_width_ = MY_SPEC.col_exprs_.count() + MY_SPEC.calc_exprs_.count() + MY_SPEC.output_.count();
    OZ(buf_exprs_.init(result_width_));
    FOREACH_CNT_X(e, MY_SPEC.col_exprs_, OB_SUCC(ret)) 
      OZ(buf_exprs_.push_back((*e)));
    FOREACH_CNT_X(e, MY_SPEC.calc_exprs_, OB_SUCC(ret)) 
      OZ(buf_exprs_.push_back((*e)));
    FOREACH_CNT_X(e, MY_SPEC.output_, OB_SUCC(ret)) 
      OZ(buf_exprs_.push_back((*e)));
    
    // construct predict buffers
    buf_results_ = static_cast<ObDatum **>(exec_ctx.get_allocator().alloc(sizeof(ObDatum *) * result_width_));
    for(int i = 0; i < result_width_; i++) {
      ObExpr *e = buf_exprs_.at(i);
      OZ(alloc_predict_buffer(exec_ctx.get_allocator(), (*e), buf_results_[i], max_buffer_size_));
    }
    if(!OB_SUCC(ret)) {
      LOG_WARN("Fail to init Predict Operator", K(ret));
    }
  }
}

ObPythonUDFOp::~ObPythonUDFOp() {}

/* predict buffer allocation */
int ObPythonUDFOp::alloc_predict_buffer(ObIAllocator &alloc, ObExpr &expr, ObDatum *&buf_result, int buffer_size)
{
  int ret = OB_SUCCESS;
  buf_result = static_cast<ObDatum *>(alloc.alloc(sizeof(ObDatum) * buffer_size));
  for(int i = 0; i < buffer_size; i++) {
    buf_result[i].ptr_ = static_cast<char *>(alloc.alloc(sizeof(int64_t)));
    buf_result[i].set_null();
  }
  ObBitVector *buf_skip = static_cast<ObBitVector *>(alloc.alloc(ObBitVector::memory_size(buffer_size)));
  ObBitVector *buf_eval_flag = static_cast<ObBitVector *>(alloc.alloc(ObBitVector::memory_size(buffer_size)));
  buf_skip->init(buffer_size);
  buf_eval_flag->init(buffer_size);
  expr.extra_buf_ = {true, buffer_size, buf_result, buf_skip, buf_eval_flag};
  return ret;
}

/* find predict_size of buffer */
int ObPythonUDFOp::find_predict_size(ObExpr *expr, int32_t &predict_size)
{
  int ret = OB_SUCCESS;
  int expr_size = 256; //default
  if (expr->type_ == T_FUN_SYS_PYTHON_UDF) {
    int size = static_cast<ObPythonUdfInfo *>(expr->extra_info_)->predict_size;
    expr_size = size > expr_size ? size : expr_size;
  } else {
    for (int32_t i = 0; i < expr->arg_cnt_; i++) {
      find_predict_size(expr->args_[i], expr_size);
    }
  }
  if (predict_size < expr_size)
    predict_size = expr_size;
  if (predict_size > max_buffer_size_)
    predict_size = max_buffer_size_;
  return ret;
}


/* override */
int ObPythonUDFOp::inner_get_next_batch(const int64_t max_row_cnt)
{
  int ret = OB_SUCCESS;
  if (use_input_buf_) {
    while (OB_SUCC(ret) && (input_buffer_.get_size() <= input_buffer_.get_max_size() - MY_SPEC.max_batch_size_) && !brs_.end_) {
      if (OB_FAIL(ObSubPlanScanOp::inner_get_next_batch(max_row_cnt))) {
        LOG_WARN("fail to inner get next batch", K(ret));
      } else if (OB_FAIL(input_buffer_.save(eval_ctx_, brs_))){
        LOG_WARN("fail to save input batchrows", K(ret));
      }
    }
    // 根据exprs的运行时间调整predict size
    int32_t current_size = 256; //default
    FOREACH_CNT_X(e, MY_SPEC.calc_exprs_, OB_SUCC(ret)) 
      OZ(find_predict_size((*e), current_size));
    FOREACH_CNT_X(e, MY_SPEC.output_, OB_SUCC(ret)) 
      OZ(find_predict_size((*e), current_size));
    predict_size_ = current_size;
    // 取出参数
    if (OB_FAIL(input_buffer_.load(eval_ctx_, brs_, brs_skip_size_, predict_size_))) {
      LOG_WARN("fail to load input batchrows", K(ret));
    } else if (OB_FAIL(clear_calc_exprs_evaluated_flags())) {
      LOG_WARN("fail to clear calc_exprs evaluated flages", K(ret));
    }
  } else {
    ret = ObSubPlanScanOp::inner_get_next_batch(max_row_cnt);
  }
  return ret;
}

/* override */
int ObPythonUDFOp::get_next_batch(const int64_t max_row_cnt, const ObBatchRows *&batch_rows) 
{
  int ret = OB_SUCCESS;
  if (use_output_buf_) {
    while (OB_SUCC(ret) && (output_buffer_.get_size() <= output_buffer_.get_max_size() / 2) && !brs_.end_) {
      if (OB_FAIL(ObOperator::get_next_batch(max_row_cnt, batch_rows))) {
        LOG_WARN("fail to inner get next batch", K(ret));
      } else if (OB_FAIL(output_buffer_.save(eval_ctx_, brs_))){
        LOG_WARN("fail to save input batchrows", K(ret));
      }
    }
    if (!output_buffer_.is_saved()) {
      // do nothing
    } else if (OB_FAIL(output_buffer_.load(eval_ctx_, brs_, brs_skip_size_, MY_SPEC.max_batch_size_))) {
      LOG_WARN("fail to load input batchrows", K(ret));
    }
    batch_rows = &brs_;
  } else {
    ret = ObOperator::get_next_batch(max_row_cnt, batch_rows);
  }
  return ret;
}

int ObPythonUDFOp::clear_calc_exprs_evaluated_flags() {
  int ret = OB_SUCCESS;
  for (int i = 0; i < MY_SPEC.calc_exprs_.count(); i++) {
    MY_SPEC.calc_exprs_[i]->get_eval_info(eval_ctx_).clear_evaluated_flag();
  }
  return ret;
}

/* ------------------------------------ buffer for python_udf ----------------------------------- */
int ObVectorBuffer::init(const common::ObIArray<ObExpr *> &exprs, ObExecContext &exec_ctx, int64_t max_buffer_size)
{
  int ret = OB_SUCCESS;
  if (OB_UNLIKELY(inited_)) {
    // do nothing
  } else if (exprs.count() == 0) {
    // do nothing
  } else {
    exprs_ = &exprs;
    exec_ctx_ = &exec_ctx;
    max_size_ = max_buffer_size; 
    datums_ = static_cast<ObDatum *>(exec_ctx.get_allocator().alloc(
      max_size_ * exprs.count() * sizeof(ObDatum)));
    if (OB_ISNULL(datums_)) {
      ret = OB_ALLOCATE_MEMORY_FAILED;
      LOG_WARN("allocate memory failed", K(ret), K(max_size_), K(exprs.count()));
    }
    inited_ = true;
    saved_size_ = 0;
  }
  return ret;
}

int ObVectorBuffer::save(ObEvalCtx &eval_ctx, ObBatchRows &brs_) 
{
  int ret = OB_SUCCESS;
  int cnt = brs_.size_ - brs_.skip_->accumulate_bit_cnt(brs_.size_);
  if (NULL == exprs_) {
    // empty expr_: do nothing
  } else if (NULL == datums_) {
    ret = OB_NOT_INIT;
  } else if (saved_size_ + cnt <= max_size_) {
    for (int64_t i = 0; i < exprs_->count(); i++) {
      ObExpr *e = exprs_->at(i);
      ObDatum *src = e->locate_batch_datums(eval_ctx);
      int64_t k = 0;
      for (int64_t j = 0; j < brs_.size_; j++) {
        /* remove skipped rows */
        if (!brs_.skip_->at(j)) {
          /* deep copy */
          datums_[i * max_size_ + saved_size_ + k].deep_copy(src[j], exec_ctx_->get_allocator());
          ++k;
        }
      }
    }
    saved_size_ += cnt;
    //brs_.size_ = 0;
  } else {
    ret = OB_ERR_UNEXPECTED;
    LOG_WARN("fail to save vector buffer", K(ret));
  }
  return ret;
}

int ObVectorBuffer::load(ObEvalCtx &eval_ctx, ObBatchRows &brs_, int64_t &brs_skip_size_ , int64_t batch_size) 
{
  int ret = OB_SUCCESS;
  if (NULL == exprs_) {
    // empty expr_: do nothing
  } else if (NULL == datums_ || saved_size_ < 0) {
    ret = OB_NOT_INIT;
  } else if (saved_size_ == 0) {
    brs_.size_ = 0;
    //brs_.end_ = true;
  } else {
    int64_t size = (saved_size_ < batch_size) ? saved_size_ : batch_size;
    for (int64_t i = 0; i < exprs_->count(); i++) {
      ObExpr *e = exprs_->at(i);
      /* deep copy */
      MEMCPY(e->locate_batch_datums(eval_ctx), datums_ + i * max_size_, sizeof(ObDatum) * size); 
      e->get_pvt_skip(eval_ctx).reset(size);
      e->get_evaluated_flags(eval_ctx).reset(size);
    }
    move(size);
    brs_.size_ = size;
    brs_.end_ = false;
    if(size > brs_skip_size_) {
      //realloc batchrows size
      void *mem = exec_ctx_->get_allocator().alloc(ObBitVector::memory_size(size));
      if (OB_ISNULL(mem)) {
        ret = OB_ALLOCATE_MEMORY_FAILED;
        LOG_WARN("allocate memory failed", K(ret));
      } else {
        brs_.skip_ = to_bit_vector(mem);
        brs_.skip_->init(size);
        brs_skip_size_ = size;
      }
    } else {
      brs_.skip_->reset(size);
    }
  }
  return ret;
}

int ObVectorBuffer::move(int64_t size) 
{
  int ret = OB_SUCCESS;
  if (NULL == exprs_) {
    // empty expr_: do nothing
  } else if (NULL == datums_) {
    ret = OB_NOT_INIT;
  } else if (saved_size_ > 0) {
    for (int64_t i = 0; i < exprs_->count(); i++) {
      /* shallow copy */
      MEMMOVE(datums_ + i * max_size_, datums_ + i * max_size_ + size, sizeof(ObDatum) * (saved_size_ - size)); 
    }
    saved_size_ -= size;
  }
  return ret;
}
/* --------------------------------------- end of buffer --------------------------------------*/

} // end namespace sql
} // end namespace oceanbase