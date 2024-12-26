// assimp_loader.h
#pragma once
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vsg/all.h>

class AssimpLoader
{
public:
    AssimpLoader();
    ~AssimpLoader();

    // 加载GLTF文件
    bool loadGLTF(const std::string& file_path);

    // 返回Assimp的场景对象，其他模块可以从中提取数据
    const aiScene* getScene() const { return _scene; }

private:
    const aiScene* _scene;
    Assimp::Importer _importer;
};