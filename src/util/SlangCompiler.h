#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <cassert>
#include <slang.h>              // vcpkg: "slang"
#include <slang-com-helper.h>   // 可选，方便 ComPtr；若无可不包含
#include <slang-com-ptr.h>

namespace dk {
enum class ShaderStage { Vertex, Fragment, Compute, Geometry, TessControl, TessEval };

struct SlangSource
{
    // 任选其一：给 path（从文件读）或给 (virtualName, code)（内存字符串）
    std::string path;
    std::string virtualName;
    std::string code;

    explicit SlangSource(std::string filePath)
        : path(std::move(filePath))
    {
    }

    SlangSource(std::string name, std::string inMemoryCode)
        : virtualName(std::move(name)), code(std::move(inMemoryCode))
    {
    }
};

struct EntryPointDesc
{
    std::string name;  // "vsMain"/"psMain"/"csMain" ...
    ShaderStage stage;
};

struct SpirvBlob
{
    std::vector<uint32_t> words;
};

struct CompileArtifacts
{
    std::vector<SpirvBlob> entrySpirvs;           // 与传入的 entry 次序一一对应
    slang::ProgramLayout*  programLayout = nullptr; // 仅在本次 compile 的 request 生命周期内有效
};

class SlangCompiler
{
public:
    SlangCompiler();             // 创建 IGlobalSession
    ~SlangCompiler() = default;  // ComPtr 自动析构

    // 可选：添加搜索路径与宏
    void addSearchPath(std::string path);
    void addDefine(std::string key, std::string value = "");

    // language: SLANG_SOURCE_LANGUAGE_SLANG / _HLSL / _GLSL
    CompileArtifacts compileToSpirv(
        const SlangSource&                 src,
        const std::vector<SlangSource>&    extraHeaders,
        const std::vector<EntryPointDesc>& entryPoints,
        SlangSourceLanguage                language,
        const char*                        spirvProfile = "spirv_1_5"
    );

private:
    Slang::ComPtr<slang::IGlobalSession>             m_global;              // 进程级可复用
    std::vector<std::string>                         m_searchPaths;
    std::vector<std::pair<std::string, std::string>> m_defines;

    static SlangStage toSlangStage(ShaderStage s);
};
}
