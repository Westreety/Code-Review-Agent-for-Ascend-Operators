#!/bin/bash
# run_all_cases.sh — 逐个运行所有 POC case（每个 case 完全独立）
OPS_MATH="${1:-/models/chenjunyang/workspace/ops-math}"
HERE="$(cd "$(dirname "$0")" && pwd)"

log() { echo -e "\n\033[1;36m=== $* ===\033[0m"; }

for case_dir in "${HERE}"/op_api/*/ "${HERE}"/op_host/*/ "${HERE}"/op_kernel/*/; do
    [ -d "$case_dir" ] || continue
    name=$(basename "$case_dir")
    if [ -f "${case_dir}/run.sh" ]; then
        log "Running ${name}"
        bash "${case_dir}/run.sh" "${OPS_MATH}" || echo "[WARN] ${name} exited with $?"
    fi
done

log "全部完成"
