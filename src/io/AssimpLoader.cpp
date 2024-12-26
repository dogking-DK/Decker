#include "AssimpLoader.h"

#include <iostream>
#include <fmt/core.h>

AssimpLoader::AssimpLoader() : _scene(nullptr) {}

AssimpLoader::~AssimpLoader() {}

bool AssimpLoader::loadGLTF(const std::string& file_path)
{

    _scene = _importer.ReadFile(file_path, aiProcess_Triangulate | aiProcess_FlipUVs);
    if (!_scene) 
    {
        fmt::print(stderr, "Failed to load GLTF file{}\n", file_path);
        return false;
    }
    return true;
}
