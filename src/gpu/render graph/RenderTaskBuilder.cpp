#include "RenderTaskBuilder.h"
#include "RenderGraph.h"
#include "Resource.h"

namespace dk {

    // ---------------------------------------------
    // RenderTaskBuilder 模板方法实现
    // （必须放在 RenderGraph 定义后）
    // ---------------------------------------------
    template<typename ResT>
    ResT* RenderTaskBuilder::create(const std::string& name,
        const typename ResT::Desc& desc,
        ResourceLifetime life)
    {
        // ResT 必须继承自 ResourceBase
        static_assert(std::is_base_of<ResourceBase, ResT>::value,
            "ResT must derive from ResourceBase");

        auto res = std::make_unique<ResT>(name, desc, life);
        auto* ptr = res.get();
        ptr->id = static_cast<ResourceId>(graph.resources_.size());
        ptr->creator = task;
        graph.resources_.push_back(std::move(res));

        task->writes.push_back(ptr);
        return ptr;
    }

    template<typename ResT>
    ResT* RenderTaskBuilder::read(ResT* res) {
        static_assert(std::is_base_of<ResourceBase, ResT>::value,
            "ResT must derive from ResourceBase");
        if (!res) return nullptr;
        task->reads.push_back(res);
        res->readers.push_back(task);
        return res;
    }

    template<typename ResT>
    ResT* RenderTaskBuilder::write(ResT* res) {
        static_assert(std::is_base_of<ResourceBase, ResT>::value,
            "ResT must derive from ResourceBase");
        if (!res) return nullptr;
        task->writes.push_back(res);
        res->writers.push_back(task);
        return res;
    }

}
