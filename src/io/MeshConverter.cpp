#include "MeshConverter.h"

MeshConverter::MeshConverter() {}

MeshConverter::~MeshConverter() {}

vsg::ref_ptr<vsg::Geometry> MeshConverter::convertToVSGMesh(const aiMesh* mesh)
{
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    extractMeshData(mesh, vertices, indices);

    vsg::ref_ptr<vsg::Geometry> geometry = vsg::Geometry::create();

    // Setup Vertex Buffer and Index Buffer
    geometry->assignArrays({});
    //geometry->setVertexData(vsg::Data::create(vertices));
    //geometry->setIndexData(vsg::Data::create(indices));

    // Setup material (if any)
    //vsg::ref_ptr<vsg::Material> material;
    //extractMeshMaterials(mesh, material);
    //geometry->setMaterial(material);

    return geometry;
}

void MeshConverter::extractMeshData(const aiMesh* mesh, std::vector<float>& vertices, std::vector<unsigned int>& indices)
{
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        vertices.push_back(mesh->mVertices[i].x);
        vertices.push_back(mesh->mVertices[i].y);
        vertices.push_back(mesh->mVertices[i].z);
        vertices.push_back(mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i].x : 0.0f);
        vertices.push_back(mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i].y : 0.0f);
        vertices.push_back(mesh->mNormals[i].x);
        vertices.push_back(mesh->mNormals[i].y);
        vertices.push_back(mesh->mNormals[i].z);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
    {
        const aiFace& face = mesh->mFaces[i];
        indices.push_back(face.mIndices[0]);
        indices.push_back(face.mIndices[1]);
        indices.push_back(face.mIndices[2]);
    }
}

//void MeshConverter::extractMeshMaterials(const aiMesh* mesh, vsg::ref_ptr<vsg::Material>& material)
//{
//    // For simplicity, assume that all meshes share the same material
//    if (mesh->mMaterialIndex >= 0) {
//        // Add logic to extract material from Assimp and create VSG material
//    }
//}
