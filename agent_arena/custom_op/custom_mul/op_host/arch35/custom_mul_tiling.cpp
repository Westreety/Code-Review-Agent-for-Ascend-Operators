/**
 * custom_mul_tiling.cpp — 简化的 Tiling 实现
 *
 * 等长切分，无广播支持。只支持 float32。
 */

#include "custom_mul_tiling.h"
#include "log/log.h"
#include "platform/platform_info.h"

namespace optiling {

static constexpr uint32_t BLOCK_SIZE = 1024;  // 每 block 处理 1024 个元素

bool CustomMulTiling::IsCapable() {
    return true;
}

ge::graphStatus CustomMulTiling::GetPlatformInfo() {
    // 简化: 使用默认 UB 大小
    ubSize_ = 128 * 1024;  // 128KB UB
    return ge::GRAPH_SUCCESS;
}

ge::graphStatus CustomMulTiling::GetShapeAttrsInfo() {
    return ge::GRAPH_SUCCESS;
}

ge::graphStatus CustomMulTiling::DoOpTiling() {
    auto x1Shape = context_->GetInputShape(0);
    if (x1Shape == nullptr) {
        OP_LOGE("CustomMul", "GetInputShape failed.");
        return ge::GRAPH_FAILED;
    }

    uint64_t totalElems = 1;
    for (int64_t i = 0; i < x1Shape->GetDimNum(); i++) {
        totalElems *= x1Shape->GetDim(i);
    }

    uint32_t numBlocks = (totalElems + BLOCK_SIZE - 1) / BLOCK_SIZE;
    context_->SetBlockDim(numBlocks);

    // 写入 tiling 数据
    auto* tilingData = reinterpret_cast<CustomMulTilingData*>(context_->GetRawTilingData());
    tilingData->blockLen = BLOCK_SIZE;
    tilingData->totalLen = static_cast<uint32_t>(totalElems);

    return ge::GRAPH_SUCCESS;
}

ge::graphStatus CustomMulTiling::DoLibApiTiling() {
    return ge::GRAPH_SUCCESS;
}

uint64_t CustomMulTiling::GetTilingKey() const {
    return tilingKey_;
}

ge::graphStatus CustomMulTiling::GetWorkspaceSize() {
    return ge::GRAPH_SUCCESS;
}

ge::graphStatus CustomMulTiling::PostTiling() {
    return ge::GRAPH_SUCCESS;
}

}  // namespace optiling
