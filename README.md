# What is IMBridge
IMBridge is a prototype system based on OceanBase 4.1.0.1 BP2. It is designed to mitigate impedence mismatches between database engines and prediction query execution.

Impedence Mismatches:
* Repetitive Inference Context Setup
* Undesirable Batching Inference

Solutions:
* Staged Prediction Function Evaluation
* Batch-aware Function Invocation

System Implementation:
* Vectorwise Python UDF expression evaluation
* One-off Inference Context Setup
* Python UDF metadata management 
* Statement Rewriter to detect and extract Python UDF
* Specific Prediction-aware Operator with datums buffer
* Adaptive inference batch size determined by heuristic algorithm

Quick Start:
0. Setup the system environment. It is optional to pull the docker image. (docker pull zcylovelynewlife/ob_env_centos7:v3)
1. Download and install OceanBase-all-in-one package (Version 4.1.0) from www.oceanbase.com/softwarecenter.
2. Deploy the OceanBase cluster.
3. Config and run replaceOb.sh shell.
4. Redeploy the OceanBase cluster.
5. Create your Python UDF and make prediction!

See more information in github.com/lovelynewlife/oceanbase/wiki
Welcome to contact us!
Email: junxiongpengecnu@outlook.com | 

