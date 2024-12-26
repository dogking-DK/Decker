#pragma once
#include <assimp/mesh.h>
#include <vsg/all.h>
#include <vector>

class MeshConverter
{
public:
    MeshConverter();
    ~MeshConverter();

    vsg::ref_ptr<vsg::Geometry> convertToVSGMesh(const aiMesh* mesh);

private:
    void extractMeshData(const aiMesh* mesh, std::vector<float>& vertices, std::vector<unsigned int>& indices);
    //void extractMeshMaterials(const aiMesh* mesh, vsg::ref_ptr<vsg::Material>& material);
};

