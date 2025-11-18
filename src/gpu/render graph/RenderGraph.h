#pragma once
#include <iostream>
#include <memory>
#include <vector>

#include "RenderTask.h"

namespace dk {

// ---------------------------------------------
    // TimelineStep：编译后的每一步
    // ---------------------------------------------
    struct TimelineStep {
        RenderTaskBase* task = nullptr;
        std::vector<ResourceBase*> toRealize;
        std::vector<ResourceBase*> toDerealize;
    };

    // ---------------------------------------------
    // RenderGraph：核心管理者（fg 风格）
    // ---------------------------------------------
    class RenderGraph {
    public:
        RenderGraph() = default;

        template<typename DataT>
        RenderTask<DataT>* addTask(
            const std::string& name,
            typename RenderTask<DataT>::SetupFunc   setup,
            typename RenderTask<DataT>::ExecuteFunc exec)
        {
            auto task = std::make_unique<RenderTask<DataT>>(name, std::move(setup), std::move(exec));
            auto* t = task.get();
            t->id = static_cast<TaskId>(tasks_.size());
            tasks_.push_back(std::move(task));

            // 立即调用 setup 构建资源依赖
            RenderTaskBuilder builder(*this, t);
            t->setup(t->data, builder);
            return t;
        }

        // 编译：拓扑排序 + 资源生命周期分析
        void compile();

        // 执行：按 timeline 顺序 realize -> execute -> derealize
        void execute();

    private:
        friend class RenderTaskBuilder;

        std::vector<std::unique_ptr<RenderTaskBase>> tasks_;
        std::vector<std::unique_ptr<ResourceBase>>   resources_;
        std::vector<TimelineStep>                    timeline_;
    };


}
