  ROOT=/opt/hive/hcatalog

DBROOT=/opt/mysql-connector
USER=hive
HADOOP_HOME=/opt/hadoop
export METASTORE_PORT=9083
export HADOOP_CLIENT_OPTS=" -XX:+HeapDumpOnOutOfMemoryError -XX:HeapDumpPath=./java_pid_<pid>.hprof "

