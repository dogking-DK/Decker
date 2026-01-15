#pragma once
#include <vector>
#include <cstdint>

#include "UUID.hpp"

namespace dk {
enum VertexAttribute : uint16_t
{
    Position  = 0,
    Normal    = 1,
    Tangent   = 2,
    Texcoord0 = 16,
    Color0    = 32,
    Joints0   = 48,
    Weights0  = 64,
};

constexpr VertexAttribute texCoordN(const int n) { return static_cast<VertexAttribute>(16 + n); }
constexpr VertexAttribute colorN(const int n) { return static_cast<VertexAttribute>(32 + n); }
constexpr VertexAttribute jointsN(const int n) { return static_cast<VertexAttribute>(48 + n); }
constexpr VertexAttribute weightsN(const int n) { return static_cast<VertexAttribute>(64 + n); }

struct RawMeshHeader
{
    uint32_t vertex_count = 0;
    uint32_t index_count  = 0;
    uint16_t attr_count   = 0;
    uint16_t reserved     = 0;
};



enum class PixelDataType : uint8_t
{
    UBYTE,                // unsigned byte
    BYTE,                 // signed byte
    USHORT,               // unsigned short (16-bit)
    SHORT,                // signed short (16-bit)
    UINT,                 // unsigned int (32-bit)
    INT,                  // signed int (32-bit)
    HALF,                 // half-float (16-bit float)
    FLOAT,                // float (32-bits float)
};

struct AttrDesc
{
    VertexAttribute semantic;      // VertexSemantic
    uint8_t  elem_count;     // 1–4
    uint8_t  comp_type;      // 0=float32, 1=uint16, 2=uint8_norm …
    uint32_t offset_bytes;   // 相对文件起始
    uint32_t byte_size;      // 属性数据大小
};

struct RawMeshData
{
    RawMeshHeader          header;
    std::vector<AttrDesc>  table;
    std::vector<std::byte> blob;     // 仅在转换时使用
    std::vector<uint32_t>  indices;
};

struct RawImageHeader
{
    int32_t       width{1}, height{1}, depth{1}, channels{4};    // channels = 4 (RGBA8) after importer
    PixelDataType comp_type;
};

struct RawMaterial
{
    float                  metallic_factor  = 0.0f;
    float                  roughness_factor = 1.0f;
    std::array<uint8_t, 4> base_color       = {255, 255, 255, 255};  // sRGBA
    UUID                   base_color_texture{};          // 无则 invalid()
    UUID                   metal_rough_texture{};         // 无则 invalid()
    UUID                   normal_texture{};              // 无则 invalid()
    UUID                   occlusion_texture{};           // 无则 invalid()
    UUID                   emissive_texture{};            // 无则 invalid()
};
}
