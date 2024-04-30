# What is IMBridge

IMBridge is a prototype system based on OceanBase 4.1.0.1 BP2. It is designed to mitigate impedence mismatches between database engines and prediction query execution.

Impedence Mismatches:

* Repetitive Inference Context Setup
* Undesirable Batching Inference

Solutions:

* Staged Prediction Function Evaluation
* Batch-aware Function Invocation

System Implementation:

* Vectorized Python UDF based prediction function evaluation (src/sql/engine/expr/ob_expr_python_udf.cpp)
* One-off Inference Context Setup
* Python UDF metadata management (Parser, Resolver, RootService...)
* Statement Rewriter to detect and extract Python UDF (src/sql/rewrite/ob_transform_pullup_filter.cpp)
* Specific Prediction-aware Operator with datums buffer (src/sql/engine/python_udf_engine/ob_python_udf_op.cpp)
* Adaptive inference batch size esitimated by heuristic algorithm

Quick Start:

1. Setup the system environment. It is optional to pull the docker image. (docker pull zcylovelynewlife/ob_env_centos7:v3)
2. Download and install OceanBase-all-in-one package (Version 4.1.0) from www.oceanbase.com/softwarecenter.
3. Deploy the OceanBase cluster.
4. Config and run replaceOb.sh shell. (./replaceOb.sh debug/release)
5. Redeploy the OceanBase cluster.
6. Create your Python UDF and make prediction!

See more information in www.github.com/lovelynewlife/oceanbase/wiki and our commit history records in www.github.com/watch2bear/oceanbase_PyUdf .

Welcome to contact us!

Email: <jxpeng@stu.ecnu.edu.cn> | <cyzhangecnu@stu.ecnu.edu.cn>

