# ----------------------------------------------------------------------------
# CMakeLists.txt — CustomMul 算子
#
# 基于 ops-math/math/mul/CMakeLists.txt 简化：
#   - 只支持 ascend950 (RegBase / 910B)
#   - 只使用 arch35 tiling
# ----------------------------------------------------------------------------

set(SUPPORT_COMPUTE_UNIT "ascend950")
set(SUPPORT_TILING_DIR "arch35")
add_all_modules_sources(OPTYPE custom_mul ACLNNTYPE aclnn_exclude COMPUTE_UNIT ${SUPPORT_COMPUTE_UNIT} TILING_DIR ${SUPPORT_TILING_DIR} DISABLE_IN_OPP TRUE)
