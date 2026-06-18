#!/bin/bash
# deploy.sh — 部署指定 case 的 buggy 算子到 NPU
#
# 用法: bash deploy.sh <case_name> [ops_math_path]
# 示例: bash deploy.sh case_01_nullptr_check
#       bash deploy.sh restore  （恢复正确版算子）

set -e

CASE="${1}"
OPS_MATH="${2:-/models/chenjunyang/workspace/ops-math}"
ERROR_DIR="/models/chenjunyang/workspace/op_testing/error_testset"

if [ -z "$CASE" ]; then
    echo "用法: bash deploy.sh <case_name|restore>"
    echo "可用 case:"
    find "${ERROR_DIR}/op_api" "${ERROR_DIR}/op_host" "${ERROR_DIR}/op_kernel" -maxdepth 1 -mindepth 1 -type d 2>/dev/null | sed "s|${ERROR_DIR}/||" | sort
    exit 1
fi

export ASCEND_HOME=/usr/local/Ascend/ascend-toolkit/latest
source ${ASCEND_HOME}/set_env.sh 2>/dev/null || true

MUL_DIR="${OPS_MATH}/math/mul"

# ---- 还原正确版 ----
restore_correct() {
    echo "[restore] 恢复 lin_space..."
    [ -d /tmp/lin_space.bak ] && mv /tmp/lin_space.bak "${OPS_MATH}/math/lin_space" 2>/dev/null || true

    echo "[restore] 还原源码..."
    cd "${OPS_MATH}"
    git checkout -- math/mul/op_api/aclnn_mul.cpp 2>/dev/null || true

    echo "[restore] 编译正确版算子..."
    [ -d math/lin_space ] && mv math/lin_space /tmp/lin_space.bak
    bash build.sh --pkg --soc=ascend910 --ops=mul --vendor_name=custom > /tmp/deploy_build.log 2>&1

    echo "[restore] 安装..."
    bash build_out/cann-ops-math-custom_linux-aarch64.run >> /tmp/deploy_build.log 2>&1

    [ -d /tmp/lin_space.bak ] && mv /tmp/lin_space.bak "${OPS_MATH}/math/lin_space" 2>/dev/null || true
    echo "[restore] 正确版算子已部署"
}

if [ "$CASE" = "restore" ]; then
    restore_correct
    exit 0
fi

# ---- 部署指定 case 的 buggy 版本 ----
# 搜索 case 目录（支持 op_api/ op_host/ op_kernel/ 子目录）
CASE_DIR=""
for prefix in "" "op_api/" "op_host/" "op_kernel/" "baseline/"; do
    if [ -d "${ERROR_DIR}/${prefix}${CASE}" ]; then
        CASE_DIR="${ERROR_DIR}/${prefix}${CASE}"
        break
    fi
done

if [ -z "$CASE_DIR" ] || [ ! -d "$CASE_DIR" ]; then
    echo "错误: case 目录不存在: $CASE"
    echo "可用 case:"
    find "${ERROR_DIR}/op_api" "${ERROR_DIR}/op_host" "${ERROR_DIR}/op_kernel" "${ERROR_DIR}/baseline" -maxdepth 1 -mindepth 1 -type d 2>/dev/null | sed "s|${ERROR_DIR}/||" | sort
    exit 1
fi

echo "[deploy] 部署 $CASE 的 buggy 算子..."

# 1. 注入 bug
echo "[1/3] 注入 bug..."
cd "${MUL_DIR}"
\cp op_api/aclnn_mul.cpp op_api/aclnn_mul.cpp.bak

# 根据 case 执行对应的注入
	case "$CASE" in
	    01_1.1_nullptr_check|case_01_nullptr_check|1.1_nullptr_check)
	        sed -i 's/OP_CHECK_NULL(out, return false);/(void)out;/' op_api/aclnn_mul.cpp
	        touch op_api/aclnn_mul.cpp
	        ;;
	    02_4.3_shape_verify|case_02_output_shape|4.3_shape_verify)
	        sed -i 's/OP_CHECK_SHAPE_NOT_EQUAL_WITH_EXPECTED_SIZE(out, dstShape, return false);/(void)out;/' op_api/aclnn_mul.cpp
	        touch op_api/aclnn_mul.cpp
	        ;;
	    03_2.1_mixdtype_removed|case_03_tiling_missing|2.1_mixdtype_removed)
	        sed -i '/MulCheckFormat(self, other);/a\
	  if (self->GetDataType() == DataType::DT_DOUBLE) return ACLNN_ERR_PARAM_INVALID;' op_api/aclnn_mul.cpp
	        touch op_api/aclnn_mul.cpp
	        ;;
	    04_3.2_empty_tensor|case_04_empty_tensor|3.2_empty_tensor)
	        sed -i '/\/\/ 空tensor处理$/,/^  }/{ /\/\/ 空tensor处理$/!{ /^  }/!d; }; /^  }/d }' op_api/aclnn_mul.cpp
	        touch op_api/aclnn_mul.cpp
	        ;;
	    05_1.2_dtype_whitelist|1.2_dtype_whitelist)
	        sed -i '/DT_DOUBLE,/s/        DataType::DT_DOUBLE, //' op_api/aclnn_mul.cpp
	        touch op_api/aclnn_mul.cpp
	        ;;
	    06_1.3_dtype_overwide|1.3_dtype_overwide)
	        sed -i '/DT_BOOL,/s/DataType::DT_BOOL/DataType::DT_BOOL, DataType::DT_UINT32/' op_api/aclnn_mul.cpp
	        touch op_api/aclnn_mul.cpp
	        ;;
	    07_1.4_errorcode_masking|1.4_errorcode_masking)
	        sed -i 's/CHECK_RET(CheckMulNotNull(self, other, out), ACLNN_ERR_PARAM_NULLPTR)/CHECK_RET(CheckMulNotNull(self, other, out), ACLNN_SUCCESS)/' op_api/aclnn_mul.cpp
	        touch op_api/aclnn_mul.cpp
	        ;;
	    08_4.2_dim_check|4.2_dim_check)
	        sed -i '/OP_CHECK_MAX_DIM(self, MAX_SUPPORT_DIMS_NUMS, return false);/d; /OP_CHECK_MAX_DIM(other, MAX_SUPPORT_DIMS_NUMS, return false);/d' op_api/aclnn_mul.cpp
	        touch op_api/aclnn_mul.cpp
	        ;;
	    *)
	        echo "未知 case: $CASE，跳过注入"
	        ;;
	esac

# 2. 编译
echo "[2/3] 编译算子..."
cd "${OPS_MATH}"
[ -d math/lin_space ] && mv math/lin_space /tmp/lin_space.bak
bash build.sh --pkg --soc=ascend910 --ops=mul --vendor_name=custom > /tmp/deploy_build.log 2>&1

# 3. 安装
echo "[3/3] 安装..."
bash build_out/cann-ops-math-custom_linux-aarch64.run >> /tmp/deploy_build.log 2>&1

[ -d /tmp/lin_space.bak ] && mv /tmp/lin_space.bak "${OPS_MATH}/math/lin_space" 2>/dev/null || true

echo "[deploy] $CASE 的 buggy 算子已部署到 NPU"
echo "[deploy] 要恢复正确版: bash deploy.sh restore"
