#pragma once
#include <memory>
#include <string>
#include <vector>

#include "Resource.h"


namespace dk {
struct RenderTaskBase;
class RenderGraph;
// ---------------------------------------------
// RenderTaskBuilder：建图辅助类
// ---------------------------------------------
class RenderTaskBuilder
{
public:
    RenderTaskBuilder(RenderGraph* g, RenderTaskBase* t)
        : _graph(g), _task(t)
    {
    }

    template <typename ResT>
    ResT* create(const std::string&         name,
                 const typename ResT::Desc& desc,
                 ResourceLifetime           life = ResourceLifetime::Transient);

    template <typename ResT>
    ResT* read(ResT* res, ResourceUsage usage = ResourceUsage::Sampled);

    template <typename ResT>
    ResT* write(ResT* res, ResourceUsage usage = ResourceUsage::StorageWrite);

    template <typename ResT>
    ResT* use(ResT* res, ResourceAccess access, ResourceUsage usage);

private:
    RenderGraph*    _graph;
    RenderTaskBase* _task;

    friend class RenderGraph;
};


}
