from __future__ import annotations

import subprocess
from pathlib import Path


def build_dot() -> str:
    return r"""
digraph RenderGraphInitFlow {
    rankdir=TB;
    compound=true;
    fontsize=12;
    fontname="Consolas";
    labelloc="t";
    label="Decker RenderGraph Init + Execute Flow";

    node [
        shape=box,
        style="rounded,filled",
        fillcolor="#F7FAFF",
        color="#6A7A99",
        fontname="Consolas",
        fontsize=10
    ];

    edge [
        color="#5E6A82",
        arrowsize=0.7
    ];

    subgraph cluster_startup {
        label="Startup / Build Graph";
        color="#D8E2F0";
        style="rounded";

        A [label="VulkanEngine::init()"];
        B [label="init_renderables()"];
        C [label="RenderSystem ctor + init()"];
        D [label="resizeRenderTargets(extent)"];
        E [label="create _color_target / _depth_target"];
        F [label="RenderSystem::buildGraph()"];
        G [label="reset graph state\n(_graph, flags, named resources)"];

        H [label="buildGraphFromAsset()"];
        I [label="load assets/config/render_graph.json"];
        J [label="GraphAssetIO parse\n(resources/nodes/edges)"];
        K [label="compileGraphAsset\n(validate + topo + inputPins)"];
        L [label="addAssetImageResourcesTask()\ncreate/bind image resources"];
        M [label="iterate compiled nodes\nresolve color_in/depth_in\nregister pass tasks"];
        N [label="_graph.compile();\n_graph_built=true;\n_compiled=true"];

        O [label="buildGraphLegacy()"];
        P [label="addRenderTargetResourcesTask + clear\nregister built-in passes\n_graph.compile()"];
    }

    subgraph cluster_frame {
        label="Per Frame Execute";
        color="#D8E2F0";
        style="rounded";

        Q [label="VulkanEngine::draw_main()\ncreate RenderGraphContext"];
        R [label="RenderSystem::execute(ctx)"];
        S [label="if !_graph_built => buildGraph()\nif !_compiled => _graph.compile()"];
        T [label="RenderGraph::execute(ctx)\nfor timeline:\nrealize -> task.execute -> derealize"];
    }

    A -> B -> C -> D -> E -> F -> G -> H;

    H -> I -> J -> K -> L -> M -> N;
    H -> O [label="asset load/compile failed"];
    O -> P;

    N -> Q;
    P -> Q;
    Q -> R -> S -> T;
}
""".lstrip()


def main() -> int:
    repo_root = Path(__file__).resolve().parents[1]
    out_dir = repo_root / "assets" / "config"
    out_dir.mkdir(parents=True, exist_ok=True)

    dot_path = out_dir / "render_graph_init_flow.dot"
    svg_path = out_dir / "render_graph_init_flow.svg"

    dot_path.write_text(build_dot(), encoding="utf-8")
    print(f"[ok] wrote DOT: {dot_path}")

    dot_cmd = ["dot", "-Tsvg", str(dot_path), "-o", str(svg_path)]
    result = subprocess.run(dot_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("[warn] failed to render SVG with graphviz dot")
        if result.stderr:
            print(result.stderr.strip())
        return result.returncode

    print(f"[ok] wrote SVG: {svg_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

