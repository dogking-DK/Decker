#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Resource.h"

namespace dk {
class RenderTaskBuilder;

// ---------------------------------------------
// RenderTaskBase：任务基类
// ---------------------------------------------
struct  RenderTaskBase
{
    TaskId      id = ~0u;
    std::string name;

    std::vector<RGResourceBase*> reads;
    std::vector<RGResourceBase*> writes;

    virtual      ~RenderTaskBase() = default;
    virtual void execute(RenderGraphContext& ctx) = 0;
};

// ---------------------------------------------
// 模板 RenderTask<Data>：带用户数据和 lambda
// ---------------------------------------------
template <typename DataT>
struct RenderTask : RenderTaskBase
{
    using Data        = DataT;
    using SetupFunc   = std::function<void(Data&, RenderTaskBuilder&)>;
    using ExecuteFunc = std::function<void(const Data&, RenderGraphContext&)>;

    Data        data{};
    SetupFunc   setup;
    ExecuteFunc run;

    RenderTask(const std::string& n,
               SetupFunc          s,
               ExecuteFunc        e)
        : setup(std::move(s)), run(std::move(e))
    {
        name = n;
    }

    void execute(RenderGraphContext& ctx) override
    {
        if (run) run(data, ctx);
    }
};
}
