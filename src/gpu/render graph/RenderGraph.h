#pragma once
#include <iostream>
#include <memory>
#include <vector>

#include "RenderTask.h"
#include "RenderTaskBuilder.h"
#include "Resource.h"
namespace dk {
// ---------------------------------------------
// TimelineStep：编译后的每一步
// ---------------------------------------------
struct TimelineStep
{
    RenderTaskBase*            task = nullptr;
    std::vector<RGResourceBase*> toRealize;
    std::vector<RGResourceBase*> toDerealize;
};

// ---------------------------------------------
// RenderGraph：核心管理者（fg 风格）
// ---------------------------------------------
class RenderGraph
{
public:
    RenderGraph() = default;

    template <typename DataT>
    RenderTask<DataT>* addTask(
        const std::string&                      name,
        typename RenderTask<DataT>::SetupFunc   setup,
        typename RenderTask<DataT>::ExecuteFunc exec)
    {
        markDirty();   // 新增：一旦有人改 graph，就标记需要重新 compile

        auto  task = std::make_unique<RenderTask<DataT>>(name, std::move(setup), std::move(exec));
        auto* t    = task.get();
        t->id      = static_cast<TaskId>(tasks_.size());
        tasks_.push_back(std::move(task));

        // 立即调用 setup 构建资源依赖
        RenderTaskBuilder builder(this, t);
        t->setup(t->data, builder);
        return t;
    }

    // 编译：拓扑排序 + 资源生命周期分析
    void compile();

    // 执行：按 timeline 顺序 realize -> execute -> derealize
    void execute(RenderGraphContext& ctx);
    void markDirty() { compiled_ = false; }
private:
    friend class RenderTaskBuilder;

    std::vector<std::unique_ptr<RenderTaskBase>> tasks_;
    std::vector<std::unique_ptr<RGResourceBase>>   resources_;
    std::vector<TimelineStep>                    timeline_;

    bool compiled_ = false;
    bool compileLogPrinted_ = false;
};


// ---------------------------------------------
// RenderTaskBuilder 模板方法实现
// （必须放在 RenderGraph 定义后）
// ---------------------------------------------
template <typename ResT>
ResT* RenderTaskBuilder::create(const std::string& name,
    const typename ResT::Desc& desc,
    ResourceLifetime           life)
{
    _graph->compiled_ = false;  // 或 _graph->markDirty();
    // ResT 必须继承自 RGResourceBase
    static_assert(std::is_base_of_v<RGResourceBase, ResT>,
        "ResT must derive from RGResourceBase");

    auto  res = std::make_unique<ResT>(name, desc, life);
    auto* ptr = res.get();
    ptr->_id = static_cast<ResourceId>(_graph->resources_.size());
    ptr->creator = _task;
    _graph->resources_.push_back(std::move(res));

    _task->writes.push_back(ptr);
    return ptr;
}

template <typename ResT>
ResT* RenderTaskBuilder::read(ResT* res)
{
    static_assert(std::is_base_of_v<RGResourceBase, ResT>,
        "ResT must derive from RGResourceBase");
    if (!res) return nullptr;
    _task->reads.push_back(res);
    res->readers.push_back(_task);
    return res;
}

template <typename ResT>
ResT* RenderTaskBuilder::write(ResT* res)
{
    static_assert(std::is_base_of_v<RGResourceBase, ResT>,
        "ResT must derive from RGResourceBase");
    if (!res) return nullptr;
    _task->writes.push_back(res);
    res->writers.push_back(_task);
    return res;
}

}
