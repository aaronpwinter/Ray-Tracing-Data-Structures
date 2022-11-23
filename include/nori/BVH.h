//
// Created by Aaron Winter on 11/23/22.
//

#pragma once

///The upper bound for triangles in a node that stops the node from subdividing.
#define FEW_TRIS 10
#define MAX_DEPTH 10


#include "nori/AccelTree.h"

NORI_NAMESPACE_BEGIN

    class BVH: public AccelTree
    {
    public:

        /// A node for the Octree, which contains 8 children, stores its own AABB,
        ///     and a vector of triangle indices.
        struct Node
        {
            Node(BoundingBox3f bb, std::vector<TriInd>* triangles)
            {
                for (int i = 0; i < 8; ++i)
                {
                    children[i] = nullptr;
                }
                AABB = bb;
                tris = triangles;
            }

            ~Node()
            {
                for (auto & c : children)
                {
                    delete c;
                }
                delete tris;

            }

            bool isLeaf() const
            {
                return tris != nullptr;
            }

            uint32_t nodeCount() const
            {
                uint32_t count = 1;
                for (auto & i : children)
                {
                    if (i != nullptr)
                    {
                        count += i->nodeCount();
                    }
                }

                return count;
            }

            uint32_t triCount() const
            {
                if(isLeaf() && tris != nullptr) return tris->size();

                uint32_t count = 0;
                for (auto & i : children)
                {
                    if (i != nullptr)
                    {
                        count += i->triCount();
                    }
                }

                return count;
            }

            Node* children[2];
            BoundingBox3f AABB;
            //The indices of the triangles in the mesh. If nullptr this is not a leaf node
            std::vector<TriInd>* tris;
        };

        /// A simple struct for holding a float and a Node, used for sorting.
        struct NodeComp
        {
            NodeComp(): dist(0), n(nullptr) {};
            NodeComp(float d, Node* n): dist(d), n(n) {};

            bool operator<(const NodeComp& right) const
            {
                return dist < right.dist;
            }

            float dist;
            Node* n;
        };

    public:
        ~BVH() override
        {
            delete root;
        }

        void build() override;

        TriInd rayIntersect(const Ray3f &ray_, Intersection &its, bool shadowRay) const override;

        /// Returns the bounding box for the child node at the specified index.
        /// \param bb The Bounding Box of the parent node
        /// \param index The index of this child within the parent node
        /// \return The bounding box of the child node
        static BoundingBox3f childBB(const BoundingBox3f& bb, uint index);

    private:
        Node* build(const BoundingBox3f& bb, std::vector<TriInd>* tris, int depth);

        bool triIntersects(const BoundingBox3f& bb, const TriInd& tri);

        /// Searches through all the triangles in a leaf node for the closest intersection, and
        ///     returns that triangle index. Returns -1 on no intersection
        /// \param n The LEAF node to look through.
        /// \param ray The ray
        /// \param its Intersection
        /// \param shadowRay If this is a shadow ray query
        /// \return TriInd of triangle in the meshes on intersection, or -1 on none.
        TriInd leafRayTriIntersect(Node* n, const Ray3f& ray_, Intersection &its, bool shadowRay) const;

        /// Searches through all the triangles in a node for the closest intersection, and
        ///     returns that triangle index. Returns -1 on no intersection
        /// \param n The node to look through.
        /// \param ray The ray
        /// \param its Intersection
        /// \param shadowRay If this is a shadow ray query
        /// \return TriInd of triangle in the meshes on intersection, or -1 on none.
        TriInd nodeCloseTriIntersect(Node* n, const Ray3f& ray_, Intersection &its, bool shadowRay) const;

    private:
        Node* root;

    };

NORI_NAMESPACE_END