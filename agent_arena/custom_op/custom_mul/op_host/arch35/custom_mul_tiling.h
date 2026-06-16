/**
 * custom_mul_tiling.h — Tiling 数据结构
 */

#ifndef CUSTOM_MUL_TILING_H_
#define CUSTOM_MUL_TILING_H_

#include "tiling/tiling_api.h"
#include "op_host/tiling_base_class.h"

namespace optiling {

struct CustomMulTilingData {
    uint32_t blockLen;
    uint32_t totalLen;
};

class CustomMulTiling : public Ops::Base::TilingBaseClass {
public:
    explicit CustomMulTiling(gert::TilingContext* context)
        : Ops::Base::TilingBaseClass(context) {}

    gert::TilingContext* GetContext() const { return context_; }

protected:
    bool IsCapable() override;
    ge::graphStatus GetPlatformInfo() override;
    ge::graphStatus GetShapeAttrsInfo() override;
    ge::graphStatus DoOpTiling() override;
    ge::graphStatus DoLibApiTiling() override;
    uint64_t GetTilingKey() const override;
    ge::graphStatus GetWorkspaceSize() override;
    ge::graphStatus PostTiling() override;

public:
    uint64_t tilingKey_ = 0;
    int64_t ubSize_ = 0;
};

}  // namespace optiling

#endif  // CUSTOM_MUL_TILING_H_
