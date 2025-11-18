#include <iostream>
#include <vk_engine.h>

#include "render graph/RenderGraph.h"
#include "render graph/RenderTaskBuilder.h"
#include "render graph/Resource.h"
int main(int argc, char* argv[])
{
    using MyRes = dk::Resource<dk::DummyDesc, dk::DummyActual>;

    dk::RenderGraph graph;

    MyRes* res1 = nullptr;
    MyRes* res2 = nullptr;

    // Task A
    struct AData { MyRes* r1 = nullptr; };
    graph.addTask<AData>(
        "TaskA",
        // setup
        [&](AData& data, dk::RenderTaskBuilder& b) {
            auto* r1 = b.create<MyRes>("R1", dk::DummyDesc{ 1 });
            data.r1  = r1;
            res1     = r1;
        },
        // execute
        [&](const AData& data) {
            std::cout << "      [TaskA] running. R1=" << data.r1->get() << "\n";
        }
    );

    // Task B
    struct BData { MyRes* r1 = nullptr; MyRes* r2 = nullptr; };
    graph.addTask<BData>(
        "TaskB",
        [&](BData& data, dk::RenderTaskBuilder& b) {
            data.r1  = b.read<MyRes>(res1);
            auto* r2 = b.create<MyRes>("R2", dk::DummyDesc{ 2 });
            data.r2  = r2;
            res2     = r2;
        },
        [&](const BData& data) {
            std::cout << "      [TaskB] running. R1=" << data.r1->get()
                << ", R2=" << data.r2->get() << "\n";
        }
    );

    // Task C
    struct CData { MyRes* r2 = nullptr; };
    graph.addTask<CData>(
        "TaskC",
        [&](CData& data, dk::RenderTaskBuilder& b) {
            data.r2 = b.read<MyRes>(res2);
        },
        [&](const CData& data) {
            std::cout << "      [TaskC] running. R2=" << data.r2->get() << "\n";
        }
    );

    dk::RenderGraphContext ctx;

    // 编译 + 执行
    graph.compile();
    graph.execute(ctx);

    return 0;

    dk::VulkanEngine engine;

    engine.init();

    engine.run();

    engine.cleanup();

    return 0;
}
