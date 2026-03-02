#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <array>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "gpu/render graph/BuiltinPassRegistry.h"
#include "gpu/render graph/GraphAssetBuilder.h"
#include "gpu/render graph/GraphAssetIO.h"

namespace py = pybind11;

namespace {

dk::ResourceLifetime parse_lifetime_or_throw(const std::string& text)
{
    if (text == "Transient")
    {
        return dk::ResourceLifetime::Transient;
    }
    if (text == "External")
    {
        return dk::ResourceLifetime::External;
    }
    if (text == "Persistent")
    {
        return dk::ResourceLifetime::Persistent;
    }

    throw std::invalid_argument("Invalid lifetime: " + text);
}

// Convert Python scalar/vector values into strongly-typed graph params.
dk::GraphParam make_param_from_python(const std::string& name, const py::handle& value)
{
    if (py::isinstance<py::bool_>(value))
    {
        return dk::GraphParam{.name = name, .type = dk::ParamType::Bool, .value = value.cast<bool>()};
    }
    if (py::isinstance<py::int_>(value))
    {
        return dk::GraphParam{.name = name, .type = dk::ParamType::Int, .value = value.cast<std::int32_t>()};
    }
    if (py::isinstance<py::float_>(value))
    {
        return dk::GraphParam{.name = name, .type = dk::ParamType::Float, .value = value.cast<float>()};
    }
    if (py::isinstance<py::str>(value))
    {
        return dk::GraphParam{.name = name, .type = dk::ParamType::String, .value = value.cast<std::string>()};
    }

    if (py::isinstance<py::sequence>(value))
    {
        const auto seq = py::reinterpret_borrow<py::sequence>(value);
        if (seq.size() == 2)
        {
            return dk::GraphParam{
                .name = name,
                .type = dk::ParamType::Vec2,
                .value = std::array<float, 2>{seq[0].cast<float>(), seq[1].cast<float>()},
            };
        }
        if (seq.size() == 3)
        {
            return dk::GraphParam{
                .name = name,
                .type = dk::ParamType::Vec3,
                .value = std::array<float, 3>{seq[0].cast<float>(), seq[1].cast<float>(), seq[2].cast<float>()},
            };
        }
        if (seq.size() == 4)
        {
            return dk::GraphParam{
                .name = name,
                .type = dk::ParamType::Vec4,
                .value = std::array<float, 4>{seq[0].cast<float>(), seq[1].cast<float>(), seq[2].cast<float>(), seq[3].cast<float>()},
            };
        }
    }

    throw std::invalid_argument("Unsupported param type for \"" + name + "\"");
}

}

PYBIND11_MODULE(decker_render_graph, m)
{
    m.doc() = "RenderGraph construction and validation bindings";

    // Builder-oriented API: create resources/nodes/edges then validate or save.
    py::class_<dk::GraphAssetBuilder>(m, "GraphAssetBuilder")
        .def(py::init<std::string, std::uint32_t>(),
             py::arg("name") = "graph",
             py::arg("schema_version") = 1)
        .def(
            "add_image_resource",
            [](dk::GraphAssetBuilder& builder,
               std::string name,
               std::uint32_t width,
               std::uint32_t height,
               std::uint32_t depth,
               std::uint32_t mip_levels,
               std::uint32_t array_layers,
               std::uint32_t format,
               std::uint32_t usage,
               std::uint32_t samples,
               std::uint32_t aspect_mask,
               std::string lifetime,
               std::string external_key)
            {
                dk::GraphImageDesc desc{};
                desc.width = width;
                desc.height = height;
                desc.depth = depth;
                desc.mipLevels = mip_levels;
                desc.arrayLayers = array_layers;
                desc.format = format;
                desc.usage = usage;
                desc.samples = samples;
                desc.aspectMask = aspect_mask;

                const auto resource_lifetime = parse_lifetime_or_throw(lifetime);
                builder.addImageResource(std::move(name), desc, resource_lifetime, std::move(external_key));
            },
            py::arg("name"),
            py::arg("width") = 1,
            py::arg("height") = 1,
            py::arg("depth") = 1,
            py::arg("mip_levels") = 1,
            py::arg("array_layers") = 1,
            py::arg("format") = 0,
            py::arg("usage") = 0,
            py::arg("samples") = 1,
            py::arg("aspect_mask") = 0,
            py::arg("lifetime") = "Transient",
            py::arg("external_key") = "")
        .def(
            "add_buffer_resource",
            [](dk::GraphAssetBuilder& builder,
               std::string name,
               std::uint64_t size,
               std::uint32_t usage,
               std::string lifetime,
               std::string external_key)
            {
                dk::GraphBufferDesc desc{};
                desc.size = size;
                desc.usage = usage;
                const auto resource_lifetime = parse_lifetime_or_throw(lifetime);
                builder.addBufferResource(std::move(name), desc, resource_lifetime, std::move(external_key));
            },
            py::arg("name"),
            py::arg("size"),
            py::arg("usage") = 0,
            py::arg("lifetime") = "Transient",
            py::arg("external_key") = "")
        .def(
            "add_node",
            [](dk::GraphAssetBuilder& builder,
               std::string type,
               const py::dict& params,
               dk::GraphNodeId node_id)
            {
                std::vector<dk::GraphParam> graph_params;
                graph_params.reserve(params.size());

                for (const auto& item : params)
                {
                    graph_params.push_back(make_param_from_python(item.first.cast<std::string>(), item.second));
                }

                const auto& node = builder.addNode(std::move(type), std::move(graph_params), node_id);
                return node.id;
            },
            py::arg("type"),
            py::arg("params") = py::dict{},
            py::arg("node_id") = 0)
        .def(
            "add_edge",
            [](dk::GraphAssetBuilder& builder,
               dk::GraphNodeId from_node,
               std::string from_pin,
               dk::GraphNodeId to_node,
               std::string to_pin,
               dk::GraphEdgeId edge_id)
            {
                const auto& edge = builder.addEdge(
                    from_node,
                    std::move(from_pin),
                    to_node,
                    std::move(to_pin),
                    edge_id);
                return edge.id;
            },
            py::arg("from_node"),
            py::arg("from_pin"),
            py::arg("to_node"),
            py::arg("to_pin"),
            py::arg("edge_id") = 0)
        .def(
            "validate",
            [](const dk::GraphAssetBuilder& builder)
            {
                std::string error;
                const auto pass_registry = dk::createBuiltinRenderPassRegistry();
                const bool ok = builder.validate(error, &pass_registry);
                return py::make_tuple(ok, error);
            })
        .def(
            "to_json_text",
            [](const dk::GraphAssetBuilder& builder)
            {
                std::string json_text;
                std::string error;
                if (!dk::saveGraphAssetToJsonText(builder.asset(), json_text, error))
                {
                    throw std::runtime_error(error);
                }
                return json_text;
            })
        .def(
            "save_json",
            [](const dk::GraphAssetBuilder& builder, const std::string& path)
            {
                std::string error;
                const bool ok = dk::saveGraphAssetToJsonFile(path, builder.asset(), error);
                return py::make_tuple(ok, error);
            },
            py::arg("path"));
}
