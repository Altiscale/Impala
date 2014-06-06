# Copyright 2012 Cloudera Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# Source this file from the $IMPALA_HOME directory to
# setup your environment. If $IMPALA_HOME is undefined
# this script will set it to the current working directory.

export JAVA_HOME=${JAVA_HOME-/usr/java/default}
if [ ! -d $JAVA_HOME ] ; then
  export JAVA_HOME="/usr/lib/jvm/java-openjdk"
  echo "ok - trying to look for openjdk path $JAVA_HOME"
  if [ ! -d $JAVA_HOME ] ; then
    echo "Error! JAVA_HOME must be set to the location of your JDK!"
    exit 1
  fi
fi
echo "ok - exist JAVA_HOME=$JAVA_HOME"

# Default hadoop and hive version is 2.2.0 and 0.12.0
if [ "x${HADOOP_VERSION}" = "x" ] ; then
  export HADOOP_VERSION=2.2.0
fi
if [ "x${HIVE_VERSION}" = "x" ] ; then
  export HIVE_VERSION=0.12.0
fi
if [ "x${IMPALA_VERSION}" = "x" ] ; then
  export IMPALA_VERSION=1.2.2
fi

if [ -z $IMPALA_HOME ]; then
    this=${0/-/} # login-shells often have leading '-' chars
    shell_exec=`basename $SHELL`
    if [ "$this" = "$shell_exec" ]; then
        # Assume we're already in IMPALA_HOME
        interactive=1
        export IMPALA_HOME=`pwd`
    else
        interactive=0
        while [ -h "$this" ]; do
            ls=`ls -ld "$this"`
            link=`expr "$ls" : '.*-> \(.*\)$'`
            if expr "$link" : '.*/.*' > /dev/null; then
                this="$link"
            else
                this=`dirname "$this"`/"$link"
            fi
        done

        # convert relative path to absolute path
        bin=`dirname "$this"`
        script=`basename "$this"`
        bin=`cd "$bin"; pwd`
        this="$bin/$script"

        export IMPALA_HOME=`dirname "$bin"`
    fi
fi

HADOOP_LZO_JAR=`find /opt/hadoop-${HADOOP_VERSION}/share/hadoop/common/lib/ -type f -name "hadoop-lzo*.jar" | head -1`
export HADOOP_LZO=${HADOOP_LZO-/opt/hadoop-$HADOOP_VERSION}
export IMPALA_LZO=${IMPALA_LZO-~/Impala-lzo}

# Directory where local cluster logs will go when running tests or loading data

export IMPALA_GFLAGS_VERSION=2.0
export IMPALA_GPERFTOOLS_VERSION=2.0
export IMPALA_GLOG_VERSION=0.3.2
export IMPALA_GTEST_VERSION=1.6.0
export IMPALA_SNAPPY_VERSION=1.0.5
export IMPALA_CYRUS_SASL_VERSION=2.1.23
export IMPALA_OPENLDAP_VERSION=2.4.25
export IMPALA_SQUEASEL_VERSION=3.3

#export IMPALA_HADOOP_VERSION=2.0.0-cdh4.5.0
#export IMPALA_HBASE_VERSION=0.94.6-cdh4.5.0
#export IMPALA_HIVE_VERSION=0.10.0-cdh4.5.0
export IMPALA_SENTRY_VERSION=1.1.0
export IMPALA_THRIFT_VERSION=0.9.0
export IMPALA_AVRO_VERSION=1.7.4
export IMPALA_LLVM_VERSION=3.3
export IMPALA_PARQUET_VERSION=1.2.5

export PATH=$IMPALA_HOME/bin:$PATH

export HADOOP_HOME=/opt/hadoop
if [ ! -d "$HADOOP_HOME" ] ; then
  export HADOOP_HOME=/opt/hadoop-$HADOOP_VERSION
  if [ ! -d "$HADOOP_HOME" ] ; then
    echo "error - $HADOOP_HOME doesn't exist, the installation may not be complete for build process, symbolic link wasn't created"
  fi
fi
export HADOOP_CONF_DIR=/etc/hadoop
if [ ! -d "$HADOOP_CONF_DIR" ] ; then
  export HADOOP_CONF_DIR=/etc/hadoop-$HADOOP_VERSION
  if [ ! -d "$HADOOP_CONF_DIR" ] ; then
    echo "error - $HADOOP_CONF_DIR doesn't exist, the installation may not be complete for build process, symbolic link wasn't created"
  fi
