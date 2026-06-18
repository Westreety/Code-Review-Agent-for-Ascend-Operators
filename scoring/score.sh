#!/bin/bash
# score.sh — 对照 ground_truth 评分 Agent 输出
#
# 用法: bash score.sh <case_number> <agent_output.md>
#
# 示例: bash score.sh 01 agent_arena/output/case_01/result.md

set -e

CASE_NUM="${1}"
AGENT_OUTPUT="${2}"

if [ -z "$CASE_NUM" ] || [ -z "$AGENT_OUTPUT" ]; then
    echo "用法: bash score.sh <case_number> <agent_output.md>"
    echo "示例: bash score.sh 01 agnt_output/case_01/result.md"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GT_FILE="${SCRIPT_DIR}/../ground_truth/case_${CASE_NUM}.md"

if [ ! -f "$GT_FILE" ]; then
    echo "错误: ground_truth 文件不存在: $GT_FILE"
    exit 1
fi

if [ ! -f "$AGENT_OUTPUT" ]; then
    echo "错误: Agent 输出文件不存在: $AGENT_OUTPUT"
    exit 1
fi

echo "=========================================="
echo "  Case ${CASE_NUM} 评分报告"
echo "=========================================="
echo ""

# --- 显示 ground_truth 关键信息 ---
echo "--- Ground Truth ---"
grep -A1 "文件" "$GT_FILE" | head -2
grep -A1 "函数" "$GT_FILE" | head -2
grep -A1 "行号" "$GT_FILE" | head -2
grep -A1 "错误分类" "$GT_FILE" | head -2
echo ""

# --- 评分维度 ---
# 维度 1: bug 发现 (0/1/2)
#   检测: Agent 是否提到了正确的函数名、大致行号范围
echo "--- 评分 ---"
echo ""

BUG_SCORE=0
TEST_SCORE=0
FIX_SCORE=0

# 1. Bug 发现评分
FUNC_NAME=$(grep -oP '函数.*?`\K[^`]+' "$GT_FILE" | head -1)
# 去掉 Markdown 链接语法和尾部括号，避免 grep 字面匹配问题
FUNC_NAME=$(echo "$FUNC_NAME" | sed 's/()$//')
LINE_NUM=$(grep -oP '行号.*?第\s*\K[0-9]+' "$GT_FILE" | head -1)

if [ -n "$FUNC_NAME" ]; then
    if grep -qi "$FUNC_NAME" "$AGENT_OUTPUT" 2>/dev/null; then
        BUG_SCORE=1
        # 检查是否有具体行号
        if grep -qP '[0-9]{2,}' "$AGENT_OUTPUT" 2>/dev/null; then
            BUG_SCORE=2
        fi
    fi
fi
echo "  bug 发现:  ${BUG_SCORE}/2  (函数: ${FUNC_NAME})"

# 2. 测试生成评分 (0/1/2)
#   检测: 是否包含 C++ 测试代码
if grep -qP '(aclnnMul|aclnnMulGetWorkspaceSize|CreateAclTensor|aclTensor)' "$AGENT_OUTPUT" 2>/dev/null; then
    TEST_SCORE=1
    # 检查是否有 NPU 运行结果
    if grep -qP '(PASS|FAIL|exit=|returned|Segmentation|aclrtMalloc)' "$AGENT_OUTPUT" 2>/dev/null; then
        TEST_SCORE=2
    fi
fi
echo "  测试生成:  ${TEST_SCORE}/2"

# 3. 修复方案评分 (0/1/2)
#   检测: 是否包含 fix/diff/修复 相关内容
if grep -qiP '(fix|修复|恢复|diff|正确.*写法)' "$AGENT_OUTPUT" 2>/dev/null; then
    FIX_SCORE=1
    # 检查是否有具体代码
    if grep -qP '(OP_CHECK|return\s+(true|false))' "$AGENT_OUTPUT" 2>/dev/null; then
        FIX_SCORE=2
    fi
fi
echo "  解释修复:  ${FIX_SCORE}/2"

TOTAL=$((BUG_SCORE + TEST_SCORE + FIX_SCORE))
PASS_THRESHOLD=4

echo ""
echo "  总分: ${TOTAL}/6  ($([ $TOTAL -ge $PASS_THRESHOLD ] && echo '✅ 通过' || echo '❌ 未通过'))"
echo ""

# --- 关键差异对比 ---
echo "--- 关键差异 ---"
echo ""
echo "=== Ground Truth (关键内容) ==="
head -30 "$GT_FILE"
echo ""
echo "=== Agent 输出 (前 60 行) ==="
head -60 "$AGENT_OUTPUT"

echo ""
echo "=========================================="
echo "  详细 ground_truth: ${GT_FILE}"
echo "  Agent 输出: ${AGENT_OUTPUT}"
echo "=========================================="

exit 0
