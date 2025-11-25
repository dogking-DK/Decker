#include "RenderGraph.h"
#include "Resource.h"
#include "RenderTask.h"

namespace dk {
void RenderGraph::compile()
{
    const size_t N = tasks_.size();
    timeline_.clear();
    if (N == 0) return;

    // 1. 根据资源读写关系构建任务依赖图
    std::vector<std::vector<TaskId>> adj(N);
    std::vector<int>                 indegree(N, 0);

    auto addEdge = [&](TaskId from, TaskId to)
    {
        if (from == to) return;
        adj[from].push_back(to);
        indegree[to]++;
    };

    for (auto& resPtr : resources_)
    {
        auto* res = resPtr.get();
        // 以 writers 为主：写写、写读 建依赖
        std::vector<TaskId> writers;
        writers.reserve(res->writers.size());
        for (auto* w : res->writers)
        {
            writers.push_back(w->id);
        }
        std::ranges::sort(writers);
        writers.erase(std::ranges::unique(writers).begin(), writers.end());

        // 写写链：w0 -> w1 -> w2 ...
        for (size_t i = 1; i < writers.size(); ++i)
        {
            addEdge(writers[i - 1], writers[i]);
        }

        // 写读：每个 writer -> 所有 reader
        for (TaskId w : writers)
        {
            for (auto* r : res->readers)
            {
                addEdge(w, r->id);
            }
        }
    }

    // 2. Kahn 拓扑排序
    std::queue<TaskId> q;
    for (TaskId i = 0; i < static_cast<TaskId>(N); ++i)
    {
        if (indegree[i] == 0) q.push(i);
    }

    std::vector<TaskId> order;
    order.reserve(N);
    while (!q.empty())
    {
        TaskId u = q.front();
        q.pop();
        order.push_back(u);
        for (TaskId v : adj[u])
        {
            if (--indegree[v] == 0) q.push(v);
        }
    }

    if (order.size() != N)
    {
        std::cerr << "[RG] ERROR: cycle detected in task graph!\n";
        // 简单处理：仍然继续，但执行顺序可能错误
    }

    // 3. 计算每个 task 在拓扑序中的 index
    std::vector<int> topoIndex(N, -1);
    for (size_t i = 0; i < order.size(); ++i)
    {
        topoIndex[order[i]] = static_cast<int>(i);
    }

    // 4. 计算资源的 firstUse / lastUse
    for (auto& resPtr : resources_)
    {
        auto* res   = resPtr.get();
        int   first = INT_MAX;
        int   last  = -1;

        auto considerTask = [&](RenderTaskBase* t)
        {
            if (!t) return;
            int idx = topoIndex[t->id];
            if (idx < 0) return;
            first = std::min(first, idx);
            last  = std::max(last, idx);
        };

        for (auto* w : res->writers) considerTask(w);
        for (auto* r : res->readers) considerTask(r);
        // creator 一般已经在 writers 中，这里可以不用再考虑

        if (last < 0)
        {
            // 没被使用：first/last 保持 -1，后面执行中忽略
            continue;
        }

        res->firstUse = first;
        res->lastUse  = last;
    }

    // 5. 构建 timeline：给每个时间点挂 task 和 realize/derealize
    timeline_.clear();
    timeline_.resize(order.size());
    for (size_t i = 0; i < order.size(); ++i)
    {
        TaskId tid        = order[i];
        timeline_[i].task = tasks_[tid].get();
    }

    for (auto& resPtr : resources_)
    {
        auto* res = resPtr.get();
        if (res->firstUse < 0) continue;
        timeline_[res->firstUse].toRealize.push_back(res);
        timeline_[res->lastUse].toDerealize.push_back(res);
    }

    // 打印拓扑序和资源生命周期，方便调试
    std::cout << "[RG] Compile done. Task order:\n";
    for (size_t i = 0; i < order.size(); ++i)
    {
        std::cout << "  [" << i << "] Task " << tasks_[order[i]]->name << " (id=" << order[i] << ")\n";
    }
    std::cout << "[RG] Resource lifetimes:\n";
    for (auto& resPtr : resources_)
    {
        auto* res = resPtr.get();
        if (res->firstUse < 0)
        {
            std::cout << "  Resource \"" << res->_name << "\" not used by any task\n";
        }
        else
        {
            std::cout << "  Resource \"" << res->_name << "\" : firstUse=" << res->firstUse
                << ", lastUse=" << res->lastUse << "\n";
        }
    }
}

void RenderGraph::execute(RenderGraphContext& ctx)
{
    std::cout << "[RG] Execute begin\n";
    for (size_t i = 0; i < timeline_.size(); ++i)
    {
        auto& step = timeline_[i];
        std::cout << "  [Step " << i << "]\n";

        for (auto* r : step.toRealize)
        {
            r->realize(ctx);
        }

        if (step.task)
        {
            std::cout << "    Execute task \"" << step.task->name << "\" (id=" << step.task->id << ")\n";
            step.task->execute(ctx);
        }

        for (auto* r : step.toDerealize)
        {
            r->derealize(ctx);
        }
    }
    std::cout << "[RG] Execute end\n";
}
}
