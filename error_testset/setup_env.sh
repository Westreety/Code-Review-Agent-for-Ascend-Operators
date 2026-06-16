#!/bin/bash
# ============================================================
# setup_env.sh — CANN 环境配置脚本
# 在每次容器重启后执行一次即可
# ============================================================

set -e

echo "=== 检查 CANN 环境 ==="

# 1. 设置 ASCEND_HOME
export ASCEND_HOME=/usr/local/Ascend/ascend-toolkit/latest

if [ ! -f "${ASCEND_HOME}/version.cfg" ]; then
    echo "[ERROR] CANN 工具链未安装！请检查 ${ASCEND_HOME}"
    exit 1
fi
echo "[OK] CANN 版本: $(grep 'runtime_running_version' ${ASCEND_HOME}/version.cfg | head -1)"

# 2. 加载 set_env.sh
if [ -f "${ASCEND_HOME}/set_env.sh" ]; then
    source ${ASCEND_HOME}/set_env.sh
    echo "[OK] set_env.sh 已加载"
else
    # 手动设置关键环境变量
    export PATH=${ASCEND_HOME}/bin:${ASCEND_HOME}/compiler/bin:$PATH
    export LD_LIBRARY_PATH=${ASCEND_HOME}/lib64:${ASCEND_HOME}/compiler/lib64:$LD_LIBRARY_PATH
    export PYTHONPATH=${ASCEND_HOME}/python/site-packages:$PYTHONPATH
    echo "[OK] 手动设置环境变量"
fi

# 3. 验证
echo ""
echo "=== 环境检查 ==="
echo "ASCEND_HOME       = ${ASCEND_HOME}"
echo "g++               = $(g++ --version | head -1)"
echo "cmake             = $(cmake --version | head -1)"
echo "NPU 数量           = $(npu-smi info 2>/dev/null | grep -c 'Ascend910')"
echo ""

# 4. 确认 ops-math 存在
OPS_MATH_PATH=${1:-/models/chenjunyang/workspace/ops-math}
if [ -d "${OPS_MATH_PATH}/math/mul/op_api" ]; then
    echo "[OK] ops-math 源码: ${OPS_MATH_PATH}"
else
    echo "[WARN] ops-math 未找到于 ${OPS_MATH_PATH}"
    echo "       请确认路径并传入: source setup_env.sh /your/ops-math/path"
fi

# 5. 确认 error_testset 存在
TEST_SET_PATH=${2:-/models/chenjunyang/workspace/error_testset}
if [ -f "${TEST_SET_PATH}/ERROR_TAXONOMY.md" ]; then
    echo "[OK] error_testset: ${TEST_SET_PATH}"
else
    echo "[WARN] error_testset 未找到于 ${TEST_SET_PATH}"
    echo "       请确认路径并传入: source setup_env.sh /ops-math /error_testset"
fi

echo ""
echo "=== 环境就绪，可以运行: bash ${TEST_SET_PATH}/run_all_cases.sh ==="
