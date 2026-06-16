#!/bin/bash
# CANN Mul 算子测试编译脚本模板
#
# 用法: bash build.sh <你的测试文件.cpp> [输出文件名]
#
# 示例: bash build.sh my_test.cpp
#       bash build.sh my_test.cpp /tmp/my_test

set -e

TEST_FILE="${1}"
OUTPUT="${2:-/tmp/mul_test}"

if [ -z "$TEST_FILE" ]; then
    echo "用法: bash build.sh <测试文件.cpp>"
    exit 1
fi

g++ -std=c++17 "$TEST_FILE" \
    -I/usr/local/Ascend/cann-8.5.0/include \
    -L/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64 \
    -L/usr/local/Ascend/cann-8.5.0/opp/vendors/custom_math/op_api/lib \
    -lascendcl -lnnopbase -lcust_opapi \
    -Wl,-rpath,/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64 \
    -Wl,-rpath,/usr/local/Ascend/cann-8.5.0/opp/vendors/custom_math/op_api/lib \
    -o "$OUTPUT"

echo "编译成功: $OUTPUT"
