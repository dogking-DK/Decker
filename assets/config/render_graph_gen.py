#!/usr/bin/env python3
"""
Generate render_graph.json from Python using decker_render_graph bindings.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate Decker render graph asset")
    parser.add_argument(
        "--out",
        default="assets/config/render_graph.json",
        help="Output render graph JSON path",
    )
    parser.add_argument(
        "--module-dir",
        default="",
        help="Optional directory that contains decker_render_graph module",
    )
    return parser.parse_args()


def build_default_graph(drg) -> object:
    builder = drg.GraphAssetBuilder(name="main_render_graph", schema_version=1)

    builder.add_image_resource(
        name="scene_color",
        width=1,
        height=1,
        depth=1,
        mip_levels=1,
        array_layers=1,
        format=0,
        usage=0,
        samples=1,
        aspect_mask=0,
        lifetime="External",
        external_key="scene_color",
    )
    builder.add_image_resource(
        name="scene_depth",
        width=1,
        height=1,
        depth=1,
        mip_levels=1,
        array_layers=1,
        format=0,
        usage=0,
        samples=1,
        aspect_mask=0,
        lifetime="External",
        external_key="scene_depth",
    )

    clear_id = builder.add_node(
        "ClearTargets",
        {
            "clear_color": [0.95, 0.96, 0.98, 1.0],
            "clear_depth": 1.0,
            "clear_stencil": 0,
        },
        node_id=1,
    )
    opaque_id = builder.add_node("OpaquePass", node_id=2)
    outline_id = builder.add_node("OutlinePass", node_id=3)
    debug_aabb_id = builder.add_node("DebugAabbPass", node_id=4)
    gizmo_id = builder.add_node("UiGizmoPass", node_id=5)
    fluid_id = builder.add_node("FluidVolumePass", node_id=6)
    voxel_id = builder.add_node("VoxelPass", node_id=7)

    builder.add_edge(clear_id, "color_out", opaque_id, "color_in", edge_id=1)
    builder.add_edge(opaque_id, "color_out", outline_id, "color_in", edge_id=2)
    builder.add_edge(outline_id, "color_out", debug_aabb_id, "color_in", edge_id=3)
    builder.add_edge(debug_aabb_id, "color_out", gizmo_id, "color_in", edge_id=4)
    builder.add_edge(gizmo_id, "color_out", fluid_id, "color_in", edge_id=5)
    builder.add_edge(fluid_id, "color_out", voxel_id, "color_in", edge_id=6)

    builder.add_edge(clear_id, "depth_out", opaque_id, "depth_in", edge_id=7)
    builder.add_edge(opaque_id, "depth_out", outline_id, "depth_in", edge_id=8)
    builder.add_edge(outline_id, "depth_out", debug_aabb_id, "depth_in", edge_id=9)
    builder.add_edge(debug_aabb_id, "depth_out", gizmo_id, "depth_in", edge_id=10)
    builder.add_edge(gizmo_id, "depth_out", fluid_id, "depth_in", edge_id=11)
    builder.add_edge(fluid_id, "depth_out", voxel_id, "depth_in", edge_id=12)

    return builder


def build_default_graph_dict() -> dict:
    return {
        "schema_version": 1,
        "name": "main_render_graph",
        "resources": [
            {
                "name": "scene_color",
                "kind": "Image",
                "lifetime": "External",
                "external_key": "scene_color",
                "desc": {
                    "width": 1,
                    "height": 1,
                    "depth": 1,
                    "mip_levels": 1,
                    "array_layers": 1,
                    "format": 0,
                    "usage": 0,
                    "samples": 1,
                    "aspect_mask": 0,
                },
            },
            {
                "name": "scene_depth",
                "kind": "Image",
                "lifetime": "External",
                "external_key": "scene_depth",
                "desc": {
                    "width": 1,
                    "height": 1,
                    "depth": 1,
                    "mip_levels": 1,
                    "array_layers": 1,
                    "format": 0,
                    "usage": 0,
                    "samples": 1,
                    "aspect_mask": 0,
                },
            },
        ],
        "nodes": [
            {
                "id": 1,
                "type": "ClearTargets",
                "params": [
                    {"name": "clear_color", "type": "Vec4", "value": [0.95, 0.96, 0.98, 1.0]},
                    {"name": "clear_depth", "type": "Float", "value": 1.0},
                    {"name": "clear_stencil", "type": "Int", "value": 0},
                ],
            },
            {"id": 2, "type": "OpaquePass", "params": []},
            {"id": 3, "type": "OutlinePass", "params": []},
            {"id": 4, "type": "DebugAabbPass", "params": []},
            {"id": 5, "type": "UiGizmoPass", "params": []},
            {"id": 6, "type": "FluidVolumePass", "params": []},
            {"id": 7, "type": "VoxelPass", "params": []},
        ],
        "edges": [
            {"id": 1, "from_node": 1, "from_pin": "color_out", "to_node": 2, "to_pin": "color_in"},
            {"id": 2, "from_node": 2, "from_pin": "color_out", "to_node": 3, "to_pin": "color_in"},
            {"id": 3, "from_node": 3, "from_pin": "color_out", "to_node": 4, "to_pin": "color_in"},
            {"id": 4, "from_node": 4, "from_pin": "color_out", "to_node": 5, "to_pin": "color_in"},
            {"id": 5, "from_node": 5, "from_pin": "color_out", "to_node": 6, "to_pin": "color_in"},
            {"id": 6, "from_node": 6, "from_pin": "color_out", "to_node": 7, "to_pin": "color_in"},
            {"id": 7, "from_node": 1, "from_pin": "depth_out", "to_node": 2, "to_pin": "depth_in"},
            {"id": 8, "from_node": 2, "from_pin": "depth_out", "to_node": 3, "to_pin": "depth_in"},
            {"id": 9, "from_node": 3, "from_pin": "depth_out", "to_node": 4, "to_pin": "depth_in"},
            {"id": 10, "from_node": 4, "from_pin": "depth_out", "to_node": 5, "to_pin": "depth_in"},
            {"id": 11, "from_node": 5, "from_pin": "depth_out", "to_node": 6, "to_pin": "depth_in"},
            {"id": 12, "from_node": 6, "from_pin": "depth_out", "to_node": 7, "to_pin": "depth_in"},
        ],
    }


def main() -> int:
    args = parse_args()

    if args.module_dir:
        sys.path.insert(0, str(Path(args.module_dir).resolve()))

    drg = None
    try:
        import decker_render_graph as drg
    except Exception as exc:  # noqa: BLE001
        print(f"[render_graph_gen] import decker_render_graph failed, fallback to pure python: {exc}")

    out_path = Path(args.out)
    if not out_path.is_absolute():
        out_path = Path.cwd() / out_path
    out_path.parent.mkdir(parents=True, exist_ok=True)

    if drg:
        builder = build_default_graph(drg)
        ok, error = builder.validate()
        if not ok:
            print(f"[render_graph_gen] graph validate failed: {error}", file=sys.stderr)
            return 1

        ok, error = builder.save_json(str(out_path))
        if not ok:
            print(f"[render_graph_gen] save json failed: {error}", file=sys.stderr)
            return 1
    else:
        graph = build_default_graph_dict()
        out_path.write_text(json.dumps(graph, indent=2), encoding="utf-8")

    print(f"[render_graph_gen] generated: {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
