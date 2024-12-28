#pragma once
#include <assimp/mesh.h>
#include <vector>

class MeshConverter
{
public:
    MeshConverter();
    ~MeshConverter();
private:
    void extractMeshData(const aiMesh* mesh, std::vector<float>& vertices, std::vector<unsigned int>& indices);
    //void extractMeshMaterials(const aiMesh* mesh, vsg::ref_ptr<vsg::Material>& material);
};

