#include <vk_types.h>
#include "Macros.h"
#include "Object.h"


DECKER_START
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

        virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override
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

        virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
    };

    void MeshNode::draw(const glm::mat4& topMatrix, DrawContext& ctx)
    {
        glm::mat4 nodeMatrix = topMatrix * worldTransform;

        //for (auto& s : mesh->surfaces)
        //{
        //    RenderObject def;
        //    def.indexCount = s.count;
        //    def.firstIndex = s.startIndex;
        //    def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
        //    def.material = &s.material->data;
        //    def.bounds = s.bounds;
        //    def.transform = nodeMatrix;
        //    def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

        //    if (s.material->data.passType == MaterialPass::Transparent)
        //    {
        //        ctx.TransparentSurfaces.push_back(def);
        //    }
        //    else
        //    {
        //        ctx.OpaqueSurfaces.push_back(def);
        //    }
        //}

        // recurse down
        Node::draw(topMatrix, ctx);
    }

DECKER_END
