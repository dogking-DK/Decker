﻿#pragma once

#include <vk_types.h>

#include "Bound.h"
#include "Macros.h"
#include "Object.h"

namespace dk {
struct MeshAsset;

struct RenderObject
{
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance* material;
    Bounds bounds;
    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};


struct DrawContext
{
    std::vector<RenderObject> OpaqueSurfaces;
    std::vector<RenderObject> TransparentSurfaces;
};

// base class for a renderable dynamic object
class IRenderable : Object
{
    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct Node : IRenderable
{
    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children)
        {
            c->refreshTransform(worldTransform);
        }
    }

    void draw(const glm::mat4& topMatrix, DrawContext& ctx) override
    {
        // draw children
        for (auto& c : children)
        {
            c->draw(topMatrix, ctx);
        }
    }
};

struct MeshNode : Node
{
    std::shared_ptr<MeshAsset> mesh;

    void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

} // dk