fi

export PATH=$HADOOP_HOME/bin:$PATH
export PATH=$HIVE_HOME/bin:$PATH

### Hive looks for jar files in a single directory from HIVE_AUX_JARS_PATH plus
### any jars in AUX_CLASSPATH. (Or a list of jars in HIVE_AUX_JARS_PATH.)
export HIVE_AUX_JARS_PATH=$IMPALA_FE_DIR/target
export AUX_CLASSPATH=$HADOOP_LZO_JAR

# Configure python path
. $IMPALA_HOME/bin/set-release-pythonpath.sh

# These arguments are, despite the name, passed to every JVM created
# by an impalad.
# - Enable JNI check
# When running hive UDFs, this check makes it unacceptably slow (over 100x)
# Enable if you suspect a JNI issue
# TODO: figure out how to turn this off only the stuff that can't run with it.
#LIBHDFS_OPTS="-Xcheck:jni -Xcheck:nabounds"
# - Points to the location of libbackend.so.
LIBHDFS_OPTS="${LIBHDFS_OPTS} -Djava.library.path=${HADOOP_HOME}/lib/native/"
# READER BEWARE: This always points to the debug build.
# TODO: Consider having cmake scripts change this value depending on
# the build type.
export LIBHDFS_OPTS="${LIBHDFS_OPTS}"

export JAVA_LIBRARY_PATH=/usr/lib/impala/lib/

# So that the frontend tests and PlanService can pick up libbackend.so
# and other required libraries
LIB_JAVA=`find ${JAVA_HOME}/   -name libjava.so | head -1`
LIB_JSIG=`find ${JAVA_HOME}/   -name libjsig.so | head -1`
LIB_JVM=` find ${JAVA_HOME}/   -name libjvm.so  | head -1`
LIB_HDFS=`find ${HADOOP_HOME}/ -name libhdfs.so | head -1`
LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:`dirname ${LIB_JAVA}`:`dirname ${LIB_JSIG}`"
LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:`dirname ${LIB_JVM}`:`dirname ${LIB_HDFS}`"
LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/usr/lib/impala/lib"
export LD_LIBRARY_PATH
export LD_PRELOAD="${LD_PRELOAD}:${LIB_JSIG}"

#CLASSPATH=$IMPALA_FE_DIR/target/dependency:$CLASSPATH
#CLASSPATH=$IMPALA_FE_DIR/target/classes:$CLASSPATH
#CLASSPATH=$IMPALA_FE_DIR/src/test/resources:$CLASSPATH
CLASSPATH=$HADOOP_LZO_JAR:$CLASSPATH
for jar in `find /usr/lib/impala/lib/ -type f -name "*.jar"`
do
  CLASSPATH=$CLASSPATH:$jar
done
export CLASSPATH

# Setup aliases
# Helper alias to script that verifies and merges Gerrit changes
# alias gerrit-verify-merge="${IMPALA_AUX_TEST_HOME}/jenkins/gerrit-verify-merge.sh"

echo "IMPALA_HOME            = $IMPALA_HOME"
echo "HADOOP_HOME            = $HADOOP_HOME"
echo "HADOOP_CONF_DIR        = $HADOOP_CONF_DIR"
echo "MINI_DFS_BASE_DATA_DIR = $MINI_DFS_BASE_DATA_DIR"
echo "HIVE_HOME              = $HIVE_HOME"
echo "HIVE_CONF_DIR          = $HIVE_CONF_DIR"
echo "HBASE_HOME             = $HBASE_HOME"
echo "HBASE_CONF_DIR         = $HBASE_CONF_DIR"
echo "THRIFT_HOME            = $THRIFT_HOME"
echo "HADOOP_LZO             = $HADOOP_LZO"
echo "IMPALA_LZO             = $IMPALA_LZO"
echo "CLASSPATH              = $CLASSPATH"
echo "LIBHDFS_OPTS           = $LIBHDFS_OPTS"
echo "PYTHONPATH             = $PYTHONPATH"
echo "JAVA_HOME              = $JAVA_HOME"
echo "LD_LIBRARY_PATH        = $LD_LIBRARY_PATH"
echo "LD_PRELOAD             = $LD_PRELOAD"
