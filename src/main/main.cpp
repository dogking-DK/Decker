#include <iostream>
#include <vk_engine.h>
#include <vk_mem_alloc.h>

#include "render graph/RenderGraph.h"
#include "render graph/RenderTaskBuilder.h"
#include "render graph/Resource.h"
#include "render graph/ResourceTexture.h"

int main(int argc, char* argv[])
{
    dk::VulkanEngine engine;

    engine.init();

    engine.run();

    engine.cleanup();

    return 0;
}
