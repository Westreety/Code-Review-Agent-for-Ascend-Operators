#!/bin/bash
# build.sh — 编译 CustomMul 算子（集成到 ops-math 构建系统）
#
# 用法: bash build.sh [--pkg] [--clean]
#   需要在 ops-math 根目录下运行，通过软链接或路径映射集成 custom_mul 目录。

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OPS_MATH="${OPS_MATH:-/models/chenjunyang/workspace/ops-math}"

DO_PKG=true
DO_CLEAN=false

for arg in "$@"; do
    case "$arg" in
        --clean) DO_CLEAN=true; DO_PKG=false ;;
        --no-pkg) DO_PKG=false ;;
    esac
done

if [ "$DO_CLEAN" = true ]; then
    rm -rf "${OPS_MATH}/math/custom_mul"
    echo "[clean] removed math/custom_mul symlink"
    exit 0
fi

echo "[custom_mul] 链接到 ops-math 构建系统..."

# 创建软链接到 ops-math/math/ 下
ln -sfn "${SCRIPT_DIR}" "${OPS_MATH}/math/custom_mul"

# 使用 ops-math 的构建系统编译
cd "${OPS_MATH}"

if [ "$DO_PKG" = true ]; then
    echo "[custom_mul] 编译 + 打包..."
    bash build.sh --pkg --soc=ascend910 --ops=custom_mul --vendor_name=custom_test > /tmp/custom_mul_build.log 2>&1

    if [ $? -eq 0 ]; then
        echo "[custom_mul] 安装到 NPU..."
        bash build_out/cann-ops-math-custom_test_linux-aarch64.run >> /tmp/custom_mul_build.log 2>&1
        echo "[custom_mul] ✅ 部署完成"
        echo "[custom_mul] 库路径: /usr/local/Ascend/cann-8.5.0/opp/vendors/custom_test/op_api/lib/"
    else
        echo "[custom_mul] ❌ 编译失败，查看 /tmp/custom_mul_build.log"
        exit 1
    fi
else
    echo "[custom_mul] 仅编译（不打包）..."
    bash build.sh --soc=ascend910 --ops=custom_mul --vendor_name=custom_test > /tmp/custom_mul_build.log 2>&1
fi
