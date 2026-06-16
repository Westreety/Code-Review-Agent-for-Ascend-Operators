#!/bin/bash
# 1.1 空指针校验缺失
set -e
OPS_MATH="${1:-/models/chenjunyang/workspace/ops-math}"
HERE="$(cd "$(dirname "$0")" && pwd)"

export ASCEND_HOME=/usr/local/Ascend/ascend-toolkit/latest
source ${ASCEND_HOME}/set_env.sh 2>/dev/null || true
export PATH=${ASCEND_HOME}/bin:${ASCEND_HOME}/compiler/bin:${PATH}
export LD_LIBRARY_PATH=${ASCEND_HOME}/aarch64-linux/lib64:/usr/local/Ascend/cann-8.5.0/opp/vendors/custom_math/op_api/lib:${LD_LIBRARY_PATH}

MUL_DIR="${OPS_MATH}/math/mul"
cleanup() {
    [ -f "${MUL_DIR}/op_api/aclnn_mul.cpp.bak" ] && \cp "${MUL_DIR}/op_api/aclnn_mul.cpp.bak" "${MUL_DIR}/op_api/aclnn_mul.cpp" && rm -f "${MUL_DIR}/op_api/aclnn_mul.cpp.bak"
    [ -d /tmp/lin_space.bak_poc ] && mv /tmp/lin_space.bak_poc "${OPS_MATH}/math/lin_space" 2>/dev/null
}
trap cleanup EXIT

echo "[1/5] 注入错误: 删除 out 空指针检查..."
cd "${MUL_DIR}"
\cp op_api/aclnn_mul.cpp op_api/aclnn_mul.cpp.bak
sed -i 's/OP_CHECK_NULL(out, return false);/(void)out;  \/\/ BUG: nullptr check removed/' op_api/aclnn_mul.cpp
touch op_api/aclnn_mul.cpp

echo "[2/5] 编译算子..."
cd "${OPS_MATH}"
[ -d math/lin_space ] && mv math/lin_space /tmp/lin_space.bak_poc
bash build.sh --pkg --soc=ascend910 --ops=mul --vendor_name=custom > /tmp/poc_build.log 2>&1

echo "[3/5] 安装..."
bash build_out/cann-ops-math-custom_linux-aarch64.run >> /tmp/poc_build.log 2>&1

echo "[4/5] 编译并运行测试..."
RPT="${HERE}/result_$(date +%Y%m%d_%H%M%S).md"
cat > "${RPT}" << 'EOF'
# POC Test Report: 1.1_nullptr_check

| 项目 | 内容 |
|------|------|
| **错误分类** | 一类·参数校验 (1.1) |
| **错误名称** | 空指针校验缺失 |
| **注入层** | op_api — `aclnn_mul.cpp` |
| **注入难度** | 🟢 易 |
| **可检测性** | 🟢 易 |

## 修改内容

| 文件 | 函数 | 变更 |
|------|------|------|
| `math/mul/op_api/aclnn_mul.cpp` | `CheckMulNotNull()` | `OP_CHECK_NULL(out, return false)` → `(void)out;` |

**注入方式**: 删除 `out` 参数的空指针校验宏，代以无操作的 `(void)out;`。
**预期后果**: `out` 为 `nullptr` 时不再被拦截，后续解引用导致段错误 (SEGFAULT)。

## 攻击测试策略
构造 `self` 和 `other` 为合法 tensor，`out` 传 `nullptr`。

| 行为 | 预期 |
|------|------|
| 正确算子 | GetWorkspaceSize 返回 ACLNN_ERR_PARAM_NULLPTR |
| Bug 算子 | 返回 ACLNN_SUCCESS 或段错误 |

## 测试结果

EOF
echo "| **时间** | $(date) |" >> "${RPT}"

set +e
g++ -std=c++17 "${HERE}/test_attack.cpp" \
    -I/usr/local/Ascend/cann-8.5.0/include \
    -L/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64 \
    -L/usr/local/Ascend/cann-8.5.0/opp/vendors/custom_math/op_api/lib \
    -lascendcl -lnnopbase -lcust_opapi \
    -Wl,-rpath,/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64 \
    -Wl,-rpath,/usr/local/Ascend/cann-8.5.0/opp/vendors/custom_math/op_api/lib \
    -o /tmp/poc_test 2>/tmp/poc_compile.log
COMPILE_RET=$?

if [ $COMPILE_RET -eq 0 ]; then
    echo "| **编译** | ✅ 成功 |" >> "${RPT}"
    echo '```' >> "${RPT}"
    /tmp/poc_test >> "${RPT}" 2>&1; TEST_EXIT=$?
    echo '```' >> "${RPT}"
else
    echo "| **编译** | ❌ 失败 |" >> "${RPT}"
    echo '```' >> "${RPT}"; cat /tmp/poc_compile.log >> "${RPT}"; echo '```' >> "${RPT}"
    TEST_EXIT=1
fi
set -e

if [ $TEST_EXIT -ne 0 ]; then
    echo "| **结果** | ✅ BUG DETECTED (exit=${TEST_EXIT}) |" >> "${RPT}"
    echo -e "\n## 结论\n\n✅ 注入的错误被成功检测。" >> "${RPT}"
else
    echo "| **结果** | ❌ BUG NOT DETECTED |" >> "${RPT}"
    echo -e "\n## 结论\n\n❌ 注入的错误未被检测。" >> "${RPT}"
fi
ln -sf "$(basename "${RPT}")" "${HERE}/result_latest.md"
exit $TEST_EXIT
