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

    // ����GLTF�ļ�
    bool loadGLTF(const std::string& file_path);

    // ����Assimp�ĳ�����������ģ����Դ�����ȡ����
    const aiScene* getScene() const { return _scene; }

private:
    const aiScene* _scene;
    Assimp::Importer _importer;
};