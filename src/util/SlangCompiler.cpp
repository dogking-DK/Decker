//// SlangCompiler.cpp
//#include "SlangCompiler.h"
//#include <cstring>
//#include <iostream>
//
//namespace dk {
//static inline void SLANG_THROW_IF_FAILED(SlangResult r, const char* what)
//{
//    if (SLANG_FAILED(r)) throw std::runtime_error(what);
//}
//
//SlangStage SlangCompiler::toSlangStage(ShaderStage s)
//{
//    switch (s)
//    {
//    case ShaderStage::Vertex: return SLANG_STAGE_VERTEX;
//    case ShaderStage::Fragment: return SLANG_STAGE_FRAGMENT;
//    case ShaderStage::Compute: return SLANG_STAGE_COMPUTE;
//    case ShaderStage::Geometry: return SLANG_STAGE_GEOMETRY;
//    case ShaderStage::TessControl: return SLANG_STAGE_HULL;    // == TCS
//    case ShaderStage::TessEval: return SLANG_STAGE_DOMAIN;  // == TES
//    }
//    return SLANG_STAGE_NONE;
//}
//
//SlangCompiler::SlangCompiler()
//{
//    // 1) 创建全局会话（C++ 接口）
//    SLANG_THROW_IF_FAILED(createGlobalSession(m_global.writeRef()),
//                          "createGlobalSession() failed");
//}
//
//void SlangCompiler::addSearchPath(std::string path) { m_searchPaths.emplace_back(std::move(path)); }
//void SlangCompiler::addDefine(std::string k, std::string v) { m_defines.emplace_back(std::move(k), std::move(v)); }
//
//CompileArtifacts SlangCompiler::compileToSpirv(
//    const SlangSource&                 src,
//    const std::vector<SlangSource>&    extraHeaders,
//    const std::vector<EntryPointDesc>& entryPoints,
//    SlangSourceLanguage                language,
//    const char*                        spirvProfile
//)
//{
//    CompileArtifacts artifacts;
//
//    // 2) 为当前编译创建会话（可按需重用）
//    Slang::ComPtr<slang::ISession> session;
//    {
//        slang::SessionDesc sdesc = {};
//        // 也可以在 sdesc 里设置预编译 stdlib / caps 等，这里保持最小化
//        SLANG_THROW_IF_FAILED(m_global->createSession(sdesc, session.writeRef()),
//                              "IGlobalSession::createSession() failed");
//    }
//
//    // 3) 创建 compile request
//    Slang::ComPtr<slang::ICompileRequest> req;
//    SLANG_THROW_IF_FAILED(session->createCompileRequest(req.writeRef()),
//                          "ISession::createCompileRequest() failed");
//
//    // 目标后端：SPIR-V
//    int targetIndex = req->addCodeGenTarget(SLANG_SPIRV);
//    if (spirvProfile && spirvProfile[0])
//    {
//        auto prof = session->findProfile(spirvProfile);
//        req->setTargetProfile(targetIndex, prof);
//    }
//
//    // 一些建议的开关
//    req->setDebugInfoLevel(SLANG_DEBUG_INFO_LEVEL_STANDARD);
//    req->setGenerateWholeProgram(true);
//
//    // 搜索路径与宏
//    for (auto& p : m_searchPaths) req->addSearchPath(p.c_str());
//    for (auto& kv : m_defines) req->addPreprocessorDefine(kv.first.c_str(), kv.second.c_str());
//
//    // 加一个 translation unit
//    const char* tuName = src.virtualName.empty() ? "module" : src.virtualName.c_str();
//    int         tu     = req->addTranslationUnit(language, tuName);
//
//    if (!src.path.empty())
//    {
//        req->addSourceFile(tu, src.path.c_str());
//    }
//    else
//    {
//        const char* vname = src.virtualName.empty() ? "in_memory" : src.virtualName.c_str();
//        req->addSourceString(tu, vname, src.code.c_str());
//    }
//    for (size_t i = 0; i < extraHeaders.size(); ++i)
//    {
//        if (!extraHeaders[i].path.empty())
//        {
//            req->addSourceFile(tu, extraHeaders[i].path.c_str());
//        }
//        else
//        {
//            std::string name = extraHeaders[i].virtualName.empty()
//                                   ? ("header_" + std::to_string(i))
//                                   : extraHeaders[i].virtualName;
//            req->addSourceString(tu, name.c_str(), extraHeaders[i].code.c_str());
//        }
//    }
//
//    // 指定 entry points
//    std::vector<int> epIndices(entryPoints.size());
//    for (size_t i = 0; i < entryPoints.size(); ++i)
//    {
//        epIndices[i] = req->addEntryPoint(
//            tu,
//            entryPoints[i].name.c_str(),
//            toSlangStage(entryPoints[i].stage)
//        );
//    }
//
//    // 编译
//    {
//        SlangResult r     = req->compile();
//        const char* diags = req->getDiagnosticOutput();
//        if (diags && std::strlen(diags))
//        {
//            std::cerr << "[Slang diagnostics]\n" << diags << std::endl;
//        }
//        SLANG_THROW_IF_FAILED(r, "Slang compile failed");
//    }
//
//    // 取每个 entry 的 SPIR-V
//    artifacts.entrySpirvs.resize(entryPoints.size());
//    for (size_t i = 0; i < entryPoints.size(); ++i)
//    {
//        size_t      size = 0;
//        const void* code = req->getEntryPointCode(epIndices[i], targetIndex, &size);
//        if (!code || size == 0) throw std::runtime_error("getEntryPointCode() returned empty blob");
//
//        auto& dst = artifacts.entrySpirvs[i].words;
//        dst.resize(size / sizeof(uint32_t));
//        std::memcpy(dst.data(), code, size);
//    }
//
//    // 反射根对象（C++ 接口）：仅在 req 生命周期内有效
//    artifacts.programLayout = req->getProgramLayout();
//    // 如果需要在外部长期使用，建议立刻把需要的数据抄到你自己的结构里
//
//    return artifacts; // ComPtr 在栈上，req 析构会使 programLayout 失效；若要用反射，请把解析放在此函数内部完成
//}
//}
